# 用 BeginChild/EndChild 隔离三段 UI 布局

## 概述

将 `AltitudeDisplay::render()` 内部的三段内容（模式切换 / 海拔显示 / 底部控制区）各自用 `BeginChild`/`EndChild` 包裹，让 ImGui 自动管理游标流转，彻底移除"renderXxx 返回 contentBottomY + 手动 SetCursorScreenPos 跳转"的脆弱机制。第1/3段用 `ImGuiChildFlags_AutoResizeY` 自适应内容高度，第2段弹性填满剩余空间。

## 现状分析

### 当前问题

[AltitudeDisplay.cpp](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/Frontend/AltitudeDisplay.cpp) 的 `render()` 主入口（L87-117）：

```cpp
// 第1段：标准按钮，游标自动推进
interacted |= renderModeSwitcher(winW);

// 第2段：ImDrawList 绝对坐标绘制，返回 contentBottomY
float contentBottomY = wp.y;
switch (m_mode) { ... 各 renderXxx 返回 float ... }

// 关键修复：手动跳转游标到第2段底部 ← 脆弱点
ImGui::SetCursorScreenPos(ImVec2(wp.x, contentBottomY + ItemSpacing.y));

// 第3段：标准按钮
interacted |= renderControls(data, st, loc);
```

**问题根源**：
1. 5 个 `renderXxx` 函数都用 `SetCursorScreenPos` 绝对坐标绘制（基于 `GetWindowPos()` + `displaySize`），绕过 ImGui 游标系统
2. 每个函数需返回"内容底部 Y"供 `render()` 跳转游标，返回值计算各不相同且易错
3. 坐标基准是主窗口 `GetWindowPos()` + `io.DisplaySize`，而非局部可用区域，导致与第1/3段耦合

### 现有约束

