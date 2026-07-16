# C++ 程序获取海拔高度实现计划

## Summary

为 ImGui_Altitude 项目实现跨平台（macOS + Android）海拔高度获取功能。采用独立 `LocationProvider` 抽象类 + 各平台子类的架构，由 `Backend` 单例持有。macOS 用 CoreLocation（`CLLocation.altitude` 直接是 MSL，无需修正）；Android 用 Kotlin `LocationManager` 经 JNI push 到 native，C++ 用 GeographicLib EGM96 把 WGS84 椭球高修正为 MSL。Frontend 做一个简易海拔显示 UI 端到端验证。定位为「连续更新 + 观察者回调」模式，Android 权限由 UI 按钮触发。

## Current State Analysis

基于 Phase 1 探索，现状如下：

- **Backend**（[Backend.h](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/Backend/Backend.h) / [Backend.cpp](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/Backend/Backend.cpp)）：空壳单例，`init()` 返回 true 无逻辑；`.cpp` 已预留 `#ifdef __ANDROID__ / #else / #endif` 平台骨架；头文件已引入 `<mutex>`/`<chrono>`/`nlohmann/json` 等基础设施。
- **Frontend**（[Frontend.cpp](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/Frontend/Frontend.cpp)）：`update()` 每帧调用，第 62 行有 `// TODO：绘制主界面` 接入点；`init()` 内已调 `Backend::Instance().init()`。
- **macOS 入口**（[mainMacDesktop.mm](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/mainMacDesktop.mm)）：Obj-C++，`NSApp run` 主线程 run loop 已具备（CoreLocation delegate 回调需主线程）；`Frontend::init(20.0f,1.33f)` 在 `AppViewController` 初始化时调用。
- **Android 入口**（[mainAndroid.cpp](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/mainAndroid.cpp)）：`native_app_glue`，全局 `static android_app* g_App`；已有 JNI 调 Kotlin 的成熟模式（`ShowSoftKeyboardInput` 第 266 行、`PollUnicodeChars` 第 299 行，固定四步 `GetEnv→Attach→GetObjectClass/GetMethodID/CallXxx→Detach`）；`Init()` 内 g_App 就绪后调 `Frontend::init(22.0f,3.0f)`。
- **xmake.lua**（[xmake.lua](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/xmake.lua)）：已 `add_requires("geographiclib 2.1.1", {shared=false})` 并 `add_packages("geographiclib")`；macOS 编 `*.mm` + 框架（AppKit/Metal/MetalKit/QuartzCore/GameController）；Android 编 `Backend/*.cpp` 为 shared 库；`after_build` 拷贝产物到 `android/app/libs/arm64-v8a/`。
- **Info.plist**（[assets/app/Info.plist](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/assets/app/Info.plist)）：无任何定位权限声明。
- **GeographicLib Geoid API**（`~/.xmake/packages/g/geographiclib/2.1.1/.../include/GeographicLib/Geoid.hpp`）：`Geoid(name, path, cubic, threadsafe)` 构造，数据文件 `<name>.pgm` 须在 `path` 目录；`ConvertHeight(lat,lon,h,ELLIPSOIDTOGEOID)` 返回 MSL 高（`ELLIPSOIDTOGEOID=-1`）；**xrepo 包不含 `.pgm` 数据文件**，需用 `geographiclib-get-geoids egm96-5` 工具下载（约 2MB）。
- **Android 模板**（`ThirdParty/imgui/examples/example_android_opengl3/android/`）：`MainActivity.kt` 继承 `NativeActivity`、`AndroidManifest.xml` 当前无定位权限。实际 `android/` 目录外部维护，模板仅作接入参考。

## Assumptions & Decisions（来自 Phase 2/3 确认）

1. **架构**：独立 `LocationProvider` 抽象基类 + 平台子类，`Backend` 持有 `std::unique_ptr<LocationProvider>`。
2. **Android 定位层**：Kotlin `LocationManager` 获取 WGS84 椭球高 → JNI push 到 native → C++ GeographicLib 修正为 MSL。
3. **macOS 接入**：新建独立 Obj-C++ 文件封装 CoreLocation（`mainMacDesktop.mm` 不直接持有 CLLocationManager）。
4. **UI 范围**：顺带在 `Frontend::update()` 做简易海拔显示 UI。
5. **更新模式**：连续更新 + 观察者回调（`startUpdates(cb)`/`stopUpdates()`，子类更新数据后触发 callback）。
6. **Android 数据流向**：Kotlin push via JNI（Kotlin `LocationListener.onLocationChanged` 调 native 方法 `nativeOnLocation`，需 `RegisterNatives` 注册）。
7. **权限时机**：UI「开始测量」按钮触发 `requestPermission()`（Android 运行时权限；macOS 顺带触发 `requestWhenInUseAuthorization`）。
8. **EGM96 数据分发**：数据文件放仓库 `assets/geoid/egm96-5.pgm`，Android 运行时从 assets 拷贝到内部存储目录后传给 Geoid 构造；macOS 不需要数据文件。
9. **线程安全**：基类用 `std::mutex` 保护 `lastKnown`/`status`/`callback`（macOS CoreLocation 回调在主线程，Android LocationListener 默认主线程，但 mutex 仍保留以防跨线程读取）。

