# 交接文档

> 本文件供新会话快速了解上下文，避免重复踩坑。

## 一、我们在做什么任务

重构 `AltitudeDisplay` 的 UI 布局，用 `BeginChild`/`EndChild` 将三段内容（模式切换 / 海拔显示 / 底部控制区）完全隔离，让 ImGui 自动管理游标流转，彻底移除"renderXxx 返回 contentBottomY + 手动 SetCursorScreenPos 跳转"的脆弱机制。

## 二、已经完成了什么

### 1. BeginChild 三段隔离（核心重构）

- **`Frontend/AltitudeDisplay.h`**：5 个 `renderXxx` 返回类型 `float` → `void`，参数名 `winW/winH` → `availW/availH`；`renderModeSwitcher` 移除 `winW` 参数；新增 `estimateControlsHeight()` 声明。
- **`Frontend/AltitudeDisplay.cpp`**：
  - `render()` 用三个 `BeginChild` 包裹：
    - 第1段 `##mode_switcher`：`ImGuiChildFlags_AutoResizeY | AlwaysUseWindowPadding`
    - 第2段 `##alt_display`：`size.y = avail.y - h3`，弹性填满剩余，`ImMax(..., 50.0f)` 兜底
    - 第3段 `##controls`：`ImGuiChildFlags_AutoResizeY | AlwaysUseWindowPadding`
  - macOS 下 `PushStyleColor(ImGuiCol_ChildBg, 0)` 保留毛玻璃
  - 5 个 `renderXxx` 内部坐标基准从 `displaySize` 改为子窗口 `GetContentRegionAvail()` + `GetWindowPos()`
  - `renderModeSwitcher` 用 `GetContentRegionAvail().x` 获取宽度
  - 新增 `estimateControlsHeight()` 按 `m_showDetails` 状态预估第3段高度

### 2. 尺寸比例调大

模式二/三/四的尺寸比例从"整个窗口"时代的值调大，适配子窗口可用空间：

| 模式 | 参数 | 旧值 | 新值 |
|------|------|------|------|
| Gauge | R 系数 | 0.28 | 0.40 |
| Gauge | center.y 比例 | 0.36 | 0.44 |
| Gauge | 数值字体 | 4.0f | 5.0f |
| HUD | tapeX 比例 | 0.28 | 0.35 |
| HUD | y0/y1 比例 | 0.26/0.64 | 0.12/0.88 |
| HUD | 数值字体 | 4.0f | 5.0f |
| Card | p_min/p_max Y | 0.20/0.64 | 0.06/0.94 |
| Card | 数值字体 | 5.0f | 6.0f |

## 三、当前卡在哪

**无明显卡点**，编译通过（零警告，仅 spdlog 第三方告警），应用已启动。

用户可能仍需视觉微调（如某些模式的比例、间距），但核心布局机制已稳定。

## 四、下一步计划

1. **视觉微调**（可选）：用户查看应用后，可能对某些模式的尺寸/间距提出进一步调整需求。
2. **Android 适配**：当前改动仅验证 macOS，Android 端需编译测试（`xmake f -p android -a arm64-v8a && xmake`）。
3. **Heading 支持**：macOS 的 `heading` 始终为 0.0（CoreLocation/CoreMotion 在 macOS 不可用），Android 端 JNI 接口已就绪（`nativeOnHeading`），需 Kotlin 侧调用。

## 五、踩过的坑（不要再踩）

### 1. 子窗口 padding 未计入高度预估

**问题**：第3段用 `AlwaysUseWindowPadding`，子窗口上下各加 `WindowPadding.y`（默认 8px），但 `estimateControlsHeight()` 没算这部分，导致第2段占多了，第3段被挤出窗口底部。

**修复**：`estimateControlsHeight()` 返回值加 `padY * 2.0f`。

### 2. 尺寸比例未适配子窗口

**问题**：模式二/三/四的尺寸比例是"整个窗口"时代的值，现在 `availH` 只是第2段子窗口的高度（比整个窗口小），这些模式显得更小。

**修复**：调大比例（见上文表格）。

### 3. `ImGuiStyle` 字段名错误

**问题**：`estimateControlsHeight()` 用了 `sty.SeparatorPadding`，但 ImGui 1.92.8 只有 `SeparatorTextPadding`（用于 `SeparatorText`，不是普通 `Separator`）。

**修复**：改用 `(sp + 1.0f)` 估算普通 Separator 高度。

### 4. `CenteredText` / `renderStatusBadge` 坐标语义

**关键约定**：
- `ImDrawList` 绘制用**绝对屏幕坐标**（`wp.x + offsetX, wp.y + offsetY`）
- `CenteredText` / `renderStatusBadge` 内部用 `GetWindowPos()`，传参是**相对子窗口的偏移**
- 当有绝对坐标变量（如 `center.y`）需传给 `CenteredText` 时，必须减 `wp.y` 转相对

**示例**：
```cpp
// center 是绝对坐标
CenteredText(valStr, 5.0f, col, availW, center.y - wp.y - R * 0.25f);  // 减 wp.y 转相对
renderStatusBadge(st, availW * 0.5f, center.y + R1 + 24.0f - wp.y);    // 减 wp.y 转相对
```

### 5. macOS 必须用 `open` 启动

**问题**：`xmake run` 启动裸二进制，无 bundle 上下文，CoreLocation 会静默丢弃授权请求（不弹窗不回调）。

**正确做法**：`open pkg/NativeApp.app`

### 6. macOS 毛玻璃需 `PushStyleColor`

**问题**：`BeginChild` 默认用 `ImGuiCol_ChildBg` 背景色，不透明会遮挡毛玻璃效果。

**修复**：macOS 下 `PushStyleColor(ImGuiCol_ChildBg, ImVec4(0,0,0,0))`，子窗口背景透明。

## 六、关键文件速查

| 文件 | 作用 |
|------|------|
| `Frontend/AltitudeDisplay.h` | 5 种显示模式声明 |
| `Frontend/AltitudeDisplay.cpp` | 渲染实现（BeginChild 布局 + 5 种模式绘制） |
| `Frontend/Frontend.cpp` | 主窗口创建，调用 `m_altDisplay.render()` |
| `Backend/LocationProvider.h` | 定位数据/状态/回调抽象 |
| `Backend/LocationProviderMac.mm` | macOS CoreLocation 实现（heading 始终 0） |
| `Backend/LocationProviderAndroid.cpp` | Android JNI 定位实现（heading 接口已就绪） |
| `mainMacDesktop.mm` | macOS 入口，启用 Docking（L66） |
| `xmake.lua` | 构建配置，`Frontend/*.cpp` 自动编译 |

## 七、构建命令

```bash
# macOS
xmake f -m releasedbg && xmake
open pkg/NativeApp.app

# Android
xmake f -p android -a arm64-v8a && xmake
```
