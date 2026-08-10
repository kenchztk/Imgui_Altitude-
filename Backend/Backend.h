#pragma once
#include "nlohmann/json.hpp"
#include <optional>
#include <chrono>
#include <string>
#include <mutex>
#include <any>
#include <map>
#include <memory>
#include "LocationProvider.h"
#include "PressureProvider.h"
#include "AltitudeFusion.h"

class Backend
{
    public:
        Backend();
        ~Backend();
        
        static Backend& Instance();

        bool init();

        // 定位提供者（macOS CoreLocation / Android LocationManager + EGM96）
        LocationProvider& location();

        // 融合后的最新数据（GPS+EGM 绝对基准 ⊕ 气压计相对变化）。
        // Frontend 每帧调用；无气压计设备时 fusedAltitude 等于 altitudeMSL。
        LocationData currentFused();

        // 设备是否具备气压计（UI 据此决定是否显示气压/速率）
        bool hasBarometer() const;

    private:
        std::unique_ptr<LocationProvider> m_location;
        std::unique_ptr<PressureProvider> m_pressure;
        AltitudeFusion m_fusion;
};