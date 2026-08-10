#pragma once
#include "imgui/imgui.h"

// 圆形仪表（海拔 hero）：背景细圆环 + 270° 进度弧 + 中心标签/大数字/单位。
// 只读式（无拖拽编辑），仿 imgui-knobs 形态：单一命名空间 + 立即模式函数。
namespace RadialGauge {

// center/R 为圆心与半径；ratio 归一化到 [0,1]，仅 drawArc=true 时绘制进度弧。
// 布局/间距与原 RealtimeScreen::renderAltitudeHero 逐像素一致。
void Render(ImVec2 center, float R, float ratio,
            const char* label, const char* value, const char* unit,
            ImU32 arcColor, bool drawArc);

} // namespace RadialGauge