## Proposed Changes

### 1. 新建 `Backend/LocationProvider.h`（抽象基类 + 数据结构）

定义跨平台接口与数据载体：

```cpp
#pragma once
#include <functional>
#include <memory>
#include <mutex>
#include <cstdint>

struct LocationData {
    double altitudeMSL = 0.0;        // 海拔（MSL，米）— UI 显示用
    double altitudeEllipsoid = 0.0;  // 椭球高（米）— Android 原始值
    double latitude = 0.0;
    double longitude = 0.0;
    double horizontalAccuracy = 0.0;
    double verticalAccuracy = 0.0;
    int64_t timestampMs = 0;
    bool valid = false;
};

enum class LocationStatus { Idle, Starting, Active, Denied, Error };

// 观察者回调：数据更新或状态变化时触发（子类在更新后调用 notifyCallback）
using LocationCallback = std::function<void(const LocationData&, LocationStatus)>;

class LocationProvider {
public:
    virtual ~LocationProvider() = default;
    // 工厂：按平台返回具体子类（实现见 LocationProvider.cpp）
    static std::unique_ptr<LocationProvider> Create();

    // —— 子类实现 ——
    virtual void startUpdates(LocationCallback cb) = 0;
    virtual void stopUpdates() = 0;
    virtual void requestPermission() = 0;   // 触发权限请求（UI 按钮调用）

    // —— 基类通用（线程安全）——
    LocationData lastKnown() const;          // Frontend 每帧读取
    LocationStatus status() const;

protected:
    // 子类更新数据/状态后调用，触发 callback 并缓存
    void updateAndNotify(const LocationData& d, LocationStatus s);

    mutable std::mutex m_mutex;
    LocationData m_last;
    LocationStatus m_status = LocationStatus::Idle;
    LocationCallback m_callback;
};
```

### 2. 新建 `Backend/LocationProvider.cpp`（工厂 + 通用实现）

```cpp
#include "LocationProvider.h"

std::unique_ptr<LocationProvider> LocationProvider::Create() {
#if defined(__APPLE__)
    extern std::unique_ptr<LocationProvider> CreateLocationProviderMac();
    return CreateLocationProviderMac();
#elif defined(__ANDROID__)
    extern std::unique_ptr<LocationProvider> CreateLocationProviderAndroid();
    return CreateLocationProviderAndroid();
#else
    return nullptr;
#endif
}

LocationData LocationProvider::lastKnown() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_last;
}
LocationStatus LocationProvider::status() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_status;
}
void LocationProvider::updateAndNotify(const LocationData& d, LocationStatus s) {
    LocationCallback cb;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_last = d; m_status = s; cb = m_callback;
    }
    if (cb) cb(d, s);
}
```

### 3. 新建 `Backend/LocationProviderMac.mm`（macOS CoreLocation 实现）

- `#import <CoreLocation/CoreLocation.h>`，定义 `CLLocationManagerDelegate` 的 ObjC 类，持有 `LocationProviderMac*` 反向指针。
- delegate `didUpdateLocations:`：取 `locations.lastObject`，**`CLLocation.altitude` 本身即 MSL**，直接填 `altitudeMSL`（`altitudeEllipsoid` 置 0，CoreLocation 不提供椭球高），填经纬度/精度/时间戳，调 `updateAndNotify(d, Active)`。
- delegate `locationManagerDidChangeAuthorization:`：根据 `authorizationStatus` 更新状态（`AuthorizedWhenInUse/Always`→可 `startUpdatingLocation`；`Denied`→`Denied`；`NotDetermined`→等待）。
- `startUpdates(cb)`：主线程创建 `CLLocationManager`（设 `delegate`、`desiredAccuracy=kCLLocationAccuracyBest`），缓存 cb，状态置 `Starting`；若已授权则 `startUpdatingLocation`，否则先 `requestWhenInUseAuthorization`。
- `requestPermission()`：`[m_mgr requestWhenInUseAuthorization]`。
- `stopUpdates()`：`[m_mgr stopUpdatingLocation]`。
- 暴露 `CreateLocationProviderMac()` 返回 `make_unique<LocationProviderMac>()`。
- 线程注意：`CLLocationManager` 必须主线程，`Frontend::update` 在 `drawInMTKView` 主线程调用，UI 按钮触发 `startUpdates`/`requestPermission` 天然在主线程；`NSApp run` 提供 run loop。

