#ifdef _WIN32
#  include "platform/win/Main.h"
#endif
#include "../assets/fonts/fa_solid_900.cpp"
#include "imgui/imgui_internal.h"
#include "implot.h"
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

namespace {

// 每屏顶部 header（标题 / 位置 / 设置入口）。screen 传引用，齿轮可跳转到设置屏。
void DrawHeader(Screen& screen, const LocationData& data)
{
    ImVec2 o = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;

    auto text = [](const char* label, float scale, const ImVec4& col, float x, float y) {
        ImGui::SetWindowFontScale(scale);
        ImGui::SetCursorScreenPos(ImVec2(x, y));
        ImGui::TextColored(col, "%s", label);
        ImGui::SetWindowFontScale(1.0f);
    };

    if (screen == Screen::Realtime)
    {
        text("当前位置", 0.8f, ImVec4(0.39f, 0.46f, 0.56f, 1.0f), o.x, o.y + 2.0f);
        const char* val = data.valid ? "玉龙雪山 · 东麓" : "等待定位…";
        text(val, 1.0f, ImVec4(0.97f, 0.98f, 0.99f, 1.0f), o.x, o.y + 18.0f);
        // 右上角齿轮 -> 设置
        ImGui::SetCursorScreenPos(ImVec2(o.x + w - 32.0f, o.y));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.08f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.14f));
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100, 116, 139, 255));
        if (ImGui::Button(ICON_FA_GEAR, ImVec2(32, 32)))
            screen = Screen::Settings;
        ImGui::PopStyleColor(4);
    }
    else if (screen == Screen::Records)
    {
        text("运动记录", 1.6f, ImVec4(0.97f, 0.98f, 0.99f, 1.0f), o.x, o.y);
        text("本月 3 条轨迹", 0.8f, ImVec4(0.39f, 0.46f, 0.56f, 1.0f), o.x, o.y + 26.0f);
    }
    else if (screen == Screen::Map)
    {
        text("地图", 1.6f, ImVec4(0.97f, 0.98f, 0.99f, 1.0f), o.x, o.y);
        text("玉龙雪山 · 东麓", 0.8f, ImVec4(0.39f, 0.46f, 0.56f, 1.0f), o.x, o.y + 26.0f);
    }
    else // Settings
    {
        text("设置", 1.6f, ImVec4(0.97f, 0.98f, 0.99f, 1.0f), o.x, o.y);
        text("单位 · 定位 · 显示", 0.8f, ImVec4(0.39f, 0.46f, 0.56f, 1.0f), o.x, o.y + 26.0f);
    }
}

} // namespace

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

    // ImPlot（趋势图）上下文：必须与 ImGui 共享同一 ImGuiContext。
    // imgui 编为独立 libimgui.{dylib,so} 共享库，跨模块需显式指定，否则 implot 取不到上下文。
    ImPlot::CreateContext();
    ImPlot::SetImGuiContext(ImGui::GetCurrentContext());

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

    // 根窗口铺满整屏：macOS 半透明毛玻璃；Android 不透明深色画布；不做任何边框/景深装饰
#if defined(__APPLE__)
    ImGuiStyle& stk = ImGui::GetStyle();
    const ImVec4& wbg = stk.Colors[ImGuiCol_WindowBg];
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(wbg.x, wbg.y, wbg.z, 0.3f));
#elif defined(__ANDROID__)
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.063f, 0.078f, 0.110f, 1.0f)); // ~ (16,20,28)
#endif
    // 根窗口留白：Android 走安全区+零边距铺满，四周不留 padding；macOS 保留 16px 呼吸留白
    const ImVec2 kRootPad = ImVec2(
#if defined(__ANDROID__)
        0.0f, 0.0f
#else
        16.0f, 16.0f
#endif
    );
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    kRootPad);
    ImGui::Begin("h e l l o", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);

    // 定位：Android 仅顶部按系统安全区避让状态栏/挖孔，左右与底部彻底零边距铺满；其余平台铺满整屏
#ifdef __ANDROID__
    float safeTop = 0.0f, safeRight = 0.0f, safeBottom = 0.0f, safeLeft = 0.0f;
    AndroidGetSafeInsets(safeTop, safeRight, safeBottom, safeLeft);
    ImGui::SetWindowPos(ImVec2(0.0f, safeTop));
    ImGui::SetWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y - safeTop));
#else
    ImGui::SetWindowPos(ImVec2(0, 0));
    ImGui::SetWindowSize(io.DisplaySize);
#endif

    // 数据取融合结果（GPS+EGM 绝对基准 ⊕ 气压计相对变化）；无气压计时等同纯 GPS
    LocationProvider& loc = Backend::Instance().location();
    LocationData data = Backend::Instance().currentFused();
    LocationStatus st = loc.status();

    // 子区域不再叠加 WindowPadding（padding 已由根窗口统一控制），仅按内容自适应高度
    const ImGuiChildFlags kFlagsAuto = ImGuiChildFlags_AutoResizeY;

    // 顶部每屏 header
    ImGui::BeginChild("##header", ImVec2(0, 0), kFlagsAuto);
    DrawHeader(m_screen, data);
    ImGui::EndChild();

    // 主体内容区（弹性填满剩余；记录/设置内容超长时可滚动）
    ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImGuiStyle& sty = ImGui::GetStyle();
    float tabEst = 56.0f + sty.WindowPadding.y * 2.0f + sty.ItemSpacing.y;
    float bodyH = ImMax(avail.y - tabEst, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("##body", ImVec2(0, bodyH), ImGuiChildFlags_None);
    {
        bool interacted = false;
        switch (m_screen)
        {
            case Screen::Realtime: interacted = m_realtime.render(data, st, loc, io.DisplaySize); break;
            case Screen::Records:  interacted = m_records.render(data, st, loc, io.DisplaySize); break;
            case Screen::Map:      interacted = m_map.render(data, st, loc, io.DisplaySize); break;
            case Screen::Settings: interacted = m_settings.render(data, st, loc, io.DisplaySize); break;
        }
        if (interacted)
            m_lastActiveTime = std::chrono::steady_clock::now();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();

    // 底部导航（pill）
    ImGui::BeginChild("##tabbar", ImVec2(0, 0), kFlagsAuto);
    {
        Screen clicked = NavBar::render(m_screen);
        if (clicked != m_screen)
        {
            m_screen = clicked;
            m_lastActiveTime = std::chrono::steady_clock::now();
        }
    }
    ImGui::EndChild();

    ImGui::End();
    ImGui::PopStyleVar(3);   // WindowBorderSize + WindowRounding + WindowPadding
    ImGui::PopStyleColor(1); // WindowBg

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
