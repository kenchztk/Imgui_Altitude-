#pragma once
#include "imgui/imgui.h"
#include "Frontend/AltitudeDisplay.h"
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
        ImVec4 clear_color{0.10f, 0.10f, 0.10f, 1.00f};
        // 最近一次检测到用户交互的时间，用于空闲判定
        std::chrono::steady_clock::time_point m_lastActiveTime{};
        AltitudeDisplay m_altDisplay;  // 海拔醒目显示模块（4 模式 + 动效）
};