### 4. 新建 `Backend/LocationProviderAndroid.cpp`（Android：JNI 接收 + GeographicLib 修正）

- 持有 `static LocationProviderAndroid* g_Inst = nullptr;`（供 native 函数路由）。
- 持有 `std::unique_ptr<GeographicLib::Geoid> m_geoid;`，构造时初始化：
  - 用 `g_App->activity->assetManager` 打开 `geoid/egm96-5.pgm`，拷贝到 `g_App->activity->internalDataPath/geoid/egm96-5.pgm`（首次拷贝，已存在则跳过）。
  - `m_geoid = make_unique<Geoid>("egm96-5", <internalDataPath>/geoid, true, true)`（`threadsafe=true` 避免缓存竞争）。
- `startUpdates(cb)`：缓存 cb，状态 `Starting`；通过 JNI 调 Kotlin `startLocationUpdates()`（复用 `g_App->activity->vm/clazz` + `GetMethodID("startLocationUpdates","()V")`）。
- `requestPermission()`：JNI 调 Kotlin `requestLocationPermission()`（签名 `()V`）；Kotlin 在 `onRequestPermissionsResult` 回调 `nativeOnPermissionResult(Z)V`。
- JNI native 方法（C 函数，由 RegisterNatives 绑定到 MainActivity）：
  - `nativeOnLocation(JNIEnv*, jobject, jdouble lat, jdouble lon, jdouble alt, jdouble acc, jlong ts)`：调 `g_Inst->onLocationPushed(...)`。
  - `nativeOnPermissionResult(JNIEnv*, jobject, jboolean granted)`：调 `g_Inst->onPermissionResult(granted)`。
- `onLocationPushed(lat,lon,alt,acc,ts)`：`double msl = m_geoid->ConvertHeight(lat, lon, alt, GeographicLib::Geoid::ELLIPSOIDTOGEOID);` 填 `LocationData`（`altitudeMSL=msl`、`altitudeEllipsoid=alt`），调 `updateAndNotify(d, Active)`。
- `onPermissionResult(granted)`：授权则 `startUpdates` 续流程；否则 `updateAndNotify(last, Denied)`。
- `stopUpdates()`：JNI 调 Kotlin `stopLocationUpdates()`。
- `RegisterLocationNatives(android_app* app)`：用 `app->activity->vm` Attach 后，`GetObjectClass(app->activity->clazz)` 取 MainActivity 类，`RegisterNatives` 注册上述两个 native 方法（解耦 Kotlin 包名）。
- 暴露 `CreateLocationProviderAndroid()`（构造时设 `g_Inst=this`）。

### 5. 改 `Backend/Backend.h` / `Backend.cpp`

- `Backend.h`：`#include "LocationProvider.h"`，新增 `private: std::unique_ptr<LocationProvider> m_location;` 与 `public: LocationProvider& location();`（断言非空）。
- `Backend.cpp`：`init()` 中 `m_location = LocationProvider::Create();`（若非空）；`location()` 返回 `*m_location`。

### 6. 改 `mainAndroid.cpp`

- `Init()` 末尾（g_App 就绪、`Frontend::init` 即 `Backend::init` 已创建 provider 后）调用 `extern bool RegisterLocationNatives(android_app*);` `RegisterLocationNatives(g_App);`，将 native 方法绑定到 MainActivity 类。
- 无需改动 `g_App` 可见性（`RegisterLocationNatives` 由 `LocationProviderAndroid.cpp` 实现，参数传入 `g_App`）。

### 7. 改 `mainMacDesktop.mm`

- **无需大改**：`Frontend::init` → `Backend::init` 会自动 `Create()` 出 `LocationProviderMac`；CoreLocation 所需主线程 run loop 由 `NSApp run` 提供。仅需确认 `Backend::init` 在 `AppViewController` init 链路内被调用（已是现状）。

### 8. 改 `Frontend/Frontend.cpp`（简易海拔 UI）

