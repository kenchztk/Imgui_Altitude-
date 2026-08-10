#pragma once
#include "imgui/imgui.h"
#include "Backend/LocationProvider.h"
#include "Frontend/Screen.h"

// 运动记录屏：统计概览 + 轨迹卡片列表（当前为模拟数据，预留真实 TrackManager 接入）。
class RecordsScreen
{
public:
    bool render(const LocationData& data, LocationStatus st,
                LocationProvider& loc, const ImVec2& displaySize);

private:
    struct Track
    {
        const char* name;
        const char* date;
        float distKm;
        int upM;
        const char* dur;
    };

    static const Track kTracks[3];
};
