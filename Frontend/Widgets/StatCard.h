#pragma once
#include "imgui/imgui.h"

// 信息卡：圆角背景 + 标签 + 大数值 + 单位（复用自 RealtimeScreen 副信息条等）。
// 仿 imgui-knobs 形态：单一命名空间 + 立即模式函数。
namespace StatCard {

// pos/size 为卡片矩形，rounding 为圆角；unit 可为空（无单位）。
// 布局与原 RealtimeScreen::renderSecondaryInfo 的 drawCard lambda 逐像素一致。
void Render(ImVec2 pos, ImVec2 size, float rounding,
            const char* label, const char* value,
            const char* unit, ImU32 valueColor);

} // namespace StatCard
