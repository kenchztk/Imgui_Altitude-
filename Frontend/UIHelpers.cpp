#include "Frontend/UIHelpers.h"
#include "imgui/imgui_internal.h"
#include <string>

namespace UI {

void CenteredText(const char* text, float scale, const ImVec4& col, float winW, float posY)
{
    ImGui::SetWindowFontScale(scale);
    ImVec2 sz = ImGui::CalcTextSize(text);
    ImVec2 wp = ImGui::GetWindowPos();
    ImGui::SetCursorScreenPos(ImVec2(wp.x + (winW - sz.x) * 0.5f, wp.y + posY));
    ImGui::TextColored(col, "%s", text);
    ImGui::SetWindowFontScale(1.0f);
}

void DrawRoundedRect(const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float rounding)
{
    ImGui::GetWindowDrawList()->AddRectFilled(p_min, p_max, col, rounding,
                                              ImDrawFlags_RoundCornersAll);
}

ImU32 accentCyan() { return IM_COL32(80, 180, 255, 255); }

void DrawRing(ImVec2 center, float R, ImU32 col, float thickness, int segments)
{
    ImGui::GetWindowDrawList()->AddCircle(center, R, col, segments, thickness);
}

void DrawGaugeArc(ImVec2 center, float R, float ratio, float a0, float sweep,
                  ImU32 col, float thickness, int segments)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float aEnd = a0 + sweep * ImClamp(ratio, 0.0f, 1.0f);
    dl->PathArcTo(center, R, a0, aEnd, segments);
    dl->PathStroke(col, 0, thickness);
}

} // namespace UI
