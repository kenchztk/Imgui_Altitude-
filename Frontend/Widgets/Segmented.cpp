#include "Frontend/Widgets/Segmented.h"
#include "imgui/imgui_internal.h"

namespace Segmented {

int Render(const char** labels, int count, int current, float totalW, float h,
           const Style& st)
{
    int result = current;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImGui::GetCursorScreenPos();

    if (st.drawBg)
        dl->AddRectFilled(p0, ImVec2(p0.x + totalW, p0.y + h),
                          st.bg, h * 0.5f, ImDrawFlags_RoundCornersAll);

    const float segW = (totalW - st.gap * (count - 1)) / (float)count;
    const float selRound = (st.rounding > 0.0f) ? st.rounding : h * 0.5f;

    for (int i = 0; i < count; ++i)
    {
        const float x = p0.x + i * (segW + st.gap);
        const bool selected = (i == current);
        const ImVec2 bp(x, p0.y);

        if (selected)
        {
            const ImVec2 a(bp.x + st.inset, bp.y + st.inset);
            const ImVec2 b(bp.x + segW - st.inset, bp.y + h - st.inset);
            dl->AddRectFilled(a, b, st.selBg, selRound, ImDrawFlags_RoundCornersAll);
        }

        ImGui::SetCursorScreenPos(bp);
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##seg", ImVec2(segW, h)))
            result = i;
        if (st.hover && !selected && ImGui::IsItemHovered())
            dl->AddRectFilled(bp, ImVec2(bp.x + segW, bp.y + h),
                              IM_COL32(255, 255, 255, 15), h * 0.5f, ImDrawFlags_RoundCornersAll);
        ImGui::PopID();

        const ImVec2 ts = ImGui::CalcTextSize(labels[i]);
        dl->AddText(ImVec2(bp.x + (segW - ts.x) * 0.5f, bp.y + (h - ts.y) * 0.5f),
                    selected ? st.selText : st.idleText, labels[i]);
    }

    ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + h));
    return result;
}

} // namespace Segmented
