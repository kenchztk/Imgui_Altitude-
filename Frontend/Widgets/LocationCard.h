#pragma once
#include "imgui/imgui.h"
#include "Backend/LocationProvider.h"

// 地图坐标卡：当前海拔 + 坐标/精度 + 「记录」按钮（浮于地图底部）。
// 复用自 MapScreen::renderLocationCard。仿 imgui-knobs 形态：单一命名空间 + 立即模式函数。
namespace LocationCard {

// mapMin/mapMax 为地图区域，用于定位卡片。卡片使用真实定位数据。
void Render(const LocationData& data, const ImVec2& mapMin, const ImVec2& mapMax);

} // namespace LocationCard
