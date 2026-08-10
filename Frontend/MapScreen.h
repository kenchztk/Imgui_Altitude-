#pragma once
#include "imgui/imgui.h"
#include "Backend/LocationProvider.h"
#include "Frontend/Screen.h"

// 地图屏：等高线地形 + 路线 + 定位标记 + 坐标卡（坐标卡使用真实定位数据）。
class MapScreen
{
public:
    bool render(const LocationData& data, LocationStatus st,
                LocationProvider& loc, const ImVec2& displaySize);

private:
    void renderTerrain(ImDrawList* dl, const ImVec2& min, const ImVec2& max);
};
