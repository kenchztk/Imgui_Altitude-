#include "Frontend/Widgets/StatusBadge.h"
#include "imgui/imgui_internal.h"
#include <cmath>

namespace StatusBadge {

void Render(const char* text, float centerX, float posY, ImU32 color, bool breathe)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();

    ImGui::SetWindowFontScale(1.0f);
    ImVec2 ts = ImGui::CalcTextSize(text);

    // 呼吸圆点：breathe 时半径按 sin 缩放
    float dotR = 5.0f;
    if (breathe)
    {
        float t = (float)ImGui::GetTime();
        dotR = 4.0f + 2.0f * (0.5f + 0.5f * sinf(t * 3.0f));
    }
    float dotX = wp.x + centerX - ts.x * 0.5f - 14.0f;
    dl->AddCircleFilled(ImVec2(dotX, wp.y + posY + ts.y * 0.5f), dotR, color, 16);

    ImGui::SetCursorScreenPos(ImVec2(wp.x + centerX - ts.x * 0.5f, wp.y + posY));
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(color), "%s", text);
}

} // namespace StatusBadge
