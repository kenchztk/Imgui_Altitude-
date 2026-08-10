#pragma once
#include "imgui/imgui.h"

// 状态徽章：呼吸圆点 + 状态文案（复用自 RealtimeScreen::renderStatusBadge）。
// 仿 imgui-knobs 形态：单一命名空间 + 立即模式函数。
namespace StatusBadge {

// text 为完整文案（可含图标字形与精度后缀）；breathe=true 时圆点做呼吸缩放。
// centerX/posY 为相对当前窗口坐标（posY 相对窗口顶部），与调用方保持一致。
void Render(const char* text, float centerX, float posY, ImU32 color, bool breathe);

} // namespace StatusBadge
