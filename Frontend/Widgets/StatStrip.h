#pragma once
#include "imgui/imgui.h"

// 统计条：整条圆角卡 + 多列（每列大数值 + 小标签，水平等分居中）。
// 复用自 RecordsScreen 统计概览卡。仿 imgui-knobs 形态：单一命名空间 + 立即模式函数。
namespace StatStrip {

struct Item { const char* value; const char* label; };

// pos/size 为整卡矩形，rounding 为圆角，items 为 count 个统计项。
// 布局与原 RecordsScreen 统计概览卡逐像素一致。
void Render(ImVec2 pos, ImVec2 size, float rounding, const Item* items, int count);

} // namespace StatStrip
