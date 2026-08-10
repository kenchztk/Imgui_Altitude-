# 用 emericg/EGM96 替代 GeographicLib 的评估与实施计划

## 一、结论（收益评估）

**有明确收益，建议替换。** 核心收益集中在 Android 端的体积与运行时复杂度，代价可忽略。

| 维度 | 现状（GeographicLib + egm96-5.pgm） | 替换后（emericg/EGM96） | 收益 |
|---|---|---|---|
| 数据体积 | 18 MB PGM 网格文件，随 APK 打包 | ~3.27 MB 系数嵌入头文件，编译进二进制 | **APK 减小约 14.7 MB** |
| 运行时 I/O | 首次启动需把 18 MB 从 assets 拷到内部存储 | 无文件 I/O，数据已编入只读段 | 首启更快、无磁盘占用 |
| 内存占用 | `threadsafe=true` 全量网格载入 RAM ~18 MB | 系数在只读数据段 ~3.3 MB | **RAM 减少约 14.7 MB** |
| 依赖 | xrepo `geographiclib 2.1.1` 静态库（仅用了 Geoid 类） | 3 个纯 C 文件，无外部依赖 | **移除一个 xrepo 包** |
| 代码复杂度 | `prepareGeoidData()` ~40 行 + Geoid 构造 + 异常处理 | 单函数 `egm96_compute_altitude_offset(lat,lon)` | 代码简化 |
| 计算成本 | 网格双线性插值 O(1) | 360 阶球谐综合 O(n²)≈130k 次三角运算/点 | 略增，但定位更新约 1Hz，可忽略 |
| 精度 | EGM96 5' 网格插值 | EGM96 360 阶球谐直接计算 | 理论同源（均 EGM96 d360），差异亚米级，远小于消费级 GPS 误差（±数米） |
| 许可证 | GeographicLib (MIT) | Zlib（permissive） | 均可商用，无阻碍 |

### 精度说明
两者均基于 EGM96 360 阶模型。GeographicLib 的 `egm96-5.pgm` 是同一模型生成的 5' 网格再做插值；emericg 直接用球谐系数综合计算。对本应用（消费级 GPS 海拔显示）而言，两者差异远小于 GPS 自身误差，不影响可用性。

### 计算成本说明
球谐综合每次调用约 130k 次三角运算，单次约 0.1~1ms 量级。定位更新频率通常 1Hz，在 Android 主线程或 JNI 回调线程上完全无压力。相较之下省下的 18MB 内存与首启 I/O 收益更实在。

## 二、现状分析（基于 Phase 1 探索）

### 当前 EGM96 使用链路（仅 Android）
1. [xmake.lua](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/xmake.lua) L29：`add_requires("geographiclib 2.1.1", ...)` 声明依赖
2. [xmake.lua](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/xmake.lua) L76：`add_packages("geographiclib", ...)` 注入目标
3. [xmake.lua](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/xmake.lua) L173-181：构建后把 `assets/geoid/egm96-5.pgm` 拷到 `android/app/src/main/assets/geoid/`
4. [LocationProviderAndroid.cpp](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/Backend/LocationProviderAndroid.cpp)：
   - L3 `#include <GeographicLib/Geoid.hpp>`
   - L45 `std::unique_ptr<GeographicLib::Geoid> m_geoid`
   - L52-74 构造函数：调 `prepareGeoidData()` 拷贝数据，再 `make_unique<Geoid>("egm96-5", dir, true, true)`
   - L82-124 `prepareGeoidData()`：AAssetManager 打开 → 检查是否已拷贝 → 写入 internalDataPath/geoid/
   - L205-217 `onLocationPushed()`：`m_geoid->ConvertHeight(lat, lon, alt, ELLIPSOIDTOGEOID)`

### 关键确认
- GeographicLib **仅**用于 Android 端 Geoid 转换，macOS 用 `CLLocation.altitude`（已是 MSL），无需替换
- `LocationProviderMac.mm` 不涉及 GeographicLib
- 18 MB 的 `assets/geoid/egm96-5.pgm` 仅 Android 运行时需要

### 符号约定（替换时务必正确）
- emericg/EGM96：`egm96_compute_altitude_offset(lat, lon)` 返回大地水准面起伏 N
- 头文件明确：`MSL altitude = GPS altitude − N`
- GeographicLib：`ConvertHeight(h, ELLIPSOIDTOGEOID)` 内部即 `h_ellipsoid − N`，直接返回 MSL
- 故替换式为：`d.altitudeMSL = alt - egm96_compute_altitude_offset(lat, lon);`

## 三、实施步骤

### Step 1：引入 emericg/EGM96 源码（作为 submodule）
- 路径：`ThirdParty/EGM96/`
- 方式：`git submodule add https://github.com/emericg/EGM96.git ThirdParty/EGM96`
- 与现有 `ThirdParty/` 下 imgui/spdlog 等 submodule 约定一致
- 仅需 3 个文件：`EGM96.c`、`EGM96.h`、`EGM96_data.h`

