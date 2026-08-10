#include "Frontend/Widgets/StatStrip.h"
#include "Frontend/UIHelpers.h"
#include "imgui/imgui_internal.h"

namespace StatStrip {

void Render(ImVec2 pos, ImVec2 size, float rounding, const Item* items, int count)
{
    UI::DrawRoundedRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                        IM_COL32(21, 27, 39, 255), rounding);

    float colW = size.x / (float)count;
    for (int i = 0; i < count; ++i)
    {
        float cx = pos.x + colW * i + colW * 0.5f;

        // 大数值（1.8x 字号，水平居中于列内偏上）
        ImGui::SetWindowFontScale(1.8f);
        ImVec2 vsz = ImGui::CalcTextSize(items[i].value);
        ImGui::SetCursorScreenPos(ImVec2(cx - vsz.x * 0.5f,
            pos.y + size.y * 0.5f - vsz.y * 0.5f - 6.0f));
        ImGui::TextColored(ImVec4(0.97f, 0.98f, 0.99f, 1.0f), "%s", items[i].value);
        ImGui::SetWindowFontScale(1.0f);

        // 小标签（底部）
        ImVec2 lsz = ImGui::CalcTextSize(items[i].label);
        ImGui::SetCursorScreenPos(ImVec2(cx - lsz.x * 0.5f, pos.y + size.y - 18.0f));
        ImGui::TextColored(ImVec4(0.39f, 0.46f, 0.56f, 1.0f), "%s", items[i].label);
    }
}

} // namespace StatStrip
