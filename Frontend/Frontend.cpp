#ifdef _WIN32
#  include "mainWinDesktop.h"
#endif
#include "../assets/fonts/fa_solid_900.cpp"
#include "imgui/imgui_internal.h"
#include "nlohmann/json.hpp"
#include "IconsFontAwesome6.h"
#include "Frontend/Frontend.h"
#include "Backend/Backend.h"
#include "StyleManager.h"
#include <spdlog/spdlog.h>

Frontend &Frontend::Instance()
{
    static Frontend sl_Instance;
    return sl_Instance;
    // TODO: insert return statement here
}

int Frontend::init(float vFontSize, float vGlobalScale)
{
    initFonts(vFontSize * vGlobalScale);
    // 启动时视为活跃，避免首帧即进入低帧率模式
    m_lastActiveTime = std::chrono::steady_clock::now();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_IsSRGB;         // Application is SRGB-aware.
#ifdef __ANDROID__
    io.ConfigFlags |= ImGuiConfigFlags_IsTouchScreen;  // Application is using a touch screen instead of a mouse.
#else

#endif
    StyleManager::SelectTheme(StyleManager::MStyle_t::CLASSIC_STYLE);
    Backend::Instance().init();

    return 0;
}

void Frontend::update()
{
    ImGuiIO& io = ImGui::GetIO();
    // 检测本帧是否有用户交互（鼠标移动/按键/修饰键），刷新活跃时间戳
    bool active = (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f);
    for (int i = 0; i < 5 && !active; ++i) active = active || io.MouseDown[i];
    active = active || io.KeyCtrl || io.KeyShift || io.KeyAlt || io.KeySuper;
    if (active) m_lastActiveTime = std::chrono::steady_clock::now();

#if defined(__APPLE__)
    // macOS 毛玻璃:主内容窗口半透明,让底层 NSVisualEffectView 透出
    ImGuiStyle& stk = ImGui::GetStyle();
    const ImVec4& wbg = stk.Colors[ImGuiCol_WindowBg];
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(wbg.x, wbg.y, wbg.z, 0.3f));
#endif
    ImGui::Begin("h e l l o", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);
    // 铺满整个显示区域，左上角对齐
    ImGui::SetWindowPos(ImVec2(0, 0));
    ImGui::SetWindowSize(io.DisplaySize);
    
    // -- 海拔高度显示（4 模式 + 动效）--
    LocationProvider& loc = Backend::Instance().location();
    LocationData data = loc.lastKnown();
    LocationStatus st = loc.status();

    // 委托 AltitudeDisplay 绘制；返回 true 表示有按钮交互，刷新空闲计时
    if (m_altDisplay.render(data, st, loc, io.DisplaySize))
        m_lastActiveTime = std::chrono::steady_clock::now();

    ImGui::End();
#if defined(__APPLE__)
    ImGui::PopStyleColor();
#endif

}

const ImVec4 &Frontend::getClearColor()
{
    return clear_color;
}

bool Frontend::isIdle(float thresholdSec) const
{
    // 距上次交互超过阈值则视为空闲，供主循环动态降帧
    auto elapsed = std::chrono::duration_cast<std::chrono::duration<float>>(
        std::chrono::steady_clock::now() - m_lastActiveTime).count();
    return elapsed >= thresholdSec;
}

void Frontend::initFonts(float size_pixels)
{
    ImGuiIO& io = ImGui::GetIO();
    
    ImFontConfig font_config;
    font_config.PixelSnapH = true;
    
    static ImVector<ImWchar> ranges;
    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
    builder.AddRanges(io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    builder.AddRanges(io.Fonts->GetGlyphRangesChineseFull());
    // builder.AddRanges(io.Fonts->GetGlyphRangesJapanese());
    // builder.AddRanges(io.Fonts->GetGlyphRangesKorean());
    builder.AddText("你好世界开始设置帮助选择主题");
    builder.BuildRanges(&ranges);
    
    ImFont* font = nullptr;
    
    const char* font_paths[] = {
        "/System/Library/Fonts/STHeiti Light.ttc",
        "/System/Library/Fonts/STHeiti Medium.ttc",
        "/System/Library/Fonts/PingFang.ttc",
        // "/Users/kench/Documents/Imgui_AllInOne/Frontend/res/NotoSansSC-Regular.ttf",
        // "/Users/kench/Documents/codespace/mine/imgui/Imgui_CodingPlanUsage/assets/fonts/MapleMono-NF-CN-Regular.ttf",
        nullptr
    };
    
    for (int i = 0; font_paths[i] != nullptr; i++)
    {
        if (std::filesystem::exists(font_paths[i]))
        {
            font = io.Fonts->AddFontFromFileTTF(font_paths[i], 18.0f, &font_config, ranges.Data);
            if (font)
            {
                break;
            }
        }
    }
    
    if (!font)
    {
        io.Fonts->AddFontDefault();
    }

    float iconFontSize = size_pixels * 2.0f / 3.0f;
    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    icons_config.GlyphMinAdvanceX = iconFontSize;
    io.Fonts->AddFontFromMemoryCompressedTTF(fa_solid_900_compressed_data, fa_solid_900_compressed_size, iconFontSize, &icons_config, icons_ranges);
}
