# 修复布局：使 renderControls 固定在面板底部，不与中间内容重合

## 问题分析

当前 `AltitudeDisplay::render()` 布局顺序：
1. 顶部：`renderModeSwitcher` - 模式切换按钮组
2. 中间：根据选中模式调用渲染函数（**使用绝对比例坐标直接绘制到整个窗口**）
3. 底部：`renderControls` - 详情按钮 + 开始/停止按钮，使用 ImGui 流式布局

问题：多种渲染模式（尤其是 `CircleGradient`）的内容已经延伸到窗口底部，流式布局的 `renderControls` 会与之重叠。

## 解决方案概要

采用**"预留空间 + 贴底绘制"**方案：
1. 先测量 `renderControls` 控件块需要的高度
2. 从总窗口高度中扣除：模式切换高度 + 控件高度 + 间距，得到**主内容可用高度**
3. 传给各个模式渲染函数，让主内容只在可用高度区域内按比例绘制
4. 最后将光标移到底部预留区域，绘制 `renderControls`

该方案改动最小，利用现有比例坐标逻辑自动适配，整体布局更协调。

## 具体修改

### 文件：`Frontend/AltitudeDisplay.cpp`

修改 `AltitudeDisplay::render()` 函数（第 79-100 行）：

**原逻辑**：
```cpp
bool AltitudeDisplay::render(...) {
    float winW = displaySize.x;
    float winH = displaySize.y;
    ...
    interacted |= renderModeSwitcher(winW);
    switch (m_mode) { ... /* 使用 winH 渲染 */ }
    interacted |= renderControls(...);
}
```

**新逻辑**：
1. 获取当前 ImGui 样式（间距、padding）
2. 先**测量** `renderControls` 需要的高度（使用 `ImGui::CalcSize` 或 临时 `BeginChild` 测量）
3. 计算 `contentMaxHeight = 总高度 - 模式切换高度 - controlsHeight - 间距`
4. 调用模式渲染函数时，传入 `contentMaxHeight` 代替原来的 `winH`
5. 用 `SetCursorScreenPos` 将光标移到 `窗口左下角 + 偏移` 位置
6. 绘制 `renderControls`

### 文件：`Frontend/AltitudeDisplay.h`

需要修改五个模式渲染函数的签名：
- `void renderMinimalCenter(..., float contentMaxHeight);`
- `void renderGauge(..., float contentMaxHeight);`
- `void renderHUD(..., float contentMaxHeight);`
- `void renderCard(..., float contentMaxHeight);`
- `void renderCircleGradient(..., float contentMaxHeight);`

所有现有的比例计算（如 `centerY = contentMaxHeight * 0.38f`）会自动适配，不需要改每个模式内部的计算逻辑。

## 关键技术点

- **测量控件高度**：使用 `ImGui` 的 `BeginChild` + `GetCursorPosY` 方法可以准确测量出 `renderControls` 在当前窗口宽度下需要的实际高度（考虑 `m_showDetails` 展开/折叠两种情况）
- **坐标原点**：所有模式已经相对于窗口左上角计算坐标，传入 `contentMaxHeight` 后，比例计算自然压缩到上方区域
- **间距保留**：要计入 `ImGuiStyle::ItemSpacing` 和 `WindowPadding`，避免贴边太紧

## 验证步骤

1. 编译运行，检查五种显示模式：
   - `MinimalCenter`：大字居中，控件在底部，不重叠
   - `Gauge`：仪表盘在上方，控件不重叠
   - `HUD`：刻度尺在上方，控件不重叠
   - `Card`：卡片在上方，控件不重叠
   - `CircleGradient`：同心圆在上方，控件不重叠（原来此模式重叠最明显）
   
2. 检查详情展开/折叠：
   - 折叠时控件高度较小，主内容区域相应变高
   - 展开时控件高度变大，主内容区域相应压缩
   - 无论哪种状态，控件始终贴底，不重叠

3. 检查不同窗口尺寸（macOS 缩放、旋转手机）：比例自适应依然有效

## 预期效果

- 控件始终固定在窗口最底部
- 五种显示模式的内容都在上方区域，不会与控件重叠
- 展开/收起详情时，主内容区域自动调整可用高度
- 原有比例布局美感保持不变
