# 复刻渐变同心圆海拔显示模式方案

## Summary

在现有四种显示模式基础上，**新增第五种模式**，完全复刻 `Xnip2026-07-16_13-39-22.png` 的设计：渐变背景 + 三层同心圆环 + 中心大字 + N/S指南针（动态方向）+ 顶部气压/精度信息。保留原有的四种模式，可通过顶部图标按钮切换。

**设计要点**：
- 五层视觉：全屏半透明 → 最外层淡蓝绿圆环 → 中间棕黄圆环 → 内层红橙渐变大圆 → 中心白色大字
- N/S箭头指南针：根据设备方向传感器动态旋转（macOS 使用 CoreLocation heading，Android 需要 Kotlin 传感器回调）
- 顶部显示气压（无数据显示 `-- mmHg`）和海拔精度（使用现有 `horizontalAccuracy`）
- 仅大圆背景渐变，保留 macOS 毛玻璃透明特性
- 经纬度/椭球高等次要信息仍保留在可折叠的"详情"按钮中

## Current State Analysis

### 现有架构
- `LocationProvider` 抽象基类，macOS `LocationProviderMac` 用 CoreLocation，Android `LocationProviderAndroid` 用 Kotlin JNI 回调
- `LocationData` 结构体：已含 `altitudeMSL/latitude/longitude/horizontalAccuracy` 等，缺少 `heading`（方位角）字段
- `AltitudeDisplay` 已有四种模式枚举与渲染，顶部图标切换按钮组，添加新模式只需扩展枚举 + 新增渲染函数
- 已有 `ImDrawList` 绘图能力，可绘制 `AddCircleFilled`/`AddRectFilledMultiColor`/`AddTriangleFilled`/`AddText` 等，完全支持所需元素

### 图片设计分析

```
┌─────────────────────────────────────┐
│ [极简][表盘][HUD][卡片][圆] ← 新增图标│
│                                     │
│  751.81 mmHg  海拔精度: 1米           ← 顶部信息行
│       ╭───────────────────────╮       ← 三层同心圆渐变
│       │  ○─────────────────○  │
│       │ │  ○───────────○  │  │
│       │ │ │             │  │  │
│       │ │ │  当前海拔     │  │  │
│       │ │ │  3628 米     │  │  │
│       │ │ │             │  │  │
│       │ │  ○───────────○  │  │
│       │  ○─────────────────○  │
│       ╰───────────────────────╯
│   红 N ↑                浅 S ↓      ← 指南针箭头（动态旋转）
│           [ ▼ 详情 ]                ← 可折叠次要信息
│           [ 开始测量 ]              ← 控制按钮
└─────────────────────────────────────┘
```

**配色**：
- 全屏背景：保留毛玻璃透明（原设计保持）
- 最外层圆环：蓝绿色 `(120, 210, 200)` 低透明度
- 中间层圆环：黄褐色 `(200, 150, 80)` 低透明度
- 内层大圆：顶部红色渐变到底部橙色 `(255, 40, 40)` → `(255, 120, 40)`
- 文字：全部白色
- N 箭头：红色填充 `(220, 30, 30)`，S 箭头：浅米色半透明 `(220, 220, 200, 180)`

## Proposed Changes

### 1. 修改 `Backend/LocationProvider.h`

在 `LocationData` 结构体新增 `heading` 字段：

```cpp
struct LocationData
{
    double altitudeMSL = 0.0;        // 海拔（MSL/正高，米）
    double altitudeEllipsoid = 0.0;  // WGS84 椭球高（米）
    double latitude = 0.0;           // 纬度
    double longitude = 0.0;          // 经度
    double horizontalAccuracy = 0.0; // 水平精度
    double verticalAccuracy = 0.0;   // 垂直精度
    double heading = 0.0;            // 方位角（真北为 0，顺时针 0-360°，弧度制）- 新增
    int64_t timestampMs = 0;         // 采样时间戳
    bool valid = false;              // 是否已有有效数据
};
```

在 `LocationProvider` 基类新增虚方法：

```cpp
virtual double lastHeading() const { return 0.0; } // 最新方位角（弧度），默认返回 0（N 朝上）
```

### 2. 修改 `Backend/LocationProviderMac.mm`

添加 heading 支持：
- `LocationProviderMac` 新增 `double m_heading` 成员，默认 0.0
- 在 `ensureManager()` 开启 `startUpdatingHeading`
- 新增 delegate 方法 `didUpdateHeading`，更新 `m_heading` 并写入 `LocationData`
- `onLocation` 中将 `m_heading` 赋值给 `d.heading`

修改后 `onLocation` 中：
```objc
d.heading = m_heading;  // 新增
updateAndNotify(d, LocationStatus::Active);
```

### 3. 修改 `Backend/LocationProviderAndroid.cpp`

添加 heading 支持：
- `LocationProviderAndroid` 新增 `double m_heading` 成员，默认 0.0
- 新增 JNI 入口 `nativeOnHeading` 接收 Kotlin 推送的方位角更新
- `onLocationPushed` 中将 `m_heading` 赋值给 `d.heading`
- 在 `RegisterLocationNatives` 中新增注册 `nativeOnHeading`
- 需要在 Kotlin `MainActivity` 中添加传感器监听（此项目 Kotlin 工程在仓库外，本方案只完成 C++ JNI 接口，Kotlin 端由原有维护者添加）

