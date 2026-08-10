#include "Frontend/RealtimeScreen.h"
#include "Frontend/UIHelpers.h"
#include "Frontend/Widgets/Segmented.h"
#include "Frontend/Widgets/StatusBadge.h"
#include "Frontend/Widgets/RadialGauge.h"
#include "Frontend/Widgets/StatCard.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "implot.h"
#include "IconsFontAwesome6.h"
#include "Frontend/AppSettings.h"
#include "Backend/LocationProvider.h"
#include <spdlog/spdlog.h>
#include <cmath>
#include <cstdio>
#include <cfloat>
#include <vector>
#include <algorithm>

// ---- 颜色常量 ----

ImU32 RealtimeScreen::statusColor(LocationStatus st)
{
    switch (st)
    {
        case LocationStatus::Idle:     return IM_COL32(204, 204, 204, 255);
        case LocationStatus::Starting: return IM_COL32(255, 204, 51, 255);
        case LocationStatus::Active:   return IM_COL32(77, 230, 102, 255);
        case LocationStatus::Denied:   return IM_COL32(242, 77, 77, 255);
        default:                       return IM_COL32(242, 77, 77, 255);
    }
}

const char* RealtimeScreen::statusText(LocationStatus st)
{
    switch (st)
    {
        case LocationStatus::Idle:     return ICON_FA_POWER_OFF "  未开始";
        case LocationStatus::Starting: return ICON_FA_ARROWS_ROTATE "  等待定位…";
        case LocationStatus::Active:   return ICON_FA_LOCATION_ARROW "  定位中";
        case LocationStatus::Denied:   return ICON_FA_BAN "  权限被拒绝";
        default:                       return ICON_FA_TRIANGLE_EXCLAMATION "  错误";
    }
}

// ---- 数字平滑插值 ----

void RealtimeScreen::updateDisplayedAltitude(float targetAlt)
{
    if (!m_altInit) { m_displayedAlt = targetAlt; m_altInit = true; return; }
    m_displayedAlt += (targetAlt - m_displayedAlt) * 0.15f;
    if (std::fabs(targetAlt - m_displayedAlt) < 0.05f)
        m_displayedAlt = targetAlt;
}

// ---- 海拔历史采样（趋势图）----

void RealtimeScreen::sampleHistory(float alt)
{
    double t = ImGui::GetTime();
    if (t - m_lastSampleT < 0.5) return;   // 约 1Hz
    m_lastSampleT = t;
    m_altHistory.push_back(alt);
    if (m_altHistory.size() > 120)
        m_altHistory.pop_front();
}

// ---- 主入口 ----

bool RealtimeScreen::render(const LocationData& data, LocationStatus st,
                            LocationProvider& loc, const ImVec2& displaySize)
{
    (void)displaySize;
    updateDisplayedAltitude(data.valid ? (float)data.fusedAltitude : m_displayedAlt);
    if (data.valid)
        sampleHistory(m_displayedAlt);

    bool interacted = false;

#if defined(__APPLE__) || defined(__ANDROID__)
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
#endif

    const ImGuiChildFlags kFlagsPadded = ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding;

    // 第1段：指标切换（海拔/气压/温度/趋势）
    ImGui::BeginChild("##metric_switcher", ImVec2(0, 0), kFlagsPadded);
    interacted |= renderMetricSwitcher();
    ImGui::EndChild();

    // 第2段：主数字区（弹性填满剩余）
    float h3 = estimateControlsHeight(data);
    float h2 = estimateSecondaryHeight();
    const ImGuiStyle& sty = ImGui::GetStyle();
    float interGaps = sty.ItemSpacing.y * 3.0f;
    float reserveBottom = h2 + h3 + interGaps + 16.0f;
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float middleH = ImMax(avail.y - reserveBottom, 0.0f);
    ImGui::BeginChild("##hero", ImVec2(0, middleH), ImGuiChildFlags_None);
    {
        ImVec2 subAvail = ImGui::GetContentRegionAvail();
        renderHero(data, st, subAvail.x, subAvail.y);
    }
    ImGui::EndChild();

    // 第3段：副信息条（气压 + 垂直速率）
    ImGui::BeginChild("##secondary", ImVec2(0, 0), kFlagsPadded);
    renderSecondaryInfo(data, st);
    ImGui::EndChild();

    // 第4段：控制区（详情 + 开始/停止）
    ImGui::BeginChild("##controls", ImVec2(0, 0), kFlagsPadded);
    interacted |= renderControls(data, st, loc);
    ImGui::EndChild();

#if defined(__APPLE__) || defined(__ANDROID__)
    ImGui::PopStyleColor();
#endif
    return interacted;
}

