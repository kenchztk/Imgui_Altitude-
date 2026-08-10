#include "Frontend/Widgets/RadialGauge.h"
#include "Frontend/UIHelpers.h"
#include "imgui/imgui_internal.h"
#include <cmath>

namespace RadialGauge {

void Render(ImVec2 center, float R, float ratio,
            const char* label, const char* value, const char* unit,
            ImU32 arcColor, bool drawArc)
{
    float winW = ImGui::GetContentRegionAvail().x;   // 用于文本水平居中
    ImVec2 wp = ImGui::GetWindowPos();

    const ImVec4 cLabel(0.39f, 0.46f, 0.56f, 1.0f);  // 标签/单位灰
    const ImVec4 cValue(0.97f, 0.98f, 0.99f, 1.0f);  // 大数字白

    // 背景细圆环 + 进度弧（270° 扫角，底部开口）
    UI::DrawRing(center, R, IM_COL32(80, 180, 255, 40), 1.5f, 80);
    if (drawArc)
    {
        const float a0 = 3.0f * IM_PI / 4.0f;
        const float sweep = 3.0f * IM_PI / 2.0f;
        UI::DrawGaugeArc(center, R, ratio, a0, sweep, arcColor, 6.0f, 80);
    }

    // 中心：小标签 + 大数字 + 单位（间距与原实现逐像素一致）
    UI::CenteredText(label, 1.4f, cLabel, winW, center.y - R * 0.42f - wp.y);
    float valY = center.y - R * 0.10f;
    UI::CenteredText(value, 5.5f, cValue, winW, valY - wp.y);
    float unitY = valY + 5.5f * 18.0f * 0.5f + 6.0f;   // 数字基线下方
    UI::CenteredText(unit, 1.6f, cLabel, winW, unitY - wp.y);
}

} // namespace RadialGauge