JNI 方法签名：
```cpp
// 签名 (D)V：heading 弧度
static void JNICALL nativeOnHeading(JNIEnv* env, jobject thiz, jdouble headingRadians);
```

### 4. 修改 `Frontend/AltitudeDisplay.h`

- `Mode` 枚举新增 `CircleGradient`（第五种模式）
- 新增私有方法 `renderCircleGradient()` 声明

### 5. 实现 `renderCircleGradient()` 在 `AltitudeDisplay.cpp`

**绘制流程**：

```
1. 获取 winW/winH，计算大圆中心 center = (winW/2, winH*0.45)
2. 计算三层圆环半径：
   - R1 = min(winW, winH) * 0.42 （最外层淡蓝绿）
   - R2 = R1 * 0.82 （中间棕黄）
   - R3 = R1 * 0.68 （内层红橙渐变大圆）
3. 绘制三层圆环：
   - dl->AddCircleFilled(center, R1, IM_COL32(120, 210, 200, 100), 64);
   - dl->AddCircleFilled(center, R2, IM_COL32(200, 150, 80, 120), 64);
   - dl->AddCircleFilledMultiColor(...) 用 PathArcTo 实现垂直渐变红到橙
4. 绘制 N/S 方向箭头（按 heading 旋转）：
   - 获取 heading = loc.lastHeading()（弧度，0=北朝上）
   - N 箭头：位置在 center 上方 R1+16，三角形顶点指向北（0 + heading）
   - S 箭头：位置在 center 下方 R1+16，顶点指向南（π + heading）
   - 用 AddTriangleFilled 绘制三角形箭头
   - N: IM_COL32(220, 30, 30, 255), S: IM_COL32(220, 220, 200, 180)
5. 绘制中心文字：
   - "当前海拔"：1.5x 字号，白色半透明，居中
   - 海拔数值：5.5x 字号，纯白色，居中，保留平滑插值 m_displayedAlt
   - "米"：跟在数值右侧或下方，2.0x 字号
   - 气压行："-- mmHg" （固定，无数据）+ 精度："海拔精度: %.0f米"，居中浅色
6. 状态徽章：在大圆下方居中显示状态文字
7. 渲染详情展开按钮和控制按钮（同其他模式）
```

**渐变大圆实现**：使用 `PathArcTo` 分段构建扇形，每段用不同颜色插值实现垂直渐变。

### 6. 更新顶部切换按钮组 `renderModeSwitcher()`

- 第五个按钮图标使用 `ICON_FA_CIRCLE`
- 当前模式高亮逻辑保持不变

### 7. 验证 FontAwesome 图标可用性

已确认 `ICON_FA_CIRCLE` 在 [IconsFontAwesome6.h](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/ThirdParty/IconFontCppHeaders/IconsFontAwesome6.h#L300) 已定义。

## Assumptions & Decisions

1. **新增第五种不替换**：用户要求新增第五种模式，保留原有四种。
2. **完整复刻设计**：包括三层圆环渐变、N/S动态指南针、顶部气压精度文字。
3. **实际方向传感器**：用户要求实际实现，不是固定装饰：
   - macOS：CoreLocation `CLLocationManager` 原生支持 `startUpdatingHeading`，直接集成
   - Android：C++ 层完成 JNI 接口设计，Kotlin 层需要维护者添加传感器监听（因为 Android 工程不在此仓库）
4. **气压数据**：项目目前不采集气压，永远显示 `-- mmHg`，保持设计完整性。
5. **海拔精度**：使用现有 `LocationData.horizontalAccuracy` 显示，和原图一致。
6. **仅大圆背景渐变**：保留 macOS 窗口毛玻璃透明，不修改全局背景。
7. **次要信息保留折叠**：经纬度/椭球高/修正量仍保留在"详情"按钮中，不直接显示在底部。
8. **方位角单位**：使用弧度制，0 表示真北朝上，顺时针增加，和 CoreLocation 一致。

## 兼容性

- 无额外依赖，所有 API 都是 ImGui 内置 + 平台原生
- xmake 自动编译 `Frontend/*.cpp`，无需修改 xmake.lua
- 现有功能完全保留，只是新增一种模式，不影响原有四种模式

## Verification Steps

1. **macOS 构建**：
   ```bash
   xmake f -m releasedbg && xmake
   ```
   确认零代码警告，编译通过。

2. **运行验证**：
   - 确认第五个图标按钮出现在顶部，可点击切换
   - 确认三层同心圆渐变绘制正确，配色和原图一致
   - 确认中心文字"当前海拔"+大字数值+"米"排版正确
   - 确认顶部显示"-- mmHg  海拔精度: X米"
   - 确认N在上S在下，heading 变化时箭头正确旋转（需要设备带磁力计）
   - 确认详情展开/收起正常，开始/停止按钮正常
   - 确认其他四种模式不受影响，功能正常

3. **Android**：C++ JNI 接口已添加，Kotlin 端需要补充传感器回调。