// ---- 指标切换分段控件（4 段）----

bool RealtimeScreen::renderMetricSwitcher()
{
    bool interacted = false;
    const Metric metrics[] = { Metric::Altitude, Metric::Pressure, Metric::Temperature, Metric::Trend };
    const char* labels[] = { "海拔", "气压", "温度", "趋势" };
    constexpr int kCount = 4;

    float availW = ImGui::GetContentRegionAvail().x;
    float segH = ImGui::GetFrameHeight() + 6.0f;

    Segmented::Style st;
    st.gap = 4.0f;
    st.hover = true;   // 未选中段 hover 时叠一层白色（保持原观感）
    int sel = Segmented::Render(labels, kCount, (int)m_metric, availW, segH, st);
    if (sel != (int)m_metric)
    {
        m_metric = metrics[sel];
        interacted = true;
        spdlog::info("[UI] 切换指标 -> {}", (int)m_metric);
    }

    ImGui::Spacing();
    return interacted;
}

// ---- 主数字区分派 ----

void RealtimeScreen::renderHero(const LocationData& data, LocationStatus st, float availW, float availH)
{
    switch (m_metric)
    {
        case Metric::Altitude:    renderAltitudeHero(data, st, availW, availH); break;
        case Metric::Pressure:    renderPressureHero(data, st, availW, availH); break;
        case Metric::Temperature: renderTemperatureHero(data, st, availW, availH); break;
        case Metric::Trend:       renderTrendHero(data, st, availW, availH); break;
    }
}

// ---- 海拔：圆环进度 + 大数字 ----

void RealtimeScreen::renderAltitudeHero(const LocationData& data, LocationStatus st,
                                        float availW, float availH)
{
    ImVec2 wp = ImGui::GetWindowPos();
    AppSettings& s = AppSettings::Instance();

    ImVec2 center(wp.x + availW * 0.5f, wp.y + availH * 0.46f);
    float R = ImMin(availW, availH) * 0.40f;

    // 计算进度比例（仅有效数据时绘制弧）
    float ratio = 0.0f;
    if (data.valid)
    {
        float maxRange = 3000.0f;
        if (m_displayedAlt > maxRange * 0.85f)
            maxRange = std::ceil(m_displayedAlt / 1000.0f) * 1000.0f + 1000.0f;
        ratio = ImClamp(m_displayedAlt / maxRange, 0.0f, 1.0f);
    }

    char valBuf[32];
    if (data.valid)
        snprintf(valBuf, sizeof(valBuf), "%.0f", s.displayLength(m_displayedAlt));
    else
        snprintf(valBuf, sizeof(valBuf), "--");
    char unitBuf[16];
    snprintf(unitBuf, sizeof(unitBuf), "%s (MSL)", s.lengthUnit());

    RadialGauge::Render(center, R, ratio, "当前海拔", valBuf, unitBuf,
                        UI::accentCyan(), data.valid);

    renderStatusBadge(data, st, availW * 0.5f, center.y + R + 28.0f - wp.y);
}

// ---- 气压：大数字 ----

