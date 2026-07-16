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

class Backend
{
    public:
        Backend();
        ~Backend();
        
        static Backend& Instance();

        bool init();

        // 定位提供者（macOS CoreLocation / Android LocationManager + GeographicLib）
        LocationProvider& location();

    private:
        std::unique_ptr<LocationProvider> m_location;
};