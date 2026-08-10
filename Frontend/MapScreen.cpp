#include "Frontend/MapScreen.h"
#include "Frontend/UIHelpers.h"
#include "Frontend/Widgets/LocationCard.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "Backend/LocationProvider.h"
#include <cmath>

void MapScreen::renderTerrain(ImDrawList* dl, const ImVec2& min, const ImVec2& max)
{
    float w = max.x - min.x;
    float h = max.y - min.y;
    // 等高线（横向波纹）
    for (int i = 0; i < 6; ++i)
    {
        float baseY = min.y + (h * (i + 1)) / 7.0f;
        dl->PathClear();
        const int steps = 48;
        for (int s = 0; s <= steps; ++s)
        {
            float tx = (float)s / (float)steps;
            float x = min.x + tx * w;
            float y = baseY + std::sinf(tx * 6.0f + i * 0.8f) * (h * 0.03f);
            dl->PathLineTo(ImVec2(x, y));
        }
        dl->PathStroke(IM_COL32(80, 180, 255, 28), 0, 1.5f);
    }
    // 路线（折线，自左下到中心偏上）
    ImVec2 p0(min.x + w * 0.18f, min.y + h * 0.85f);
    ImVec2 p1(min.x + w * 0.35f, min.y + h * 0.62f);
    ImVec2 p2(min.x + w * 0.55f, min.y + h * 0.55f);
    ImVec2 p3(min.x + w * 0.50f, min.y + h * 0.32f);
    dl->AddLine(p0, p1, IM_COL32(80, 180, 255, 220), 3.0f);
    dl->AddLine(p1, p2, IM_COL32(80, 180, 255, 220), 3.0f);
    dl->AddLine(p2, p3, IM_COL32(80, 180, 255, 220), 3.0f);
}

bool MapScreen::render(const LocationData& data, LocationStatus st,
                       LocationProvider& loc, const ImVec2& /*displaySize*/)
{
    (void)loc;
    (void)st;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImGuiStyle& sty = ImGui::GetStyle();
    ImVec2 wp = ImGui::GetWindowPos();
    float w = ImGui::GetContentRegionAvail().x;
    float h = ImGui::GetContentRegionAvail().y;
    float pad = sty.WindowPadding.x;

    float mapH = ImMax(h - 84.0f, 160.0f);
    ImVec2 mapMin(wp.x + pad, wp.y + pad);
    ImVec2 mapMax(wp.x + w - pad, mapMin.y + mapH);
    UI::DrawRoundedRect(mapMin, mapMax, IM_COL32(16, 28, 36, 255), 16.0f);
    renderTerrain(dl, mapMin, mapMax);

    // 定位标记（中心偏上）
    ImVec2 pin(mapMin.x + (mapMax.x - mapMin.x) * 0.50f, mapMin.y + (mapMax.y - mapMin.y) * 0.40f);
    dl->AddCircleFilled(pin, 13.0f, IM_COL32(80, 180, 255, 60), 24);
    dl->AddCircleFilled(pin, 7.0f, IM_COL32(80, 180, 255, 255), 20);
    dl->AddCircleFilled(pin, 3.0f, IM_COL32(255, 255, 255, 255), 12);

    // 坐标卡（浮于地图底部，使用真实定位数据）
    LocationCard::Render(data, mapMin, mapMax);
    return false;
}
