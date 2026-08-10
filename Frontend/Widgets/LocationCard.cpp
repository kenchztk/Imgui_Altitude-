#include "Frontend/Widgets/LocationCard.h"
#include "Frontend/UIHelpers.h"
#include "imgui/imgui_internal.h"
#include "IconsFontAwesome6.h"
#include "Frontend/AppSettings.h"
#include <spdlog/spdlog.h>
#include <cstdio>

namespace LocationCard {

void Render(const LocationData& data, const ImVec2& mapMin, const ImVec2& mapMax)
{
    AppSettings& s = AppSettings::Instance();

    float cardH = 64.0f;
    float cardX = mapMin.x + 14.0f;
    float cardW = (mapMax.x - mapMin.x) - 28.0f;
    float cardY = mapMax.y - cardH - 12.0f;

    UI::DrawRoundedRect(ImVec2(cardX, cardY), ImVec2(cardX + cardW, cardY + cardH),
                        IM_COL32(21, 27, 39, 255), 16.0f);

    ImGui::SetCursorScreenPos(ImVec2(cardX + 14.0f, cardY + 10.0f));
    ImGui::TextColored(ImVec4(0.39f, 0.46f, 0.56f, 1.0f), "当前海拔");

    char buf[64];
    if (data.valid)
        snprintf(buf, sizeof(buf), "%.0f %s · %.4f°N %.4f°E",
                 s.displayLength(data.fusedAltitude), s.lengthUnit(),
                 data.latitude, data.longitude);
    else
        snprintf(buf, sizeof(buf), "等待定位…");
    ImGui::SetCursorScreenPos(ImVec2(cardX + 14.0f, cardY + 30.0f));
    ImGui::TextColored(ImVec4(0.97f, 0.98f, 0.99f, 1.0f), "%s", buf);

    // 精度 + 开始记录按钮
    if (data.valid)
    {
        ImGui::SetCursorScreenPos(ImVec2(cardX + cardW - 170.0f, cardY + 12.0f));
        ImGui::TextColored(ImVec4(0.39f, 0.46f, 0.56f, 1.0f), "精度 ±%.0f m", data.horizontalAccuracy);
    }
    ImGui::SetCursorScreenPos(ImVec2(cardX + cardW - 96.0f, cardY + 34.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(IM_COL32(80, 180, 255, 255)));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(IM_COL32(80, 180, 255, 255)));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::ColorConvertU32ToFloat4(IM_COL32(80, 180, 255, 255)));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(8, 15, 30, 255));
    if (ImGui::Button(ICON_FA_PLAY " 记录", ImVec2(82.0f, 0)))
        spdlog::info("[UI] 地图屏点击「记录」");
    ImGui::PopStyleColor(4);
}

} // namespace LocationCard