### Step 2：修改 xmake.lua
- **移除** L29 的 `add_requires("geographiclib 2.1.1", ...)` 整行
- **移除** L76 `add_packages` 中的 `"geographiclib"`（保留 `nlohmann_json`、`fmt`）
- 为 Android 目标添加 `ThirdParty/EGM96/EGM96.c` 到源文件列表（或用 `add_files`/`add_includedirs`）
- 添加 `add_includedirs("ThirdParty/EGM96")` 以便 `#include "EGM96.h"`
- **移除** L173-181 拷贝 `egm96-5.pgm` 到 Android assets 的整段逻辑（不再需要）

### Step 3：改写 Backend/LocationProviderAndroid.cpp
- 移除 `#include <GeographicLib/Geoid.hpp>`，改为 `extern "C" { #include "EGM96.h" }`（EGM96.h 已有 `extern "C"` 守卫，直接 `#include "EGM96.h"` 即可）
- 移除成员 `std::unique_ptr<GeographicLib::Geoid> m_geoid`
- 移除 `prepareGeoidData()` 方法声明与实现（L40-41、L82-124 整段）
- 简化构造函数：删除 Geoid 加载逻辑（L52-74 的 try/catch 块），构造函数体可基本置空（仅保留 `g_Inst = this`）
- 改写 `onLocationPushed()`（L205-217）：
  ```cpp
  // 用 EGM96 球谐系数计算大地水准面起伏 N，椭球高减 N 得 MSL 正高
  double N = egm96_compute_altitude_offset(lat, lon);
  d.altitudeMSL = alt - N;
  ```
  无需 try/catch（C 函数不抛异常）；保留 `d.altitudeMSL = alt;` 作为注释说明的退化路径不再需要，因为计算不会失败（除非输入越界，但 lat/lon 来自系统定位，可信）
- 移除不再使用的头文件：`<filesystem>`、`<fstream>`、`<sys/stat.h>`、`<android/asset_manager.h>`（若仅 prepareGeoidData 使用）

### Step 4：清理资源文件
- 删除 `assets/geoid/egm96-5.pgm`（18 MB，不再需要）
- 删除 `assets/geoid/` 目录（若空）
- 删除 Android 构建产物中的 `android/app/src/main/assets/geoid/`（由 Step 2 的 xmake 逻辑移除自动生效）

### Step 5：更新文档 CLAUDE.md
- 第 10 行：Android 描述改为「经 emericg/EGM96 球谐系数修正为 MSL」
- 第 50 行：删除 `assets/geoid/egm96-5.pgm` 条目
- 第 56 行：xrepo 包列表移除 `geographiclib 2.1.1`；ThirdParty submodule 列表新增 `EGM96`
- 第 67 行：陷阱 3（EGM96 数据文件下载）整条删除
- 第 21 行注释：移除「EGM96 数据拷到 ...」相关说明

## 四、假设与决策

1. **假设**：emericg/EGM96 的 360 阶球谐综合精度对本应用足够。依据：与 GeographicLib 同源 EGM96，差异亚米级，远小于 GPS 误差。
2. **决策**：以 git submodule 方式引入（与项目现有 ThirdParty 约定一致），而非直接拷贝 3 个文件。
3. **决策**：仅替换 Android 端；macOS 不受影响（本就用 CoreLocation 的 MSL）。
4. **假设**：`egm96_compute_altitude_offset` 输入 lat/lon 在合法范围 `[-90,90]`/`[-180,180]`，来自 Android LocationManager，可信，无需额外校验。
5. **决策**：不保留向后兼容/双实现。直接替换，保持代码简洁。

## 五、验证步骤

1. **macOS 构建**：`xmake f -m releasedbg && xmake` 确保零警告（移除 geographiclib 不影响 macOS，但需确认 xmake.lua 改动未破坏 macOS 目标编译）
2. **Android 构建**：`xmake f -p android -a arm64-v8a && xmake` 确认 EGM96.c 编译通过、链接成功
3. **APK 体积**：对比替换前后 APK 大小，预期减少约 14.7 MB
4. **功能验证**（Android 真机）：
   - 启动应用，授权定位
   - 对比显示的海拔与替换前数值，应基本一致（差异在亚米级）
   - 确认首次启动不再有 18 MB 文件拷贝日志
5. **日志检查**：确认无 `Geoid load failed` 等错误；构造函数相关日志正常

## 六、回滚方案

若替换后精度或稳定性不达预期：
- `git revert` 相关提交即可恢复 GeographicLib + egm96-5.pgm
- submodule 引入是独立的，删除 `ThirdParty/EGM96` 不影响其他依赖
