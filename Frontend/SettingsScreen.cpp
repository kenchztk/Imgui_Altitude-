#include "Frontend/SettingsScreen.h"
#include "Frontend/UIHelpers.h"
#include "Frontend/Widgets/Segmented.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "IconsFontAwesome6.h"
#include "Frontend/AppSettings.h"
#include "Backend/LocationProvider.h"
#include "imgui_toggle.h"
#include "imgui_toggle_palette.h"
#include <spdlog/spdlog.h>

namespace {

// imgui_toggle 配色：开=青蓝、关=灰，旋钮=白（复刻原手绘 DrawToggle 观感）
static ImGuiTogglePalette gToggleOnPalette;
static ImGuiTogglePalette gToggleOffPalette;
static bool gTogglePalettesInit = []() {
    ImVec4 cyan(0.314f, 0.706f, 1.0f, 1.0f);   // (80,180,255)
    ImVec4 grey(0.314f, 0.353f, 0.412f, 1.0f); // (80,90,105)
    ImVec4 white(1.0f, 1.0f, 1.0f, 1.0f);
    gToggleOnPalette.Frame = gToggleOnPalette.FrameHover = cyan;
    gToggleOnPalette.Knob  = gToggleOnPalette.KnobHover  = white;
    gToggleOffPalette.Frame = gToggleOffPalette.FrameHover = grey;
    gToggleOffPalette.Knob  = gToggleOffPalette.KnobHover  = white;
    return true;
}();

// 在设置行右侧绘制 iOS 风格开关（替代手绘 DrawToggle）
void DrawSettingToggle(ImVec2 rowCp, const char* id, bool* val, bool& interacted,
                       float rowW, float rowH, float gap)
{
    ImGuiToggleConfig cfg;
    cfg.On.Palette  = &gToggleOnPalette;
    cfg.Off.Palette = &gToggleOffPalette;
    cfg.Size = ImVec2(44.0f, 26.0f);
    cfg.FrameRounding = 1.0f;
    cfg.KnobRounding  = 1.0f;
    ImGui::SetCursorScreenPos(ImVec2(rowCp.x + rowW - 14.0f - 44.0f, rowCp.y + (rowH - 26.0f) * 0.5f));
    if (ImGui::Toggle(id, val, cfg))
        interacted = true;
    ImGui::SetCursorScreenPos(ImVec2(rowCp.x, rowCp.y + rowH + gap));
}

} // namespace

