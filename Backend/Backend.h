#pragma once
#include "nlohmann/json.hpp"
#include <optional>
#include <chrono>
#include <string>
#include <mutex>
#include <any>
#include <map>

class Backend
{
    public:
        Backend();
        ~Backend();
        
        static Backend& Instance();

        bool init();
};