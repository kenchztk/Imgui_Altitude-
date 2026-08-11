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
    // 底部导航高度：Android 触控目标更大，加高到 80；桌面端保持 56
    float barH =
#ifdef __ANDROID__
        80.0f;
#else
        56.0f;
#endif

    // Android 设备底部为圆角矩形：按系统圆角半径内缩导航栏并居中避让圆角；
    // 圆角半径由 Kotlin 经 JNI 回传（AndroidGetBottomCornerRadius），另加少量余量
#ifdef __ANDROID__
    extern float AndroidGetBottomCornerRadius();
    const float sideMargin = ImMin(AndroidGetBottomCornerRadius() + 8.0f, w * 0.25f);
#else
    const float sideMargin = 0.0f;
#endif
    ImVec2 pN = ImVec2(p0.x + sideMargin, p0.y);
    float navW = w - sideMargin * 2.0f;

    // 整条胶囊背景（分段高亮与文字由 DrawSegmented 绘制）；半透明实现伪玻璃通透
    UI::DrawRoundedRect(pN, ImVec2(pN.x + navW, pN.y + barH), IM_COL32(21, 27, 39, 180), barH * 0.5f);

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

    // 导航内容字号：Android 触控目标更大，放大图标+文字；桌面端保持原大小
    const float navFontScale =
#ifdef __ANDROID__
        1.15f;
#else
        1.0f;
#endif
    if (navFontScale != 1.0f)
        ImGui::SetWindowFontScale(navFontScale);
    ImGui::SetCursorScreenPos(pN);
    int sel = Segmented::Render(labels, kCount, (int)current, navW, barH, st);
    if (navFontScale != 1.0f)
        ImGui::SetWindowFontScale(1.0f);
    if (sel != (int)current)
        result = (Screen)sel;

    return result;
}