void RealtimeScreen::renderPressureHero(const LocationData& data, LocationStatus st,
                                        float availW, float availH)
{
    ImVec2 wp = ImGui::GetWindowPos();
    AppSettings& s = AppSettings::Instance();

    ImVec2 center(wp.x + availW * 0.5f, wp.y + availH * 0.46f);
    float R = ImMin(availW, availH) * 0.40f;
    UI::DrawRing(center, R, IM_COL32(80, 180, 255, 24), 1.5f, 80);

    UI::CenteredText("当前气压", 1.4f, ImVec4(0.39f, 0.46f, 0.56f, 1.0f), availW, center.y - R * 0.42f - wp.y);

    char valBuf[32];
    if (data.hasPressure || (data.valid && data.pressureHPa > 0.0))
        snprintf(valBuf, sizeof(valBuf), "%.1f", s.displayPressure(data.pressureHPa));
    else
        snprintf(valBuf, sizeof(valBuf), "--");
    float valY = center.y - R * 0.10f;
    UI::CenteredText(valBuf, 5.5f, ImVec4(0.97f, 0.98f, 0.99f, 1.0f), availW, valY - wp.y);

    char unitBuf[16];
    snprintf(unitBuf, sizeof(unitBuf), "%s", s.pressureUnit());
    float meterY = valY + 5.5f * 18.0f * 0.5f + 6.0f;
    UI::CenteredText(unitBuf, 1.6f, ImVec4(0.39f, 0.46f, 0.56f, 1.0f), availW, meterY - wp.y);

    UI::CenteredText("相对海平面参考", 1.1f, ImVec4(0.39f, 0.46f, 0.56f, 1.0f), availW,
                 center.y + R * 0.45f - wp.y);

    renderStatusBadge(data, st, availW * 0.5f, center.y + R + 28.0f - wp.y);
}

// ---- 温度：大数字（按海拔直减率合成演示值）----

void RealtimeScreen::renderTemperatureHero(const LocationData& data, LocationStatus st,
                                           float availW, float availH)
{
    ImVec2 wp = ImGui::GetWindowPos();
    AppSettings& s = AppSettings::Instance();

    ImVec2 center(wp.x + availW * 0.5f, wp.y + availH * 0.46f);
    float R = ImMin(availW, availH) * 0.40f;
    UI::DrawRing(center, R, IM_COL32(80, 180, 255, 24), 1.5f, 80);

    UI::CenteredText("当前温度", 1.4f, ImVec4(0.39f, 0.46f, 0.56f, 1.0f), availW, center.y - R * 0.42f - wp.y);

    // 环境温度估算：标准 15°C 起，每千米下降 6.5°C；活跃时叠加轻微抖动
    char valBuf[32];
    if (data.valid)
    {
        double tempC = 15.0 - 6.5 * (m_displayedAlt / 1000.0);
        tempC += 0.4 * sinf((float)ImGui::GetTime() * 0.7f);
        snprintf(valBuf, sizeof(valBuf), "%.1f", s.displayTemp(tempC));
    }
    else
        snprintf(valBuf, sizeof(valBuf), "--");
    float valY = center.y - R * 0.10f;
    UI::CenteredText(valBuf, 5.5f, ImVec4(0.97f, 0.98f, 0.99f, 1.0f), availW, valY - wp.y);

    char unitBuf[16];
    snprintf(unitBuf, sizeof(unitBuf), "%s", s.tempUnit());
    float meterY = valY + 5.5f * 18.0f * 0.5f + 6.0f;
    UI::CenteredText(unitBuf, 1.6f, ImVec4(0.39f, 0.46f, 0.56f, 1.0f), availW, meterY - wp.y);

    UI::CenteredText("环境估算", 1.1f, ImVec4(0.39f, 0.46f, 0.56f, 1.0f), availW,
                 center.y + R * 0.45f - wp.y);

    renderStatusBadge(data, st, availW * 0.5f, center.y + R + 28.0f - wp.y);
}

// ---- 趋势：海拔历史折线图 ----

