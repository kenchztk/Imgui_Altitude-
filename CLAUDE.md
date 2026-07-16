# CLAUDE.md

本文件为 AI 编码助手提供项目导航。开始任何任务前请先阅读。

## 项目概述

跨平台海拔高度测量应用。使用 C++20 + xmake 构建，Dear ImGui 为 UI 框架，按平台选择渲染后端。当前阶段一范围：**macOS + Android**（Windows 已注释保留）。

- macOS：Metal 渲染，CoreLocation 获取海拔（`CLLocation.altitude` 本身即 MSL，无需修正）
- Android：OpenGL ES 3 渲染，`LocationManager` 获取 WGS84 椭球高，经 GeographicLib EGM96 修正为 MSL

## 构建命令

```bash
# macOS（必须用 open 启动 bundle，不能用 xmake run，裸二进制无 bundle 上下文会导致 CoreLocation 静默失败）
xmake f -m releasedbg && xmake
open pkg/NativeApp.app

# Android（NDK 27，arm64-v8a）
xmake f -p android -a arm64-v8a && xmake
# 产物自动拷到 android/app/libs/arm64-v8a/，EGM96 数据拷到 android/app/src/main/assets/geoid/
```

xmake 全局配置中如设了代理（`xmake g --proxy=...`），下载依赖失败时可临时 `xmake g --proxy=""` 清空。

## 架构与关键文件

### 单例分层

- `Backend/Backend.{h,cpp}`：单例 `Backend`，`init()` 创建 `LocationProvider`，`location()` 访问。业务/平台数据层。
- `Frontend/Frontend.{h,cpp}`：单例 `Frontend`，`init(fontSize, scale)` 加载字体/主题并触发 `Backend::init()`；`update()` 每帧绘制 UI（含海拔显示）。UI 层。
- `StyleManager.{h,cc}`：ImGui 主题管理（Classic 风格）。

### 平台入口

- macOS `mainMacDesktop.mm`：Cocoa + Metal。`NSApp run` 主线程 run loop，`drawInMTKView` 每帧调 `Frontend::update()`。NSVisualEffectView 毛玻璃 + 透明 MTKView。空闲时动态降帧（5fps）。
- Android `mainAndroid.cpp`：`native_app_glue`，`android_main` 轮询事件并调 `MainLoopStep()` → `Frontend::update()`。已有 JNI 调 `MainActivity.kt` 的模式（`g_App->activity->vm/->clazz`，四步：GetEnv→Attach→GetObjectClass/GetMethodID/CallXxx→Detach）。

### 定位模块（核心业务）

- `Backend/LocationProvider.h`：跨平台抽象基类。`LocationData`（含 `altitudeMSL`/`altitudeEllipsoid`/经纬度/精度）、`LocationStatus`（Idle/Starting/Active/Denied/Error）、`LocationCallback`。接口：`startUpdates(cb)`/`stopUpdates()`/`requestPermission()`/`lastKnown()`/`status()`。基类用 `std::mutex` 保证线程安全，`updateAndNotify()` 锁外触发回调避免死锁。
- `Backend/LocationProvider.cpp`：平台工厂 `Create()`（`#if __APPLE__`→Mac，`#elif __ANDROID__`→Android，否则 nullptr）+ 通用实现。
- `Backend/LocationProviderMac.mm`：CoreLocation 实现。ObjC delegate 回调 `didUpdateLocations`/`locationManagerDidChangeAuthorization`。懒创建 `CLLocationManager`，`startUpdates` 内含权限请求逻辑（NotDetermined 时自动 `requestWhenInUseAuthorization`）。
- `Backend/LocationProviderAndroid.cpp`：`#ifdef __ANDROID__` 保护。接收 Kotlin 经 JNI push 的定位数据，用 `GeographicLib::Geoid::ConvertHeight(lat,lon,alt,ELLIPSOIDTOGEOID)` 修正为 MSL。运行时从 assets 拷贝 `egm96-5.pgm` 到 `internalDataPath/geoid/` 再构造 Geoid。`RegisterLocationNatives(android_app*)` 用 `RegisterNatives` 把 `nativeOnLocation`/`nativeOnPermissionResult` 绑定到 MainActivity（解耦 Kotlin 包名）。

### 工具与资源

- `utils/Logger.{h,cpp}`：spdlog 封装单例，控制台 + 轮转文件双 sink（5MB×5），异步。日志目录：macOS `~/Library/Logs/NativeApp/`，Android 用 `setenv("HOME", internalDataPath)` 引导到内部存储。直接用 `spdlog::info/warn/error` 或 `SPDLOG_xxx` 宏。
- `assets/app/Info.plist`：macOS bundle 配置，已含 `NSLocationWhenInUseUsageDescription`。
- `assets/geoid/egm96-5.pgm`：EGM96 geoid 数据（18MB），Android 运行时必需。
- `assets/fonts/`：中文字体（MapleMono-NF-CN）+ FontAwesome6 图标字体（C++ 数组形式 `fa_solid_900.cpp` 等）。

### 第三方依赖

Git submodule（`ThirdParty/`）：`imgui`、`spdlog`、`IconFontCppHeaders`、`stb`。
xrepo 包：`nlohmann_json v3.12.0`、`geographiclib 2.1.1`（静态）、`fmt 12.2.0`（header-only）。
spdlog 配置：`SPDLOG_FMT_EXTERNAL` + `FMT_HEADER_ONLY`（用外部 fmt）。

### Android 模板（外部维护，仓库内仅参考）

`ThirdParty/imgui/examples/example_android_opengl3/android/`：`MainActivity.kt`（继承 `NativeActivity`）+ `AndroidManifest.xml`。实际 `android/` 工程目录不在仓库内。已含定位权限声明与 `LocationManager` + JNI native 回调实现。

## 重要约定与陷阱

1. **macOS 必须用 `open pkg/NativeApp.app` 运行**：`xmake run` 启动裸二进制，无 bundle 上下文，CoreLocation 会静默丢弃授权请求（不弹窗不回调）。
2. **macOS 构建后必须重新签名 bundle**：xmake `after_build` 已加 `codesign --force --deep --sign -`。链接器自带的 ad-hoc 签名不绑定 Info.plist，会导致 TCC 无法识别 usage description。
3. **EGM96 数据文件不含在 xrepo 的 geographiclib 包内**：需用 `geographiclib-get-geoids -p <dir> egm96-5` 下载，已放入 `assets/geoid/egm96-5.pgm`。
4. **Android NDK 路径**：xmake.lua 中硬编码为 `27.0.12077973`，若实际安装版本不同需修改。
5. **平台宏**：macOS 用 `__APPLE__`/`TARGET_OS_OSX`，Android 用 `__ANDROID__`，Windows 用 `_WIN32`。
6. **代码风格**：不主动添加注释（除非逻辑复杂或用户要求）；关键逻辑加简明中文注释。回答与注释均用中文。
7. **构建检查**：macOS 开了 `-Wall -Wextra`，改动后应确保零警告。spdlog 带 `{}` 格式化时会有第三方库 deprecation 告警（fmt 兼容性），非代码问题。
