#ifdef _WIN32
#  include "platform/win/Main.h"
#endif
#include "../assets/fonts/fa_solid_900.cpp"
#include "imgui/imgui_internal.h"
#include "nlohmann/json.hpp"
#include "IconsFontAwesome6.h"
#include "Frontend/Frontend.h"
#include "Backend/Backend.h"
#include "Frontend/StyleManager.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <string>
#include <cmath>
#include <cmath>

#if defined(__ANDROID__)
// mainAndroid.cpp 提供：从 APK assets 读取资源到内存（IM_ALLOC 分配，ImGui 接管释放）
extern int AndroidGetAssetData(const char* filename, void** outData);
// LocationProviderAndroid.cpp 提供：Android 安全区（圆角/挖孔）内边距，单位与 io.DisplaySize 一致
extern void AndroidGetSafeInsets(float& top, float& right, float& bottom, float& left);
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

    // 相框尺寸常量（仅 Android 生效；其余平台为 0，保持原铺满行为）
#ifdef __ANDROID__
    const float kFrameEdge = 10.0f;       // 相框边缘厚度（绘制在窗口边框上）
    const float kFrameRounding = 26.0f;   // 相框圆角半径
#else
    const float kFrameEdge = 0.0f;
    const float kFrameRounding = 0.0f;
#endif

    // Android 安全区：改用固定、克制的安全边距，避免依赖 ROM 圆角半径（常被报得过大）
#ifdef __ANDROID__
    // 固定四边内边距：把主体收进“安全矩形”，并叠加【相框 + 景深】视觉
    float margin = ImMin(io.DisplaySize.x, io.DisplaySize.y) * 0.045f;
    margin = ImClamp(margin, 28.0f, 64.0f);
    ImVec2 frameMin(margin, margin);
    ImVec2 frameMax(io.DisplaySize.x - margin, io.DisplaySize.y - margin);

    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    // 1) 墙面：上深蓝灰 -> 下近黑的纵向渐变（相框背后的“墙”）
    bg->AddRectFilledMultiColor(ImVec2(0.0f, 0.0f), io.DisplaySize,
        IM_COL32(20, 26, 36, 255), IM_COL32(20, 26, 36, 255),
        IM_COL32(7, 9, 13, 255),   IM_COL32(7, 9, 13, 255));
    // 2) 暗角 vignette：边缘压暗、中心保留，强化景深（随时间轻微漂移，产生视差）
    {
        float t = (float)ImGui::GetTime();
        float cx = io.DisplaySize.x * 0.5f + sinf(t * 0.25f) * 10.0f;
        float cy = io.DisplaySize.y * 0.5f + cosf(t * 0.21f) * 10.0f;
        float baseR = 0.58f * sqrtf(io.DisplaySize.x * io.DisplaySize.x + io.DisplaySize.y * io.DisplaySize.y);
        const int steps = 6;
        for (int i = steps; i >= 1; --i)
        {
            // f 越大越靠外、alpha 越高 -> 边角更暗，形成暗角
            float f = (float)i / (float)steps;
            bg->AddCircleFilled(ImVec2(cx, cy), baseR * f, IM_COL32(0, 0, 0, (int)(20.0f * f)), 64);
        }
    }
    // 3) 相框柔和投影：多层渐隐外扩，模拟光源自上而下的悬浮景深
    for (int i = 7; i >= 1; --i)
    {
        float e = (float)i * 3.0f;
        float a = (float)(8 - i) * 5.0f;
        float offY = e * 0.5f + 4.0f;
        bg->AddRectFilled(ImVec2(frameMin.x - e, frameMin.y - e * 0.5f),
                          ImVec2(frameMax.x + e, frameMax.y + e + offY),
                          IM_COL32(0, 0, 0, (int)ImClamp(a, 0.0f, 60.0f)),
                          kFrameRounding + e, 0);
    }
#endif
    // 检测本帧是否有用户交互（鼠标移动/按键/修饰键），刷新活跃时间戳
    bool active = (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f);
    for (int i = 0; i < 5 && !active; ++i) active = active || io.MouseDown[i];
    active = active || io.KeyCtrl || io.KeyShift || io.KeyAlt || io.KeySuper;
    if (active) m_lastActiveTime = std::chrono::steady_clock::now();

    // 根窗口 = 相框内芯：圆角 + 画框边缘 + 内边距（让控件自然收敛、留出“画框”呼吸感）
    // macOS 保持半透明毛玻璃；Android 改为不透明画框画布
#if defined(__APPLE__)
    ImGuiStyle& stk = ImGui::GetStyle();
    const ImVec4& wbg = stk.Colors[ImGuiCol_WindowBg];
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(wbg.x, wbg.y, wbg.z, 0.3f));
#elif defined(__ANDROID__)
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.063f, 0.078f, 0.110f, 1.0f)); // ~ (16,20,28) 画布
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.43f, 0.46f, 0.52f, 1.0f));     // ~ (110,118,132) 画框边缘
#endif
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, kFrameEdge);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   kFrameRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(16.0f, 16.0f));
    ImGui::Begin("h e l l o", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);

    // 定位：Android 收进相框安全矩形；其余平台铺满整屏
#ifdef __ANDROID__
    ImGui::SetWindowPos(frameMin);
    ImGui::SetWindowSize(ImVec2(frameMax.x - frameMin.x, frameMax.y - frameMin.y));
    // 内芯顶部微光（画布受光感，强化景深）
    {
        ImDrawList* wdl = ImGui::GetWindowDrawList();
        ImVec2 p0 = ImGui::GetWindowPos();
        ImVec2 wsz = ImGui::GetWindowSize();
        ImVec2 p1 = ImVec2(p0.x + wsz.x, p0.y + wsz.y);
        wdl->AddRectFilledMultiColor(p0, p1,
            IM_COL32(255, 255, 255, 16), IM_COL32(255, 255, 255, 16),
            IM_COL32(255, 255, 255, 0),  IM_COL32(255, 255, 255, 0));
    }
#else
    ImGui::SetWindowPos(ImVec2(0, 0));
    ImGui::SetWindowSize(io.DisplaySize);
#endif

    // -- 海拔高度显示（4 模式 + 动效）--
    LocationProvider& loc = Backend::Instance().location();
    LocationData data = loc.lastKnown();
    LocationStatus st = loc.status();

    // 委托 AltitudeDisplay 绘制；返回 true 表示有按钮交互，刷新空闲计时
    if (m_altDisplay.render(data, st, loc, io.DisplaySize))
        m_lastActiveTime = std::chrono::steady_clock::now();

    ImGui::End();
    ImGui::PopStyleVar(3);   // WindowBorderSize + WindowRounding + WindowPadding
#if defined(__APPLE__)
    ImGui::PopStyleColor(1); // 仅 WindowBg
#elif defined(__ANDROID__)
    ImGui::PopStyleColor(2); // WindowBg + Border
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