bool SettingsScreen::render(const LocationData& /*data*/, LocationStatus /*st*/,
                            LocationProvider& /*loc*/, const ImVec2& /*displaySize*/)
{
    bool interacted = false;
    AppSettings& s = AppSettings::Instance();
    const ImGuiStyle& sty = ImGui::GetStyle();
    float rowW = ImGui::GetContentRegionAvail().x;
    float padX = sty.WindowPadding.x;
    float rowH = 52.0f;
    float gap = 10.0f;
    (void)padX;

    // 分段控件统一风格（与 NavBar / 指标切换共用 Segmented::Render）
    Segmented::Style segStyle;
    segStyle.bg  = IM_COL32(16, 22, 32, 255);
    segStyle.gap = 2.0f;

    auto drawLabel = [&](const char* txt) {
        ImVec2 cp = ImGui::GetCursorScreenPos();
        ImVec2 ts = ImGui::CalcTextSize(txt);
        ImGui::SetCursorScreenPos(ImVec2(cp.x + 14.0f, cp.y + rowH * 0.5f - ts.y * 0.5f));
        ImGui::TextColored(ImVec4(0.97f, 0.98f, 0.99f, 1.0f), "%s", txt);
    };

    // ===== 单位 =====
    {
        ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x,
            ImGui::GetCursorScreenPos().y));
        ImGui::TextColored(ImVec4(0.39f, 0.46f, 0.56f, 1.0f), "单位");
        ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x,
            ImGui::GetCursorScreenPos().y + 4.0f));

        // 海拔单位
        {
            ImVec2 cp = ImGui::GetCursorScreenPos();
            UI::DrawRoundedRect(cp, ImVec2(cp.x + rowW, cp.y + rowH), IM_COL32(21, 27, 39, 255), 12.0f);
            drawLabel("海拔单位");
            const char* opts[] = { "米", "英尺" };
            ImGui::SetCursorScreenPos(ImVec2(cp.x + rowW - 14.0f - 120.0f, cp.y + (rowH - 30.0f) * 0.5f));
            int idx = (s.length == AppSettings::LengthUnit::Foot) ? 1 : 0;
            int newIdx = Segmented::Render(opts, 2, idx, 120.0f, 30.0f, segStyle);
            if (newIdx != idx) { s.length = (newIdx == 1) ? AppSettings::LengthUnit::Foot : AppSettings::LengthUnit::Meter; interacted = true; }
            ImGui::SetCursorScreenPos(ImVec2(cp.x, cp.y + rowH + gap));
        }
        // 气压单位
        {
            ImVec2 cp = ImGui::GetCursorScreenPos();
            UI::DrawRoundedRect(cp, ImVec2(cp.x + rowW, cp.y + rowH), IM_COL32(21, 27, 39, 255), 12.0f);
            drawLabel("气压单位");
            const char* opts[] = { "hPa", "mmHg" };
            ImGui::SetCursorScreenPos(ImVec2(cp.x + rowW - 14.0f - 130.0f, cp.y + (rowH - 30.0f) * 0.5f));
            int idx = (s.pressure == AppSettings::PressureUnit::mmHg) ? 1 : 0;
            int newIdx = Segmented::Render(opts, 2, idx, 130.0f, 30.0f, segStyle);
            if (newIdx != idx) { s.pressure = (newIdx == 1) ? AppSettings::PressureUnit::mmHg : AppSettings::PressureUnit::hPa; interacted = true; }
            ImGui::SetCursorScreenPos(ImVec2(cp.x, cp.y + rowH + gap));
        }
        // 温度单位
        {
            ImVec2 cp = ImGui::GetCursorScreenPos();
            UI::DrawRoundedRect(cp, ImVec2(cp.x + rowW, cp.y + rowH), IM_COL32(21, 27, 39, 255), 12.0f);
            drawLabel("温度单位");
            const char* opts[] = { "°C", "°F" };
            ImGui::SetCursorScreenPos(ImVec2(cp.x + rowW - 14.0f - 120.0f, cp.y + (rowH - 30.0f) * 0.5f));
            int idx = (s.temp == AppSettings::TempUnit::Fahrenheit) ? 1 : 0;
            int newIdx = Segmented::Render(opts, 2, idx, 120.0f, 30.0f, segStyle);
            if (newIdx != idx) { s.temp = (newIdx == 1) ? AppSettings::TempUnit::Fahrenheit : AppSettings::TempUnit::Celsius; interacted = true; }
            ImGui::SetCursorScreenPos(ImVec2(cp.x, cp.y + rowH + gap));
        }
    }

    // ===== 定位 =====
    {
        ImGui::TextColored(ImVec4(0.39f, 0.46f, 0.56f, 1.0f), "定位");
        ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y + 4.0f));
        {
            ImVec2 cp = ImGui::GetCursorScreenPos();
            UI::DrawRoundedRect(cp, ImVec2(cp.x + rowW, cp.y + rowH), IM_COL32(21, 27, 39, 255), 12.0f);
            drawLabel("自动校准");
            DrawSettingToggle(cp, "##autoCalibrate", &s.autoCalibrate, interacted, rowW, rowH, gap);
        }
        {
            ImVec2 cp = ImGui::GetCursorScreenPos();
            UI::DrawRoundedRect(cp, ImVec2(cp.x + rowW, cp.y + rowH), IM_COL32(21, 27, 39, 255), 12.0f);
            drawLabel("高精度模式");
            DrawSettingToggle(cp, "##highPrecision", &s.highPrecision, interacted, rowW, rowH, gap);
        }
        {
            ImVec2 cp = ImGui::GetCursorScreenPos();
            UI::DrawRoundedRect(cp, ImVec2(cp.x + rowW, cp.y + rowH), IM_COL32(21, 27, 39, 255), 12.0f);
            drawLabel("EGM96 大地水准面修正");
            DrawSettingToggle(cp, "##egm96", &s.egm96, interacted, rowW, rowH, gap);
        }
    }

    // ===== 显示 =====
    {
        ImGui::TextColored(ImVec4(0.39f, 0.46f, 0.56f, 1.0f), "显示");
        ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y + 4.0f));
        {
            ImVec2 cp = ImGui::GetCursorScreenPos();
            UI::DrawRoundedRect(cp, ImVec2(cp.x + rowW, cp.y + rowH), IM_COL32(21, 27, 39, 255), 12.0f);
            drawLabel("主题");
            const char* opts[] = { "浅色", "深色", "跟随系统" };
            ImGui::SetCursorScreenPos(ImVec2(cp.x + rowW - 14.0f - 210.0f, cp.y + (rowH - 30.0f) * 0.5f));
            int newIdx = Segmented::Render(opts, 3, s.theme, 210.0f, 30.0f, segStyle);
            if (newIdx != s.theme) { s.theme = newIdx; interacted = true; }
            ImGui::SetCursorScreenPos(ImVec2(cp.x, cp.y + rowH + gap));
        }
        {
            ImVec2 cp = ImGui::GetCursorScreenPos();
            UI::DrawRoundedRect(cp, ImVec2(cp.x + rowW, cp.y + rowH), IM_COL32(21, 27, 39, 255), 12.0f);
            drawLabel("毛玻璃背景");
            DrawSettingToggle(cp, "##frosted", &s.frosted, interacted, rowW, rowH, gap);
        }
    }

    // ===== 关于 =====
    {
        ImGui::TextColored(ImVec4(0.39f, 0.46f, 0.56f, 1.0f), "关于");
        ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y + 4.0f));

        auto staticRow = [&](const char* label, const char* value) {
            ImVec2 cp = ImGui::GetCursorScreenPos();
            UI::DrawRoundedRect(cp, ImVec2(cp.x + rowW, cp.y + rowH), IM_COL32(21, 27, 39, 255), 12.0f);
            ImVec2 ts = ImGui::CalcTextSize(label);
            ImGui::SetCursorScreenPos(ImVec2(cp.x + 14.0f, cp.y + rowH * 0.5f - ts.y * 0.5f));
            ImGui::TextColored(ImVec4(0.97f, 0.98f, 0.99f, 1.0f), "%s", label);
            ImVec2 vs = ImGui::CalcTextSize(value);
            ImGui::SetCursorScreenPos(ImVec2(cp.x + rowW - 14.0f - vs.x, cp.y + rowH * 0.5f - vs.y * 0.5f));
            ImGui::TextColored(ImVec4(0.39f, 0.46f, 0.56f, 1.0f), "%s", value);
            ImGui::SetCursorScreenPos(ImVec2(cp.x, cp.y + rowH + gap));
        };
        staticRow("版本", "v1.0.0");
        staticRow("开源许可", "ImGui · spdlog · EGM96");
        staticRow("数据源", "EGM96 球谐系数");
    }

    // 末尾补一个 0 尺寸占位项：消除“用 SetCursorPos 扩展父窗口边界”断言，并使 ##body 内容高度正确（滚动范围正确）
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
    return interacted;
}