- ImGui 1.92.8（docking 分支），支持 `ImGuiChildFlags_AutoResizeY`（imgui.h L1271）
- Docking 已启用（[mainMacDesktop.mm:66](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/mainMacDesktop.mm#L66)）但未用 DockSpace，当前是单窗口铺满
- macOS 毛玻璃：[Frontend.cpp:52](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/Frontend/Frontend.cpp#L52) 已 `PushStyleColor(ImGuiCol_WindowBg, alpha=0.3)`，BeginChild 需保持透明
- `Frontend/*.cpp` 由 [xmake.lua:48](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/xmake.lua#L48) glob 自动编译，无需改构建

## 改动方案

### 核心布局（render 主入口）

```cpp
bool AltitudeDisplay::render(const LocationData& data, LocationStatus st,
                             LocationProvider& loc, const ImVec2& displaySize)
{
    updateDisplayedAltitude(data.valid ? (float)data.altitudeMSL : m_displayedAlt);
    bool interacted = false;

    // macOS 毛玻璃：子窗口背景透明，继承父窗口半透明效果
#if defined(__APPLE__)
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
#endif

    // ---- 第1段：模式切换按钮组（高度自适应）----
    ImGui::BeginChild("##mode_switcher", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
    interacted |= renderModeSwitcher();
    ImGui::EndChild();

    // ---- 第2段：海拔显示区（弹性填满剩余空间）----
    // 需预留第3段高度 h3，第2段高度 = 当前可用高度 - h3
    float h3 = estimateControlsHeight(data);
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("##alt_display", ImVec2(0, ImMax(avail.y - h3, 50.0f)), ImGuiChildFlags_None);
    {
        // 子窗口内获取局部可用尺寸，传给 renderXxx
        ImVec2 subAvail = ImGui::GetContentRegionAvail();
        switch (m_mode) {
            case Mode::MinimalCenter:  renderMinimalCenter(data, st, subAvail.x, subAvail.y); break;
            case Mode::Gauge:          renderGauge(data, st, subAvail.x, subAvail.y); break;
            case Mode::HUD:            renderHUD(data, st, subAvail.x, subAvail.y); break;
            case Mode::Card:           renderCard(data, st, subAvail.x, subAvail.y); break;
            case Mode::CircleGradient: renderCircleGradient(data, st, subAvail.x, subAvail.y, loc.lastHeading()); break;
        }
    }
    ImGui::EndChild();

    // ---- 第3段：底部控制区（高度自适应）----
    ImGui::BeginChild("##controls", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);
    interacted |= renderControls(data, st, loc);
    ImGui::EndChild();

#if defined(__APPLE__)
    ImGui::PopStyleColor();
#endif
    return interacted;
}
```

**关键点**：
- 第1/3段 `AutoResizeY`：高度随内容自动调整，游标在 `EndChild()` 后自动推进到下方
- 第2段 `size.y = avail.y - h3`：填满第1段之后、第3段之前的剩余空间
- `ImMax(..., 50.0f)` 兜底，避免 h3 估算偏大导致第2段高度为负
- 子窗口背景透明（macOS），保留毛玻璃

### 第3段高度预估

新增私有方法 `estimateControlsHeight()`，根据 `m_showDetails` 状态计算：

```cpp
float AltitudeDisplay::estimateControlsHeight(const LocationData& data) const
{
    ImGuiStyle& sty = ImGui::GetStyle();
    float btnH = ImGui::GetFrameHeight();           // 单个按钮高度
    float sp   = sty.ItemSpacing.y;
    float h = btnH + sp;                            // 详情/收起按钮
    if (m_showDetails) {
        float lineH = ImGui::GetTextLineHeightWithSpacing();
        int lines = 3;                              // macOS: 经纬度+水平精度
#ifdef __ANDROID__
        lines = 5;                                  // Android 多椭球高+geoid修正
#endif
        h += lineH * lines + sty.SeparatorPadding * 2 + sp;  // 详情文本 + 分隔线
    }
    h += btnH;                                      // 开始/停止按钮
    return h;
}
```

> 说明：用状态预估而非"上一帧实际高度"，避免首帧/详情切换瞬间的布局抖动。估算误差由第2段 `ImMax` 兜底吸收。

### renderXxx 签名与坐标改造

**签名变更**（[AltitudeDisplay.h](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/Frontend/AltitudeDisplay.h) L42-47）：

```cpp
// 旧：返回 contentBottomY（绝对屏幕坐标）
float renderMinimalCenter(const LocationData& data, LocationStatus st, float winW, float winH);

// 新：无返回值，参数语义改为子窗口可用尺寸
void renderMinimalCenter(const LocationData& data, LocationStatus st, float availW, float availH);
// 其余 4 个同理
```

**坐标基准改造**（每个 renderXxx 内部）：

```cpp
// 旧：基于主窗口 + displaySize
ImVec2 wp = ImGui::GetWindowPos();           // 主窗口
float centerX = wp.x + winW * 0.5f;          // winW = displaySize.x
float centerY = wp.y + winH * 0.38f;

// 新：在 BeginChild 内，GetWindowPos 返回子窗口位置
ImVec2 wp = ImGui::GetWindowPos();           // 子窗口 ##alt_display
float centerX = wp.x + availW * 0.5f;        // availW = 子窗口可用宽度
float centerY = wp.y + availH * 0.38f;
```

由于 `ImGuiChildFlags_None`（无 Borders）不启用 WindowPadding，`GetWindowPos()` 即子窗口内容区左上角，`GetContentRegionAvail()` 即可用尺寸，坐标计算自然基于子窗口局部区域。

**CenteredText 辅助函数**（[AltitudeDisplay.cpp:13](file:////Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/Frontend/AltitudeDisplay.cpp#L13)）：
- 签名不变（`winW` 参数语义改为子窗口可用宽度）
- 内部 `GetWindowPos()` 在 BeginChild 内返回子窗口位置，逻辑无需改动
- 调用处传入 `availW` 而非 `winW`

**renderStatusBadge**：同理，`GetWindowPos()` 在 BeginChild 内返回子窗口位置，内部坐标计算无需改动。

### renderModeSwitcher / renderControls 改造

**renderModeSwitcher**（[AltitudeDisplay.cpp:121](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/Frontend/AltitudeDisplay.cpp#L121)）：
- 移除 `float winW` 参数，内部用 `ImGui::GetContentRegionAvail().x` 获取子窗口宽度
- 按钮宽度计算 `btnW = (availW - pad*2 - spacing*4) / 5` 改用 availW

**renderControls**（[AltitudeDisplay.cpp:492](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/Frontend/AltitudeDisplay.cpp#L492)）：
- 签名不变（已用 `ImVec2(-1, 0)` 全宽按钮，自动适配子窗口宽度）
- 内部逻辑无需改动

### 文件改动清单

| 文件 | 改动内容 |
|------|---------|
| [Frontend/AltitudeDisplay.h](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/Frontend/AltitudeDisplay.h) | 1. 5 个 renderXxx 返回类型 `float` → `void`，参数名 `winW/winH` → `availW/availH`<br>2. 新增 `float estimateControlsHeight(const LocationData&) const;` 声明<br>3. renderModeSwitcher 移除 `float winW` 参数 |
| [Frontend/AltitudeDisplay.cpp](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/Frontend/AltitudeDisplay.cpp) | 1. `render()` 用三个 BeginChild 包裹（如上核心布局）<br>2. 5 个 renderXxx 内部坐标基准从 `displaySize` 改为 `GetContentRegionAvail()`<br>3. 移除所有 `return contentBottomY` 语句<br>4. 新增 `estimateControlsHeight()` 实现<br>5. renderModeSwitcher 内部 winW 改用 `GetContentRegionAvail().x` |
| [Frontend/Frontend.cpp](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/Frontend/Frontend.cpp) | 无需改动（仍调用 `m_altDisplay.render(data, st, loc, io.DisplaySize)`） |

## 假设与决策

1. **不引入 DockSpace**：用户原话"在现有 docking 模式下"指 ImGui 已编译 docking 分支的环境，BeginChild 在此环境下正常工作；不改为 DockSpace + dockable windows（超出"隔离三段"诉求）
2. **第1/3段用 AutoResizeY**：ImGui 1.92.8 支持，高度随内容自动调整，无需手动估算
3. **第2段高度 = avail.y - h3**：h3 用 `estimateControlsHeight()` 基于状态预估；用 `ImMax(..., 50.0f)` 兜底防负值
4. **子窗口背景透明**：macOS 下 `PushStyleColor(ImGuiCol_ChildBg, 0)` 保留毛玻璃；Android 无此需求（`#if defined(__APPLE__)` 保护）
5. **保留 displaySize 参数**：render() 签名不变，Frontend.cpp 无需改动；内部主用 `GetContentRegionAvail()`
6. **CenteredText/renderStatusBadge 不改签名**：它们的 `GetWindowPos()` 调用在 BeginChild 内自动返回子窗口位置，逻辑天然兼容

## 验证步骤

1. **编译检查**（macOS）：
   ```bash
   xmake f -m releasedbg && xmake
   ```
   确保零警告（`-Wall -Wextra`，spdlog 第三方告警除外）

2. **运行检查**：
   ```bash
   open pkg/NativeApp.app
   ```
   - 窗口正常显示，毛玻璃效果保留
   - 三段内容垂直排列，无重叠
   - 切换 5 种模式，第2段内容均在子窗口内正确居中
   - 点击"详情"展开/收起，第2段高度弹性变化，第3段跟随移动，无抖动/溢出
   - 模式切换按钮、开始/停止按钮可正常交互

3. **回归检查**：
   - 确认 5 种模式（MinimalCenter/Gauge/HUD/Card/CircleGradient）均正常渲染
   - 确认 CircleGradient 的 N/S 箭头、渐变圆环位置正确
   - 确认状态徽章（呼吸圆点）位置正确

4. **代码审查**：
   - 确认无残留的 `return contentBottomY` 语句
   - 确认 renderXxx 内部不再直接用 `displaySize`，改用传入的 `availW/availH`
   - 确认 `SetCursorScreenPos` 仅在 BeginChild 内部用于局部定位（如 CenteredText），不再用于跨段游标跳转
