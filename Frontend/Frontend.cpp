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

static const auto FLAGS = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_UnsavedDocument;

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
    static float session_usage = 0.0f, monthly_usage = 0.0f, weekly_usage = 0.0f;

    ImGuiIO& io = ImGui::GetIO();
    // 检测本帧是否有用户交互（鼠标移动/按键/修饰键），刷新活跃时间戳
    bool active = (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f);
    for (int i = 0; i < 5 && !active; ++i) active = active || io.MouseDown[i];
    active = active || io.KeyCtrl || io.KeyShift || io.KeyAlt || io.KeySuper;
    if (active) m_lastActiveTime = std::chrono::steady_clock::now();

    // 仅到刷新周期才发起请求；查询走缓存，避免每帧加锁遍历 JSON
    Backend::Instance().tryRefreshUsage();
    Backend::Instance().getCachedUsage(session_usage, weekly_usage, monthly_usage);

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
    

    ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x - 240.0f, 0));
    ImGui::SameLine();
    ImGui::Button( ICON_FA_GEAR " 设置" );
    ImGui::SameLine();
    ImGui::Button( ICON_FA_CIRCLE_USER " 登录" );
    ImGui::SameLine();
    ImGui::Button( ICON_FA_POWER_OFF " 退出" ); 

    ImGui::Separator();

    // 浏览器入口：点击后在独立 WKWebView 窗口中加载目标 URL。
    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    if (ImGui::Button("Open Baidu"))
    {
    }
    ImGui::SameLine();
    if (ImGui::Button("Close WebView"))
    {
    }

    // 自动化点击演示：打开页面 -> 等待加载 -> 轮询查找元素 -> 点击 -> 等待跳转 -> 回调。
    // selector 需用 Safari 开发者工具（右键检查元素）确认实际值后替换 PUT_REAL_SELECTOR_HERE。
    if (ImGui::Button("Open & Click"))
    {

    }
    ImGui::SameLine();

    // Cookies 持久化：把当前 WKWebView 关联的 cookies 导出到本地 JSON，
    // 或从本地 JSON 恢复到当前 WKWebView（自动 reload）。
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    if (ImGui::Button("Save Cookies"))
    {
        // WebViewPanel::Instance().exportCookies();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Cookies"))
    {
        // WebViewPanel::Instance().importCookies();
    }
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
