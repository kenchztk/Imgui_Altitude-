# 修复布局系统：从"绝对中心定位"改为"自上而下流式布局"

## Summary

当前各显示模式的渲染函数用绝对屏幕坐标（`winH * 0.38` 等）定位元素，绕过了 ImGui 的游标布局系统。而底部的 `renderControls` 依赖 ImGui 游标位置。两者不协调导致控件交叠。

**解决方案**：让第二段（海拔显示区）也纳入 ImGui 的游标流式布局——从 `renderModeSwitcher` 结束后的游标位置开始，自上而下依次推进游标绘制各元素，第二段结束后游标自然停在内容底部，`renderControls` 就能正确接续。

## Current State Analysis

### 问题根因

ImGui 的布局模型是**游标流式布局**：每调用一个 `ImGui::Text`/`ImGui::Button` 等，控件画在当前游标位置，然后游标自动下移。`ImGui::SetCursorScreenPos` 可以手动指定游标位置。

当前代码的问题：

1. **`renderModeSwitcher`**（[L116-150](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/Frontend/AltitudeDisplay.cpp#L116)）：用标准 ImGui 控件，游标正常推进，结束后游标在按钮组下方。

2. **`renderMinimalCenter`**（[L154-195](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/Frontend/AltitudeDisplay.cpp#L154)）：
   - 用 `ImDrawList` 画光晕（不影响游标）
   - 用 `SetCursorScreenPos` 把游标跳到 `winH * 0.38` 处画数值
   - 用 `CenteredText` 画单位、状态徽章（也用 `SetCursorScreenPos` 跳到绝对位置）
   - **函数结束后游标停在状态徽章所在行**，而非内容底部
   - 若状态徽章的绝对 Y < 按钮组底部 Y，游标实际在按钮组上方，导致第三段从那里开始画，和第二段重叠

3. **`renderGauge`**（[L199-289](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/Frontend/AltitudeDisplay.cpp#L199)）：
   - 全部用 `ImDrawList` 绘制（刻度、指针、圆环），完全不影响游标
   - 仅 `CenteredText` 用 `SetCursorScreenPos` 跳到绝对位置画文字
   - **函数结束后游标位置 = 最后一次 SetCursorScreenPos 的位置**，与实际绘制内容的底部无关

4. **`renderControls`**（[L473-512](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/Frontend/AltitudeDisplay.cpp#L473)）：
   - 调 `ImGui::Spacing()` 后直接 `ImGui::Button`，依赖游标在正确位置
   - 但游标实际位置不确定 → 按钮可能画在第二段内容中间

5. **`renderHUD`**、**`renderCard`**、**`renderCircleGradient`**：同样的问题，混合使用 `ImDrawList`（不影响游标）和 `SetCursorScreenPos`（跳到绝对位置）。

### 关键发现

`CenteredText` 辅助函数（[L20-29](file:///Users/kench/Documents/codespace/mine/imgui/Imgui_Altitude-/Frontend/AltitudeDisplay.cpp#L20)）每次都用 `SetCursorScreenPos` 跳到绝对 Y 坐标，画完后游标停在那个 Y 位置。多次调用后游标位置取决于最后一次调用的 Y 值，而非内容实际底部。

## Proposed Changes

### 核心思路：第二段结束后统一设置游标到内容底部

在 `render()` 主入口中，第二段渲染完成后、调用 `renderControls` 之前，**手动将游标设置到第二段实际内容的底部**。这样第三段就能正确接续。

具体实现：每个 `renderXxx` 函数计算并返回自己内容区域的底部 Y 坐标（相对于窗口），`render()` 用这个值设置游标。

### 1. 修改 `AltitudeDisplay.h`

所有 5 个渲染函数签名改为返回 `float`（内容底部 Y 坐标）：

```cpp
// 返回内容底部 Y 坐标（相对于窗口原点，含 wp.y 偏移），供 render() 设置游标
float renderMinimalCenter(const LocationData& data, LocationStatus st, float winW, float winH);
float renderGauge(const LocationData& data, LocationStatus st, float winW, float winH);
float renderHUD(const LocationData& data, LocationStatus st, float winW, float winH);
float renderCard(const LocationData& data, LocationStatus st, float winW, float winH);
float renderCircleGradient(const LocationData& data, LocationStatus st, float winW, float winH, double heading);
```

### 2. 修改 `AltitudeDisplay.cpp`

#### 2.1 修改 `render()` 主入口

```cpp
// 第2段：海拔显示区域（按当前模式绘制），获取内容底部 Y
float contentBottomY = wp.y;  // 默认 fallback
switch (m_mode)
{
    case Mode::MinimalCenter:  contentBottomY = renderMinimalCenter(data, st, winW, winH); break;
    case Mode::Gauge:          contentBottomY = renderGauge(data, st, winW, winH); break;
    case Mode::HUD:            contentBottomY = renderHUD(data, st, winW, winH); break;
    case Mode::Card:           contentBottomY = renderCard(data, st, winW, winH); break;
    case Mode::CircleGradient: contentBottomY = renderCircleGradient(data, st, winW, winH, loc.lastHeading()); break;
}

// 关键修复：将游标设置到第二段内容底部，确保第三段正确接续
ImVec2 wp = ImGui::GetWindowPos();
ImGui::SetCursorScreenPos(ImVec2(wp.x, contentBottomY + ImGui::GetStyle().ItemSpacing.y));

// 第3段：底部控制区
interacted |= renderControls(data, st, loc);
```

#### 2.2 修改各 `renderXxx` 函数：返回内容底部 Y

每个函数在最后 `return` 自己内容区域的底部 Y 坐标（绝对屏幕坐标，含 `wp.y`）。

**`renderMinimalCenter`**：
```cpp
// 函数末尾，状态徽章底部即为内容底部
float bottomY = centerY + valSize.y * 0.5f + 44.0f + ImGui::GetTextLineHeightWithSpacing();
return bottomY;
```

**`renderGauge`**：
```cpp
// 圆盘底部 = center.y + R，状态徽章在 center.y + R*0.35 附近
// 取较大者
float gaugeBottom = center.y + R + 10.0f;  // 圆环底部 + 余量
float badgeBottom = center.y + R * 0.35f + ImGui::GetTextLineHeightWithSpacing();
return ImMax(gaugeBottom, badgeBottom);
```

**`renderHUD`**：
```cpp
// HUD 刻度尺底部 = y1 + 余量
return y1 + 10.0f;
```

**`renderCard`**：
```cpp
// 卡片底部 = p_max.y + 状态徽章高度
return p_max.y + 10.0f;
```

**`renderCircleGradient`**：
```cpp
// 大圆底部 = center.y + R1 + 状态徽章
float circleBottom = center.y + R1 + 24.0f + ImGui::GetTextLineHeightWithSpacing();
return circleBottom;
```

### 3. 不修改的部分

- `renderModeSwitcher`：保持不变，它用标准 ImGui 控件，游标正常推进
- `renderControls`：保持不变，它用标准 ImGui 控件，依赖游标位置
- `renderDetailsSection`：保持不变
- `CenteredText` 辅助函数：保持不变（仍用绝对坐标定位文本，这不影响最终修复）

## Assumptions & Decisions

1. **不重写为纯 ImGui 控件布局**：各模式的 `ImDrawList` 矢量绘制（圆环、刻度、指针等）必须保留，无法用标准控件替代。因此采用"绘制用绝对坐标，但函数返回内容底部 Y"的折中方案。

2. **返回绝对屏幕坐标**：各函数返回的 Y 含 `wp.y`（窗口原点偏移），`render()` 直接用 `SetCursorScreenPos` 设置，避免再转换。

3. **`renderControls` 内部不变**：只要进入 `renderControls` 时游标在正确位置，其内部的 `ImGui::Spacing()` + `ImGui::Button` 就能正常流式布局。

4. **兼容现有视觉设计**：不改变各模式的视觉外观，只修复布局衔接逻辑。

## Verification Steps

1. **macOS 构建**：`xmake f -m releasedbg && xmake`，确认零代码警告
2. **运行验证**：`open pkg/NativeApp.app`
   - 逐个切换 5 种模式，确认每种模式下"详情/开始测量"按钮都在海拔显示内容下方，不重叠
   - 展开"详情"，确认次要信息在按钮和海拔内容之间，不重叠
   - 确认各模式的视觉外观与修复前一致（位置参数不变）
3. **调整窗口大小**：拖动改变窗口尺寸，确认各模式下布局自适应不重叠
