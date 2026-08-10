#pragma once
#include "imgui/imgui.h"
#include "Frontend/Screen.h"
#include "Frontend/RealtimeScreen.h"
#include "Frontend/RecordsScreen.h"
#include "Frontend/MapScreen.h"
#include "Frontend/SettingsScreen.h"
#include "Frontend/NavBar.h"
#include <chrono>
#ifdef _WIN32
#  include <d3d11.h>
#endif

class Frontend
{
    public:
        Frontend(){}
        ~Frontend(){}

        static Frontend& Instance();

        int  init(float vFontSize, float vGlobalScale);

        void update();

        const ImVec4& getClearColor();

        // 判断是否处于空闲状态（无交互超过 thresholdSec 秒），供主循环动态降帧
        bool isIdle(float thresholdSec = 3.0f) const;

    protected:

        void initFonts(float size_pixels);

#ifdef _WIN32
        ID3D11ShaderResourceView* LoadTextureFromFile(const char* filename);
#endif

    private:
        ImVec4 clear_color{0.063f, 0.078f, 0.110f, 1.00f}; // 与 Android 画布同色，避免安全区缝隙
        // 最近一次检测到用户交互的时间，用于空闲判定
        std::chrono::steady_clock::time_point m_lastActiveTime{};
        // 当前主屏
        Screen m_screen = Screen::Realtime;
        // 各屏实例
        RealtimeScreen m_realtime;
        RecordsScreen  m_records;
        MapScreen      m_map;
        SettingsScreen m_settings;
};