在 `update()` 第 62 行 `// TODO：绘制主界面` 处插入：
- 读取 `Backend::Instance().location().lastKnown()` 与 `status()`。
- 显示：海拔 MSL 大字（米）、经纬度、椭球高（Android）、水平精度、状态文案（`Active`→「定位中」、`Denied`→「权限被拒绝」、`Starting`→「等待定位…」）。
- 「开始测量」按钮：点击调 `requestPermission()` + `startUpdates(cb)`（cb 内可记录日志，UI 靠每帧 `lastKnown()` 刷新，无需 cb 刷新 UI）。
- 「停止」按钮：`stopUpdates()`。
- 用现有字体/图标（FontAwesome6）。

### 9. 改 `assets/app/Info.plist`

添加定位权限描述（macOS 必需，否则系统直接拒绝）：
```xml
<key>NSLocationWhenInUseUsageDescription</key>
<string>用于获取当前海拔高度</string>
```

### 10. 改 `xmake.lua`

- macOS 分支 `add_frameworks` 追加 `"CoreLocation"`（LocationProviderMac.mm 需要）。
- `after_build` Android 分支：拷贝 `assets/geoid/egm96-5.pgm` → `android/app/src/main/assets/geoid/egm96-5.pgm`（若文件存在），保证运行时 AAssetManager 可读。

### 11. EGM96 数据文件准备

- 执行 `~/.xmake/packages/g/geographiclib/2.1.1/*/sbin/geographiclib-get-geoids egm96-5` 下载 `egm96-5.pgm` 到默认 `/usr/local/share/GeographicLib/geoids/`。
- 拷贝该文件到仓库 `assets/geoid/egm96-5.pgm`（纳入版本管理，供 Android 打包）。

### 12. Android Kotlin / Manifest（外部 android 工程，同步模板）

> 实际 `android/` 目录外部维护，以下为需在工程中落地的改动；仓库内 `ThirdParty/.../android/` 模板同步更新作为参考。

- `AndroidManifest.xml`：添加 `<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION"/>`。
- `MainActivity.kt`：
  - 声明 `external fun nativeOnLocation(lat:Double, lon:Double, alt:Double, acc:Double, ts:Long)`、`external fun nativeOnPermissionResult(granted:Boolean)`。
  - `requestLocationPermission()`：`ActivityCompat.requestPermissions(this, arrayOf(ACCESS_FINE_LOCATION), REQ)`，`onRequestPermissionsResult` 调 `nativeOnPermissionResult(granted)`。
  - `startLocationUpdates()`：`getSystemService(LocationManager)`，`requestLocationUpdates(GPS_PROVIDER, 1000L, 1f, listener)`，listener `onLocationChanged` 调 `nativeOnLocation(loc.latitude, loc.longitude, loc.altitude, loc.accuracy, loc.time)`。
  - `stopLocationUpdates()`：`removeUpdates(listener)`。

## Verification Steps

1. **EGM96 数据就绪**：`ls -la assets/geoid/egm96-5.pgm` 存在且约 2MB。
2. **macOS 构建运行**：`xmake f -m releasedbg && xmake`，`xmake run`。点击「开始测量」→ 系统弹定位授权 → 授权后 UI 显示海拔 MSL（与「地图/指南针」App 海拔对比误差应在 GPS 精度内，CoreLocation 无需修正）。
3. **macOS 拒绝授权路径**：系统设置关闭定位 → 重启 → 点击按钮 → UI 显示「权限被拒绝」。
4. **Android 构建**：`xmake f -p android -a arm64-v8a && xmake`，确认 `egm96-5.pgm` 已拷到 `android/app/src/main/assets/geoid/`，`.so` 已拷到 `libs/arm64-v8a/`。安装到真机。
5. **Android 运行**：点击「开始测量」→ 运行时权限弹窗 → 授权后 UI 显示海拔 MSL。**验证 EGM96 修正**：对比 `altitudeEllipsoid`（原始 WGS84）与 `altitudeMSL`（修正后）应有数十米级差异（geoid undulation），且 MSL 与已知海拔接近。
6. **Android 权限拒绝路径**：拒绝授权 → UI 显示「权限被拒绝」。
7. **日志**：macOS 已接 Logger；Android 入口当前未接 Logger，本计划不强制接入，但 `LocationProviderAndroid` 内关键路径（geoid 加载成功/失败、nativeOnLocation 触发）用 `__android_log_print`（`<android/log.h>`，已链接 `log`）输出，便于 logcat 调试。
8. **代码检查**：构建无 warning（`-Wall -Wextra` 已开），`compile_commands.json` 已自动生成。