void RealtimeScreen::renderTrendHero(const LocationData& data, LocationStatus st,
                                     float availW, float availH)
{
    (void)data; (void)st;
    UI::CenteredText("海拔趋势", 1.4f, ImVec4(0.39f, 0.46f, 0.56f, 1.0f), availW, 4.0f);

    if (m_altHistory.empty())
    {
        UI::CenteredText("等待数据…", 1.3f, ImVec4(0.39f, 0.46f, 0.56f, 1.0f), availW, availH * 0.5f);
        return;
    }

    // deque 非连续内存，拷贝到 vector 供 implot 连续访问
    std::vector<float> ys(m_altHistory.begin(), m_altHistory.end());

    // 标题下方放置图表（标题高度约 availH*0.14）
    ImGui::SetCursorPos(ImVec2(0.0f, availH * 0.14f));
    float plotH = availH * 0.80f;

    ImPlot::PushStyleColor(ImPlotCol_PlotBg,  ImVec4(0.118f, 0.161f, 0.231f, 1.0f)); // (30,41,59)
    ImPlot::PushStyleColor(ImPlotCol_FrameBg, ImVec4(0.118f, 0.161f, 0.231f, 1.0f));
    ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(10.0f, 8.0f));

    // CanvasOnly：去掉标题/图例/菜单/框选/鼠标读数；NoFrame：去掉外框，融入卡片
    ImPlotFlags flags = ImPlotFlags_CanvasOnly | ImPlotFlags_NoFrame;
    if (ImPlot::BeginPlot("##alt_trend", ImVec2(availW, plotH), flags))
    {
        ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoDecorations);
        ImPlot::SetupAxis(ImAxis_Y1, nullptr, ImPlotAxisFlags_NoDecorations);

        float mn = *std::min_element(ys.begin(), ys.end());
        float mx = *std::max_element(ys.begin(), ys.end());
        float padY = std::max(1.0f, (mx - mn) * 0.12f);
        ImPlot::SetupAxisLimits(ImAxis_Y1, (double)(mn - padY), (double)(mx + padY), ImPlotCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, (double)(ys.size() - 1), ImPlotCond_Always);

        ImPlotSpec spec(ImPlotProp_LineColor, UI::accentCyan(),
                        ImPlotProp_LineWeight, 2.5f,
                        ImPlotProp_Marker, ImPlotMarker_Circle,
                        ImPlotProp_MarkerSize, 4.0f);
        ImPlot::PlotLine("alt", ys.data(), (int)ys.size(), 1.0, 0.0, spec);

        ImPlot::EndPlot();
    }
    ImPlot::PopStyleVar(1);
    ImPlot::PopStyleColor(2);
}

// ---- 状态徽章 ----

void RealtimeScreen::renderStatusBadge(const LocationData& data, LocationStatus st, float centerX, float posY)
{
    // 文案含状态图标与精度后缀（需 data），绘制逻辑交给 StatusBadge 控件
    char buf[128];
    if (data.valid && data.horizontalAccuracy > 0.0)
        snprintf(buf, sizeof(buf), "%s  ·  精度 ±%.1f 米", statusText(st), data.horizontalAccuracy);
    else
        snprintf(buf, sizeof(buf), "%s", statusText(st));

    bool breathe = (st == LocationStatus::Starting || st == LocationStatus::Active);
    StatusBadge::Render(buf, centerX, posY, statusColor(st), breathe);
}

// ---- 副信息条：气压 + 垂直速率 ----

void RealtimeScreen::renderSecondaryInfo(const LocationData& data, LocationStatus /*st*/)
{
    const ImGuiStyle& sty = ImGui::GetStyle();
    ImVec2 wp = ImGui::GetWindowPos();
    float padX = sty.WindowPadding.x;
    float padY = sty.WindowPadding.y;
    float w = ImGui::GetContentRegionAvail().x;
    float gap = 12.0f;
    float cw = (w - gap) * 0.5f;
    float ch = kSecondaryCardH;
    float x0 = wp.x + padX;
    float y0 = wp.y + padY;
    AppSettings& s = AppSettings::Instance();

    const ImU32 kLabel = IM_COL32(100, 116, 139, 255);

    auto drawCard = [&](float x, const char* label, const char* value,
                        const char* unit, ImU32 valueCol) {
        StatCard::Render(ImVec2(x, y0), ImVec2(cw, ch), 12.0f, label, value, unit, valueCol);
    };

    char pBuf[32];
    if (data.hasPressure || (data.valid && data.pressureHPa > 0.0))
    {
        snprintf(pBuf, sizeof(pBuf), "%.1f", s.displayPressure(data.pressureHPa));
        drawCard(x0, "气压计", pBuf, s.pressureUnit(), UI::accentCyan());
    }
    else
    {
        drawCard(x0, "气压计", "无数据", "", kLabel);
    }

    char cBuf[32];
    if (data.valid)
    {
        double v = data.climbRate * 60.0;
        snprintf(cBuf, sizeof(cBuf), "%+.1f", v);
        ImU32 cCol = (v >= 0.0) ? IM_COL32(74, 222, 128, 255) : IM_COL32(251, 146, 60, 255);
        drawCard(x0 + cw + gap, "垂直速率", cBuf, "m/min", cCol);
    }
    else
    {
        drawCard(x0 + cw + gap, "垂直速率", "无数据", "", kLabel);
    }

    ImGui::Dummy(ImVec2(w, ch));
}

