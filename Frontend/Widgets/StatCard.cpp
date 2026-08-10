#include "Frontend/Widgets/StatCard.h"
#include "Frontend/UIHelpers.h"
#include "imgui/imgui_internal.h"

namespace StatCard {

void Render(ImVec2 pos, ImVec2 size, float rounding,
            const char* label, const char* value,
            const char* unit, ImU32 valueColor)
{
    const ImU32 kCardBg = IM_COL32(21, 27, 39, 255);
    const ImVec4 cLabel(0.39f, 0.46f, 0.56f, 1.0f);

    UI::DrawRoundedRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), kCardBg, rounding);

    // 标签
    ImGui::SetCursorScreenPos(ImVec2(pos.x + 12.0f, pos.y + 9.0f));
    ImGui::TextColored(cLabel, "%s", label);

    // 大数值（1.7x 字号，底部对齐）
    ImGui::SetWindowFontScale(1.7f);
    ImVec2 vsz = ImGui::CalcTextSize(value);
    float vY = pos.y + size.y - 10.0f - vsz.y;
    ImGui::SetCursorScreenPos(ImVec2(pos.x + 12.0f, vY));
    ImGui::TextColored(ImVec4(ImGui::ColorConvertU32ToFloat4(valueColor)), "%s", value);
    ImGui::SetWindowFontScale(1.0f);

    // 单位（紧随数值右侧，垂直居中）
    if (unit && *unit)
    {
        ImVec2 usz = ImGui::CalcTextSize(unit);
        ImGui::SetCursorScreenPos(ImVec2(pos.x + 12.0f + vsz.x + 5.0f, vY + vsz.y * 0.5f - usz.y * 0.5f));
        ImGui::TextColored(cLabel, "%s", unit);
    }
}

} // namespace StatCard
