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
#include <filesystem>
#include <string>

#if defined(__ANDROID__)
// mainAndroid.cpp 提供：从 APK assets 读取资源到内存（IM_ALLOC 分配，ImGui 接管释放）
extern int AndroidGetAssetData(const char* filename, void** outData);
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
// 解析 .app bundle Resources 下资源的绝对路径；裸二进制运行（无 bundle）时返回空
static std::string MacBundleResourcePath(const char* relative)
{
    CFBundleRef bundle = CFBundleGetMainBundle();
    if (!bundle)
        return {};
    CFURLRef url = CFBundleCopyResourcesDirectoryURL(bundle);
    if (!url)
        return {};
    std::string result;
    char buf[1024];
    if (CFURLGetFileSystemRepresentation(url, true, reinterpret_cast<UInt8*>(buf), sizeof(buf)))
        result = std::string(buf) + "/" + relative;
    CFRelease(url);
    return result;
}
#endif

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
    // builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
    builder.AddRanges(io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    // builder.AddRanges(io.Fonts->GetGlyphRangesChineseFull());
    // builder.AddRanges(io.Fonts->GetGlyphRangesJapanese());
    // builder.AddRanges(io.Fonts->GetGlyphRangesKorean());
    builder.AddText("你好世界开始设置帮助选择主题");
    builder.BuildRanges(&ranges);
    
    ImFont* font = nullptr;

    // -- 主字体：随应用分发的 MapleMono，按平台从不同位置加载 --
    static const char* kFontRelPath = "fonts/MapleMono-NF-CN-Regular.ttf";
#if defined(__ANDROID__)
    // Android：字体打包在 APK assets/fonts/ 内，经 AAssetManager 读入内存加载
    void* font_data = nullptr;
    int font_data_size = AndroidGetAssetData(kFontRelPath, &font_data);
    if (font_data_size > 0)
        font = io.Fonts->AddFontFromMemoryTTF(font_data, font_data_size, size_pixels, &font_config, ranges.Data);
    if (!font)
        spdlog::error("[Font] 加载 assets/{} 失败", kFontRelPath);
#elif defined(__APPLE__)
    // macOS：字体位于 .app/Contents/Resources/fonts/；裸二进制运行时回退到源码目录
    std::string font_path = MacBundleResourcePath(kFontRelPath);
    if (font_path.empty() || !std::filesystem::exists(font_path))
        font_path = std::string("assets/") + kFontRelPath;
    if (std::filesystem::exists(font_path))
        font = io.Fonts->AddFontFromFileTTF(font_path.c_str(), size_pixels, &font_config, ranges.Data);
    if (!font)
        spdlog::error("[Font] 加载 {} 失败", font_path);
#elif defined(_WIN32)
    // Windows：字体随构建拷贝到可执行文件旁的 fonts/ 目录
    if (std::filesystem::exists(kFontRelPath))
        font = io.Fonts->AddFontFromFileTTF(kFontRelPath, size_pixels, &font_config, ranges.Data);
    if (!font)
        spdlog::error("[Font] 加载 {} 失败", kFontRelPath);
#endif

#if defined(__APPLE__)
    // 回退：macOS 系统中文字体
    if (!font)
    {
        const char* sys_font = "/System/Library/Fonts/STHeiti Light.ttc";
        if (std::filesystem::exists(sys_font))
            font = io.Fonts->AddFontFromFileTTF(sys_font, size_pixels, &font_config, ranges.Data);
    }
#endif
    if (!font)
        io.Fonts->AddFontDefault();

    float iconFontSize = size_pixels * 2.0f / 3.0f;
    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    icons_config.GlyphMinAdvanceX = iconFontSize;
    io.Fonts->AddFontFromMemoryCompressedTTF(fa_solid_900_compressed_data, fa_solid_900_compressed_size, iconFontSize, &icons_config, icons_ranges);
}