float RealtimeScreen::estimateSecondaryHeight() const
{
    const ImGuiStyle& sty = ImGui::GetStyle();
    return kSecondaryCardH + sty.WindowPadding.y * 2.0f + sty.ItemSpacing.y;
}

// ---- 控制区 ----

bool RealtimeScreen::renderControls(const LocationData& data, LocationStatus st, LocationProvider& loc)
{
    bool interacted = false;
    ImGui::Spacing();

    {
        const char* detLabel = m_showDetails ? (ICON_FA_CHEVRON_UP " 收起") : (ICON_FA_CHEVRON_DOWN " 详情");
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.06f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.12f));
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100, 116, 139, 255));
        float detW = ImGui::CalcTextSize(detLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f + 8.0f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - detW);
        if (ImGui::Button(detLabel, ImVec2(detW, 0)))
        {
            m_showDetails = !m_showDetails;
            interacted = true;
        }
        ImGui::PopStyleColor(4);
    }

    if (m_showDetails)
    {
        ImGui::Separator();
        if (data.valid)
        {
            ImGui::Text("经度: %.6f", data.longitude);
            ImGui::Text("纬度: %.6f", data.latitude);
#ifdef __ANDROID__
            ImGui::Text("椭球高(WGS84): %.1f m", data.altitudeEllipsoid);
            ImGui::Text("geoid 修正量:   %.1f m", data.altitudeMSL - data.altitudeEllipsoid);
#endif
            if (data.horizontalAccuracy > 0.0)
                ImGui::Text("水平精度: %.1f m", data.horizontalAccuracy);
            if (data.hasPressure)
            {
                ImGui::Text("GPS 海拔:   %.1f m", data.altitudeMSL);
                ImGui::Text("融合海拔:   %.1f m", data.fusedAltitude);
                ImGui::Text("气压:       %.2f hPa", data.pressureHPa);
                ImGui::Text("垂直速率:   %+.1f m/min", data.climbRate * 60.0);
            }
        }
        else
        {
            ImGui::TextDisabled("暂无定位数据");
        }
        ImGui::Separator();
    }

    ImGui::Spacing();
    bool running = (st == LocationStatus::Active || st == LocationStatus::Starting);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 22.0f);
    if (running)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.118f, 0.161f, 0.231f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.21f, 0.30f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.13f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(248, 250, 252, 255));
        if (ImGui::Button(ICON_FA_STOP "  停止测量", ImVec2(-1, 0)))
        {
            spdlog::info("[UI] 点击「停止」按钮");
            loc.stopUpdates();
            interacted = true;
        }
        ImGui::PopStyleColor(4);
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(UI::accentCyan()));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(UI::accentCyan()));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::ColorConvertU32ToFloat4(UI::accentCyan()));
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(8, 15, 30, 255));
        if (ImGui::Button(ICON_FA_PLAY "  开始测量", ImVec2(-1, 0)))
        {
            spdlog::info("[UI] 点击「开始测量」按钮，触发定位与权限请求");
            loc.startUpdates([](const LocationData&, LocationStatus) {});
            interacted = true;
        }
        ImGui::PopStyleColor(4);
    }
    ImGui::PopStyleVar();
    return interacted;
}

float RealtimeScreen::estimateControlsHeight(const LocationData& data) const
{
    (void)data;
    ImGuiStyle& sty = ImGui::GetStyle();
    float btnH = ImGui::GetFrameHeight();
    float sp = sty.ItemSpacing.y;
    float padY = sty.WindowPadding.y;
    float h = sp + btnH + sp + btnH;
    if (m_showDetails)
    {
        float lineH = ImGui::GetTextLineHeightWithSpacing();
        int lines = 3;
#ifdef __ANDROID__
        lines = 5;
#endif
        h += lineH * lines + (sp + 1.0f) * 2.0f + sp;
    }
    return h + padY * 2.0f;
}
