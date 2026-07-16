# 海拔高度瞩目界面重设计方案

## Summary

为海拔高度数值设计 4 种可切换的醒目显示模式，并加入动效增强视觉冲击力。4 种模式通过顶部图标按钮组切换，以海拔数值为视觉核心，次要信息（经纬度/精度等）默认隐藏可展开。保留 macOS 毛玻璃透明背景，配合强调色块确保对比度。

- **4 种模式**：极简居中大字、航空仪表盘、HUD 刻度尺、卡片强调
- **动效**：数字平滑插值过渡、呼吸光晕、扫描线、旋转图标、边框脉冲
- **切换**：顶部 4 个图标按钮组，当前选中高亮
- **不持久化**：每次启动默认极简居中大字模式
- **次要信息**：默认折叠，点击"详情"按钮展开

## Current State Analysis

当前 [Frontend.cpp:76-89](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/Frontend/Frontend.cpp#L76-L89) 的海拔显示：

- 标题 `TextDisabled`（灰色），数值 `SetWindowFontScale(3.0f)` 放大（18px×3=54px 视觉），左对齐
- 单位 `m (MSL)` 用 1.2x 同行显示
- 纯文本堆叠，无图形装饰、无动效、无强调色
- 次要信息（经纬度/椭球高/精度）始终平铺显示
- 已 `#include "imgui/imgui_internal.h"`，可使用 `ImDrawList` 全套矢量绘图 API

**已确认可用的资源**：
- 图标：`ICON_FA_TEXT_HEIGHT`(极简)、`ICON_FA_GAUGE_HIGH`(仪表盘)、`ICON_FA_RULER_VERTICAL`(HUD)、`ICON_FA_BORDER_ALL`(卡片)、`ICON_FA_CHEVRON_DOWN/UP`(详情展开)、`ICON_FA_MOUNTAIN`(海拔标题)
- ImDrawList API：`AddCircle/Filled`、`AddRect/Filled/FilledMultiColor`、`AddLine/AddLineV/AddLineH`、`AddText`(含指定字体字号重载)、`PathArcTo`、`PathStroke`、`PathFillConvex`
- 构建自动包含 `Frontend/*.cpp`（[xmake.lua:48](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/xmake.lua#L48)），新增文件无需改 xmake.lua

## Proposed Changes

### 1. 新建 `Frontend/AltitudeDisplay.h`

定义显示模式枚举与 AltitudeDisplay 类，封装 4 种渲染模式 + 动效状态。

```cpp
#pragma once
#include "imgui/imgui.h"
#include "Backend/LocationProvider.h"

class AltitudeDisplay
{
public:
    enum class Mode : int
    {
        MinimalCenter = 0,  // 极简居中大字
        Gauge,              // 航空仪表盘
        HUD,                // HUD 刻度尺
        Card                // 卡片强调
    };

    // 渲染主入口：在当前 ImGui 窗口内绘制海拔显示区
    // displayArea 为可用于绘制的矩形区域（屏幕坐标）
    void render(const LocationData& data, LocationStatus st, const ImVec2& displayArea);

    Mode currentMode() const { return m_mode; }
    void setMode(Mode m) { m_mode = m; }

private:
    Mode m_mode = Mode::MinimalCenter;

    // 动效状态
    float m_displayedAlt = 0.0f;       // 平滑插值后的显示值
    bool  m_altInit = false;           // 首帧是否已初始化 m_displayedAlt
    bool  m_showDetails = false;       // 次要信息展开状态

    // 颜色常量（ImU32）
    static ImU32 accentCyan();         // 主强调青蓝 (80,180,255)
    static ImU32 accentAmber();        // 次强调琥珀 (255,180,60)
    static ImU32 statusColor(LocationStatus st);

    // 数字平滑插值
    void updateDisplayedAltitude(float targetAlt);

    // 4 种模式渲染
    void renderMinimalCenter(const LocationData& data, LocationStatus st, float winW, float winH);
    void renderGauge(const LocationData& data, LocationStatus st, float winW, float winH);
    void renderHUD(const LocationData& data, LocationStatus st, float winW, float winH);
    void renderCard(const LocationData& data, LocationStatus st, float winW, float winH);

    // 公用子绘制
    void renderStatusBadge(LocationStatus st, float centerX, float posY);   // 状态徽章（带旋转/脉冲）
    void renderDetailsSection(const LocationData& data);                     // 展开的次要信息
    void renderModeSwitcher(float winW);                                     // 顶部模式切换按钮组
    void renderControls(LocationStatus st, LocationProvider& loc);           // 开始/停止按钮
};
```

### 2. 新建 `Frontend/AltitudeDisplay.cpp`

实现 4 种模式与动效。核心绘制使用 `ImGui::GetWindowDrawList()` 获取 `ImDrawList*`。

#### 2.1 通用动效逻辑

**数字平滑插值**（所有模式共用，`updateDisplayedAltitude`）：
```cpp
void AltitudeDisplay::updateDisplayedAltitude(float targetAlt)
{
    if (!m_altInit) { m_displayedAlt = targetAlt; m_altInit = true; return; }
    // 每帧逼近 15%，约 6-7 帧到位（60fps 下约 100ms）
    m_displayedAlt += (targetAlt - m_displayedAlt) * 0.15f;
    if (std::abs(targetAlt - m_displayedAlt) < 0.05f)  // 吸附
        m_displayedAlt = targetAlt;
}
```

**状态颜色映射**（`statusColor`，沿用现有配色）：
- Idle: (0.8,0.8,0.8) 灰
- Starting: (1.0,0.8,0.2) 黄
- Active: (0.3,0.9,0.4) 绿
- Denied/Error: (0.95,0.3,0.3) 红

#### 2.2 render() 主入口流程

```
1. 读取 io.DisplaySize 作为 winW/winH
2. 调 updateDisplayedAltitude(data.altitudeMSL)
3. 调 renderModeSwitcher(winW)        // 顶部 4 图标按钮
4. 按 m_mode 分发到对应 renderXxx()
5. 调 renderControls(st, loc)          // 底部开始/停止按钮
```

#### 2.3 模式切换按钮组（renderModeSwitcher）

- 窗口顶部水平排列 4 个等宽图标按钮
- 每个按钮用对应图标：`ICON_FA_TEXT_HEIGHT` / `ICON_FA_GAUGE_HIGH` / `ICON_FA_RULER_VERTICAL` / `ICON_FA_BORDER_ALL`
- 当前模式按钮用 `PushStyleColor(ImGuiCol_Button, accentCyan)` 高亮
- 按钮宽度 = `(winW - 总间距) / 4`，用 `ImGui::Button(icon, ImVec2(btnW, 0))`
- 点击设置 `m_mode` 并刷新 `m_lastActiveTime`（通过返回值让 Frontend 感知交互）

#### 2.4 模式一：极简居中大字（renderMinimalCenter）

**布局**（垂直方向，winH 为窗口高）：
```
┌───────────────────────────────┐
│  [极简][表盘][HUD][卡片]      │  ← 切换按钮组
│                               │
│                               │
│         123.4                 │  ← 居中超大字（6倍字号）
│         m (MSL)               │  ← 单位（1.5倍）
│      ◎ 定位中                 │  ← 状态徽章（呼吸圆点）
│                               │
│    [ ▼ 详情 ]                 │  ← 展开按钮（可展开次要信息）
│    [  开始测量  ]             │  ← 控制按钮
└───────────────────────────────┘
```

**绘制步骤**：
1. 计算数值字号：`SetWindowFontScale(6.0f)`（18px×6=108px 视觉，Android 因主字体18px+全局3x缩放已够大，此处统一用 6.0）
2. **呼吸光晕**：在数值中心位置用 `dl->AddCircleFilled` 画半径随时间脉动的半透明青色圆
   ```cpp
   float t = (float)ImGui::GetTime();
   float pulse = 0.5f + 0.5f * sinf(t * 2.0f);          // 0~1
   float glowR = 120.0f + 20.0f * pulse;
   ImU32 glow = IM_COL32(80, 180, 255, (int)(30 + 25 * pulse));
   dl->AddCircleFilled(center, glowR, glow, 48);
   ```
3. **数值居中**：用 `ImGui::CalcTextSize` 计算文本宽，`SetCursorPosX((winW - textW) * 0.5f)` 水平居中
4. 数值颜色：Active 时用青色强调 `IM_COL32(120,220,255,255)`，其他状态用白色
5. 单位 `m (MSL)` 用 `SetWindowFontScale(1.5f)` 居中显示在数值下方
6. **状态徽章**：数值下方居中，定位中(Starting)时图标 `ICON_FA_ARROWS_ROTATE` 旋转（用 `ImGui::Image` 不可行，改用文字+角度不可控，因此用呼吸圆点 `ICON_FA_CIRCLE` 配 alpha 脉冲替代旋转）
7. 无效数据显示 `--`（居中放大）

#### 2.5 模式二：航空仪表盘（renderGauge）

**布局**：
```
┌───────────────────────────────┐
│  [极简][表盘][HUD][卡片]      │
│                               │
│         ╭─────────╮           │
│        ╱  ·  ·  ·  ╲          │  ← 外圈刻度（270°扇形）
│       │  ·       ·  │         │
│       │    123.4    │         │  ← 中心数值
│       │     m       │         │
│        ╲    │      ╱           │  ← 指针（指向当前高度角度）
│         ╰────┴────╯            │
│      ◎ 定位中                  │
│    [ ▼ 详情 ]  [开始测量]      │
└───────────────────────────────┘
```

**绘制步骤**：
1. 表盘中心 `center = (winW/2, winH*0.42)`，半径 `R = min(winW, winH) * 0.32`
2. **外圈**：`dl->AddCircle(center, R, IM_COL32(120,120,130,255), 64, 3.0f)`
3. **270° 扇形刻度**：角度范围从 `210°`（左下）到 `-30°`（右下），即 `7π/6` 到 `-π/6`
   - 主刻度每 `30°` 一条长线（`AddLine` 从 R 到 R-15，琥珀色）
   - 次刻度每 `6°` 一条短线（R 到 R-7，灰色）
   - 用 `PathArcTo` + 循环 `AddLine` 实现
4. **动态量程**：以当前海拔为中心 ±200m，刻度标注 5 个值（中心、±100、±200）
5. **指针**：根据 `(m_displayedAlt - 中心值) / 200m` 计算偏移角度，`dl->AddLine(center, tip, accentCyan, 4.0f)`，指针尾部画小圆 `AddCircleFilled`
6. **扫描线动效**：一条从中心出发的半径线随时间旋转
   ```cpp
   float sweep = t * 0.6f;   // 缓慢旋转
   // 用 PathArcTo 画一段渐变扇形（多段不同 alpha 的弧线模拟拖尾）
   ```
7. **中心数值**：`SetWindowFontScale(4.0f)` 居中显示 `m_displayedAlt`，下方 1.2x 显示 `m (MSL)`

#### 2.6 模式三：HUD 刻度尺（renderHUD）

**布局**（左侧垂直刻度带 + 右侧数值，航空 HUD 风格）：
```
┌───────────────────────────────┐
│  [极简][表盘][HUD][卡片]      │
│                               │
│  300 ┤    ┃                   │
│      │    ┃                   │  ← 刻度带（随海拔滚动）
│  200 ┤    ┃━━━━━ 123.4 m     │  ← 中间参考线（固定）+ 数值
│      │    ┃                   │
│  100 ┤    ┃                   │
│      │    ◎ 定位中            │
│    [ ▼ 详情 ]  [开始测量]      │
└───────────────────────────────┘
```

**绘制步骤**：
1. 刻度带 x 位置 `tapeX = winW * 0.25`，垂直范围 `y0=winH*0.2` 到 `y1=winH*0.65`
2. **像素/米比例**：`ppm = (y1-y0) / 200.0f`（200m 跨度填满带高）
3. **刻度滚动**：以 `m_displayedAlt` 为中心，每个刻度线 y 坐标：
   ```cpp
   float centerY = (y0 + y1) * 0.5f;
   for (int alt = floor(m_displayedAlt/50)*50 - 200; alt <= m_displayedAlt + 200; alt += 50) {
       float y = centerY - (alt - m_displayedAlt) * ppm;
       if (y < y0 || y > y1) continue;
       // 主刻度（100m 倍数）长线 + 数字标注，次刻度（50m）短线
       dl->AddLineV(tapeX, y - 8, y + 8, ...);
       if (alt % 100 == 0) dl->AddText(..., fmt::format("{}", alt).c_str());
   }
   ```
4. **固定参考线**：中间水平高亮线 `dl->AddLineH(tapeX-10, tapeX+30, centerY, accentCyan, 3.0f)`
5. **数值显示**：参考线右侧 `SetWindowFontScale(4.5f)` 显示 `m_displayedAlt`，青绿色 `IM_COL32(80,255,200,255)`
6. **科技感配色**：刻度用青绿 `IM_COL32(60,200,180,255)`，背景半透明深色矩形 `AddRectFilled` 增强对比

#### 2.7 模式四：卡片强调（renderCard）

**布局**：
```
┌───────────────────────────────┐
│  [极简][表盘][HUD][卡片]      │
│                               │
│  ╭─────────────────────────╮  │
│  │  ⛰ 海拔高度             │  │  ← 圆角卡片（强调色渐变背景）
│  │                         │  │
│  │     123.4 m (MSL)       │  │  ← 大字数值
│  │  ━━━━━━━━━━━━━━         │  │  ← 状态进度条
│  │  ◎ 定位中               │  │
│  ╰─────────────────────────╯  │
│    [ ▼ 详情 ]  [开始测量]      │
└───────────────────────────────┘
```

**绘制步骤**：
1. 卡片矩形：`p_min=(winW*0.1, winH*0.18)`，`p_max=(winW*0.9, winH*0.62)`
2. **渐变背景**：`dl->AddRectFilledMultiColor(p_min, p_max, 深青, 深蓝, 深蓝, 深青)`（左上右下渐变）
3. **圆角描边脉冲**：
   ```cpp
   float pulse = 0.5f + 0.5f * sinf(t * 2.5f);
   ImU32 border = IM_COL32(80, 180, 255, (int)(120 + 80 * pulse));
   dl->AddRect(p_min, p_max, border, 16.0f, 0, 2.5f);   // 16px 圆角
   ```
4. 卡片内标题：`ICON_FA_MOUNTAIN 海拔高度`，1.2x 字号，居中
5. 卡片内数值：`SetWindowFontScale(5.0f)` 居中显示，白色
6. **状态进度条**：数值下方一条 `AddRectFilled` 横条，宽度按状态比例（Active=满、Starting=半、其他=空），颜色用 statusColor

### 3. 修改 `Frontend/Frontend.h`

增加 AltitudeDisplay 成员：
```cpp
#include "Frontend/AltitudeDisplay.h"
// ...
private:
    ImVec4 clear_color{0.10f, 0.10f, 0.10f, 1.00f};
    std::chrono::steady_clock::time_point m_lastActiveTime{};
    AltitudeDisplay m_altDisplay;   // 新增：海拔显示模块
```

### 4. 修改 `Frontend/Frontend.cpp`

重构 `update()`（[L39-L135](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/Frontend/Frontend.cpp#L39-L135)）：

- 保留：毛玻璃 PushStyleColor（L48-53）、窗口创建铺满（L54-57）、数据获取（L60-62）、交互检测（L42-46）
- **删除**：原有海拔数值绘制（L64-128 的状态文案/大字/详细信息/控制按钮）
- **替换为**：调用 `m_altDisplay.render(data, st, io.DisplaySize)`
- 保留：PopStyleColor（L131-133）

`render()` 内部会处理模式切换按钮、海拔数值绘制、状态、详情展开、控制按钮。交互检测需扩展：AltitudeDisplay 的按钮点击也应刷新 `m_lastActiveTime`——通过 `render()` 返回 `bool` 表示是否有交互，或让 Frontend 在调用后检测 `io.WantCaptureMouse`。简单方案：render 内部按钮点击时直接调用 `Frontend::Instance().notifyActive()`（新增轻量方法刷新时间戳）。

**notifyActive 新增**（Frontend.h/cpp）：
```cpp
void Frontend::notifyActive() { m_lastActiveTime = std::chrono::steady_clock::now(); }
```

## 颜色方案

| 用途 | RGB | 说明 |
|------|-----|------|
| 主强调青蓝 | (80,180,255) | 极简光晕、仪表指针、HUD 参考线、卡片描边 |
| 次强调琥珀 | (255,180,60) | 仪表盘刻度 |
| HUD 青绿 | (80,255,200) | HUD 数值与刻度 |
| 卡片渐变深青 | (30,60,90) | 卡片背景左上 |
| 卡片渐变深蓝 | (20,40,70) | 卡片背景右下 |
| 状态色 | 沿用现有 | Idle 灰/Starting 黄/Active 绿/Denied 红 |

## Assumptions & Decisions

1. **新建独立模块 AltitudeDisplay**：4 种模式 + 动效约 500+ 行，独立文件保持 Frontend.cpp 清晰。`Frontend/*.cpp` 自动编译，无需改 xmake.lua。
2. **不持久化模式选择**：每次启动默认 MinimalCenter（用户确认）。
3. **主字体字号硬编码 18px 不改**：保持现有行为，通过 `SetWindowFontScale` 放大。Android 端已有全局 3x 缩放补偿。
4. **数字插值用线性逼近**：`displayed += (target-displayed)*0.15`，简单高效，60fps 下约 100ms 到位。
5. **旋转图标限制**：ImGui 文本无法旋转，Starting 状态用呼吸圆点替代旋转图标（AddCircleFilled 配 alpha 脉冲）。
6. **仪表盘量程动态**：以当前海拔 ±200m 为显示范围，避免海拔极高时刻度失效。
7. **次要信息折叠**：`m_showDetails` 控制，默认 false，点击"详情"按钮（ICON_FA_CHEVRON_DOWN/UP）切换。
8. **毛玻璃保留**：macOS 仍 alpha=0.3，各模式的强调色块/深色背景矩形负责确保对比度。
9. **fmt 格式化**：项目已用外部 fmt（SPDLOG_FMT_EXTERNAL），HUD 刻度数字标注用 `fmt::format`。

## Verification Steps

1. **macOS 构建**：
   ```bash
   xmake f -m releasedbg && xmake
   ```
   确保零警告（`-Wall -Wextra`），无编译错误。spdlog 的 fmt deprecation 告警可忽略。
2. **运行**：
   ```bash
   open pkg/NativeApp.app
   ```
   - 验证 4 个模式切换按钮可点击切换，当前模式高亮
   - 验证每个模式的海拔数值居中/正确显示
   - 验证动效：数字平滑过渡、呼吸光晕、扫描线旋转、边框脉冲
   - 验证"详情"展开/收起经纬度等信息
   - 验证开始/停止按钮功能正常
   - 验证定位中状态徽章动效
3. **Android 构建**（验证触屏与高 DPI）：
   ```bash
   xmake f -p android -a arm64-v8a && xmake
   ```
   - 验证 4 模式在移动端布局正常，字号足够大
   - 验证触屏点击切换模式与展开详情
4. **视觉检查**：
   - 各模式在毛玻璃透明背景下强调色块对比度足够
   - 数值在任意桌面壁纸下清晰可读
   - 动效流畅无明显卡顿（空闲降帧 5fps 时动效应仍可见或可接受静止）
