#pragma once
#include "imgui/imgui.h"
#include "Backend/LocationProvider.h"
#include "Frontend/Screen.h"

// 设置屏：单位 / 定位 / 显示 / 关于。单位与开关写入 AppSettings，实时影响其他屏显示。
class SettingsScreen
{
public:
    bool render(const LocationData& data, LocationStatus st,
                LocationProvider& loc, const ImVec2& displaySize);
};
