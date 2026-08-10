#include "Frontend/NavBar.h"
#include "Frontend/UIHelpers.h"
#include "Frontend/Widgets/Segmented.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "IconsFontAwesome6.h"
#include <string>

namespace {
const char* kLabels[] = { "实时", "记录", "地图", "设置" };
const char* kIcons[] = { ICON_FA_LOCATION_ARROW, ICON_FA_LIST, ICON_FA_MAP_PIN, ICON_FA_GEAR };
constexpr int kCount = 4;
} // namespace

Screen NavBar::render(Screen current)
{
    Screen result = current;
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float barH = 56.0f;

    // 整条胶囊背景（分段高亮与文字由 DrawSegmented 绘制）
    UI::DrawRoundedRect(p0, ImVec2(p0.x + w, p0.y + barH), IM_COL32(21, 27, 39, 255), barH * 0.5f);

    std::string navLabels[kCount];
    const char* labels[kCount];
    for (int i = 0; i < kCount; ++i)
    {
        navLabels[i] = std::string(kIcons[i]) + "  " + kLabels[i];
        labels[i] = navLabels[i].c_str();
    }

    Segmented::Style st;
    st.drawBg = false;                 // 胶囊背景已单独画
    st.gap    = 0.0f;                  // 整宽均分，无段间距
    st.inset  = 4.0f;                  // 选中高亮在段内收缩 4px
    st.rounding = (barH - 8.0f) * 0.5f;

    int sel = Segmented::Render(labels, kCount, (int)current, w, barH, st);
    if (sel != (int)current)
        result = (Screen)sel;

    ImGui::Spacing();
    return result;
}
