#include "Frontend/AltitudeDisplay.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "IconsFontAwesome6.h"
#include "Backend/LocationProvider.h"
#include <spdlog/spdlog.h>
#include <cmath>
#include <cstdio>

namespace {

// 在指定 posY 处水平居中绘制一段文本（指定字号缩放与颜色）
void CenteredText(const char* text, float scale, const ImVec4& col, float winW, float posY)
{
    ImGui::SetWindowFontScale(scale);
    ImVec2 sz = ImGui::CalcTextSize(text);
    ImVec2 wp = ImGui::GetWindowPos();
    ImGui::SetCursorScreenPos(ImVec2(wp.x + (winW - sz.x) * 0.5f, wp.y + posY));
    ImGui::TextColored(col, "%s", text);
    ImGui::SetWindowFontScale(1.0f);
}

} // namespace

// ---- 颜色常量 ----

ImU32 AltitudeDisplay::accentCyan()  { return IM_COL32(80, 180, 255, 255); }
ImU32 AltitudeDisplay::accentAmber() { return IM_COL32(255, 180, 60, 255); }
ImU32 AltitudeDisplay::hudGreen()    { return IM_COL32(80, 255, 200, 255); }

ImU32 AltitudeDisplay::statusColor(LocationStatus st)
{
    switch (st)
    {
        case LocationStatus::Idle:     return IM_COL32(204, 204, 204, 255); // 灰
        case LocationStatus::Starting: return IM_COL32(255, 204, 51, 255);  // 黄
        case LocationStatus::Active:   return IM_COL32(77, 230, 102, 255);  // 绿
        case LocationStatus::Denied:   return IM_COL32(242, 77, 77, 255);   // 红
        default:                       return IM_COL32(242, 77, 77, 255);   // 红(Error)
    }
}

const char* AltitudeDisplay::statusText(LocationStatus st)
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

const char* AltitudeDisplay::statusIcon(LocationStatus st)
{
    switch (st)
    {
        case LocationStatus::Idle:     return ICON_FA_POWER_OFF;
        case LocationStatus::Starting: return ICON_FA_ARROWS_ROTATE;
        case LocationStatus::Active:   return ICON_FA_LOCATION_ARROW;
        case LocationStatus::Denied:   return ICON_FA_BAN;
        default:                       return ICON_FA_TRIANGLE_EXCLAMATION;
    }
}

// ---- 数字平滑插值 ----

void AltitudeDisplay::updateDisplayedAltitude(float targetAlt)
{
    if (!m_altInit) { m_displayedAlt = targetAlt; m_altInit = true; return; }
    m_displayedAlt += (targetAlt - m_displayedAlt) * 0.15f;
    if (std::fabs(targetAlt - m_displayedAlt) < 0.05f)
        m_displayedAlt = targetAlt;
}

// ---- 主入口 ----

bool AltitudeDisplay::render(const LocationData& data, LocationStatus st,
                             LocationProvider& loc, const ImVec2& displaySize)
{
    float winW = displaySize.x;
    float winH = displaySize.y;
    updateDisplayedAltitude(data.valid ? (float)data.altitudeMSL : m_displayedAlt);

    bool interacted = false;
    interacted |= renderModeSwitcher(winW);

    switch (m_mode)
    {
        case Mode::MinimalCenter: renderMinimalCenter(data, st, winW, winH); break;
        case Mode::Gauge:         renderGauge(data, st, winW, winH); break;
        case Mode::HUD:           renderHUD(data, st, winW, winH); break;
        case Mode::Card:          renderCard(data, st, winW, winH); break;
        case Mode::CircleGradient: renderCircleGradient(data, st, winW, winH, loc.lastHeading()); break;
    }

    interacted |= renderControls(data, st, loc);
    return interacted;
}

// ---- 模式切换按钮组 ----

bool AltitudeDisplay::renderModeSwitcher(float winW)
{
    bool interacted = false;
    const Mode modes[] = { Mode::MinimalCenter, Mode::Gauge, Mode::HUD, Mode::Card, Mode::CircleGradient };
    const char* icons[] = { ICON_FA_TEXT_HEIGHT, ICON_FA_GAUGE_HIGH,
                            ICON_FA_RULER_VERTICAL, ICON_FA_BORDER_ALL, ICON_FA_CIRCLE };

    ImGuiStyle& sty = ImGui::GetStyle();
    float spacing = sty.ItemSpacing.x;
    float pad = sty.WindowPadding.x;
    float btnW = (winW - pad * 2.0f - spacing * 4.0f) / 5.0f;

    ImGui::SetWindowFontScale(1.4f);
    for (int i = 0; i < 5; ++i)
    {
        if (i > 0) ImGui::SameLine(0.0f, spacing);
        bool selected = (m_mode == modes[i]);
        if (selected)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(accentCyan()));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(accentCyan()));
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(20, 30, 45, 255));
        }
        if (ImGui::Button(icons[i], ImVec2(btnW, 0.0f)))
        {
            m_mode = modes[i];
            interacted = true;
            spdlog::info("[UI] 切换显示模式 -> {}", (int)m_mode);
        }
        if (selected) ImGui::PopStyleColor(3);
    }
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Spacing();
    return interacted;
}

// ---- 模式一：极简居中大字 ----

void AltitudeDisplay::renderMinimalCenter(const LocationData& data, LocationStatus st,
                                          float winW, float winH)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float t = (float)ImGui::GetTime();
    float pulse = 0.5f + 0.5f * sinf(t * 2.0f);

    float centerX = winW * 0.5f;
    float centerY = winH * 0.36f;

    // 呼吸光晕（仅有数据时）
    if (data.valid)
    {
        float glowR = winW * 0.16f + winW * 0.02f * pulse;
        ImU32 glow = IM_COL32(80, 180, 255, (int)(28 + 22 * pulse));
        dl->AddCircleFilled(ImVec2(centerX, centerY), glowR, glow, 48);
    }

    // 数值
    char buf[32];
    const char* valStr;
    if (data.valid)
        snprintf(buf, sizeof(buf), "%.1f", m_displayedAlt), valStr = buf;
    else
        valStr = "--";

    bool active = (st == LocationStatus::Active);
    ImVec4 valCol = active ? ImVec4(0.47f, 0.86f, 1.0f, 1.0f) : ImVec4(0.94f, 0.94f, 0.94f, 1.0f);

    ImGui::SetWindowFontScale(6.0f);
    ImVec2 valSize = ImGui::CalcTextSize(valStr);
    ImVec2 wp = ImGui::GetWindowPos();
    ImGui::SetCursorScreenPos(ImVec2(wp.x + (winW - valSize.x) * 0.5f, wp.y + centerY - valSize.y * 0.5f));
    ImGui::TextColored(valCol, "%s", valStr);
    ImGui::SetWindowFontScale(1.0f);

    // 单位
    CenteredText("m (MSL)", 1.5f, ImVec4(0.6f, 0.7f, 0.85f, 1.0f), winW, centerY + valSize.y * 0.5f + 8.0f);

    // 状态徽章
    renderStatusBadge(st, centerX, centerY + valSize.y * 0.5f + 44.0f);
}

// ---- 模式二：航空仪表盘 ----

void AltitudeDisplay::renderGauge(const LocationData& data, LocationStatus st,
                                  float winW, float winH)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float t = (float)ImGui::GetTime();

    ImVec2 center(winW * 0.5f, winH * 0.40f);
    float R = ImMin(winW, winH) * 0.30f;

    // 270° 扇形：底部开口，从 135°(左下) 经上 到 45°(右下)
    const float a_min = 3.0f * IM_PI / 4.0f;       // 135°
    const float a_max = a_min + 3.0f * IM_PI / 2.0f; // 135°+270°=405°(=45°)

    // 背景半透明圆盘增强对比
    dl->AddCircleFilled(center, R + 6.0f, IM_COL32(15, 20, 30, 160), 64);

    // 外圈
    dl->AddCircle(center, R, IM_COL32(120, 130, 145, 255), 64, 3.0f);
    dl->AddCircle(center, R * 0.92f, IM_COL32(60, 70, 85, 180), 48, 1.0f);

    // 刻度：主刻度 9 等分(每 33.75°)，次刻度在主刻度间
    const int majorTicks = 9;
    for (int i = 0; i <= majorTicks; ++i)
    {
        float a = a_min + (float)i / majorTicks * (a_max - a_min);
        float ca = cosf(a), sa = sinf(a);
        // 主刻度长线
        dl->AddLine(ImVec2(center.x + ca * R, center.y + sa * R),
                    ImVec2(center.x + ca * (R - 16.0f), center.y + sa * (R - 16.0f)),
                    accentAmber(), 2.5f);
        // 两侧次刻度
        for (int j = 1; j < 4; ++j)
        {
            float a2 = a + (float)j / 4.0f * (a_max - a_min) / majorTicks;
            if (a2 > a_max) break;
            float ca2 = cosf(a2), sa2 = sinf(a2);
            dl->AddLine(ImVec2(center.x + ca2 * R, center.y + sa2 * R),
                        ImVec2(center.x + ca2 * (R - 8.0f), center.y + sa2 * (R - 8.0f)),
                        IM_COL32(100, 110, 125, 200), 1.0f);
        }
    }

    // 量程标注：动态范围 [0, maxRange]
    float maxRange = 1000.0f;
    if (m_displayedAlt > 0.0f)
        maxRange = std::ceil(m_displayedAlt / 1000.0f) * 1000.0f + 1000.0f;
    float labelFontSize = ImMax(11.0f, winH * 0.018f);
    for (int i = 0; i <= 4; ++i)
    {
        float a = a_min + (float)i / 4.0f * (a_max - a_min);
        float val = (float)i / 4.0f * maxRange;
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", (int)val);
        float ca = cosf(a), sa = sinf(a);
        ImVec2 tp(center.x + ca * (R - 30.0f), center.y + sa * (R - 30.0f));
        ImVec2 ts = ImGui::CalcTextSize(buf);
        dl->AddText(ImGui::GetFont(), labelFontSize,
                    ImVec2(tp.x - ts.x * 0.5f, tp.y - ts.y * 0.5f),
                    IM_COL32(180, 190, 200, 220), buf);
    }

    // 扫描线动效
    if (data.valid)
    {
        float scanProgress = fmodf(t * 0.25f, 1.0f);
        float scanA = a_min + scanProgress * (a_max - a_min);
        ImVec2 scanTip(center.x + cosf(scanA) * R * 0.88f, center.y + sinf(scanA) * R * 0.88f);
        dl->AddLine(center, scanTip, IM_COL32(80, 180, 255, 140), 2.0f);
    }

    // 指针
    if (data.valid)
    {
        float ratio = (maxRange > 0.0f) ? (m_displayedAlt / maxRange) : 0.0f;
        ratio = ImClamp(ratio, 0.0f, 1.0f);
        float na = a_min + ratio * (a_max - a_min);
        ImVec2 tip(center.x + cosf(na) * R * 0.80f, center.y + sinf(na) * R * 0.80f);
        dl->AddLine(center, tip, accentCyan(), 4.0f);
        dl->AddCircleFilled(center, 8.0f, accentCyan(), 20);
        dl->AddCircleFilled(center, 4.0f, IM_COL32(20, 30, 45, 255), 16);
    }

    // 中心数值
    char valBuf[32];
    const char* valStr = data.valid ? (snprintf(valBuf, sizeof(valBuf), "%.1f", m_displayedAlt), valBuf) : "--";
    CenteredText(valStr, 4.0f, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), winW, center.y - R * 0.25f);
    CenteredText("m (MSL)", 1.2f, ImVec4(0.6f, 0.7f, 0.85f, 1.0f), winW, center.y + R * 0.08f);

    // 状态徽章
    renderStatusBadge(st, winW * 0.5f, center.y + R * 0.30f);
}

// ---- 模式三：HUD 刻度尺 ----

void AltitudeDisplay::renderHUD(const LocationData& data, LocationStatus st,
                                float winW, float winH)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();

    float tapeX = wp.x + winW * 0.28f;
    float y0 = wp.y + winH * 0.22f;
    float y1 = wp.y + winH * 0.60f;
    float centerY = (y0 + y1) * 0.5f;
    float tapeH = y1 - y0;
    float ppm = tapeH / 200.0f; // 200m 跨度填满带高

    // 背景半透明深色矩形
    dl->AddRectFilled(ImVec2(tapeX - 16.0f, y0), ImVec2(tapeX + 22.0f, y1),
                      IM_COL32(10, 20, 25, 180), 6.0f);

    // 刻度线（以 m_displayedAlt 为中心，每 50m 一条）
    int altStart = (int)std::floor(m_displayedAlt / 50.0f) * 50 - 200;
    int altEnd = (int)std::ceil(m_displayedAlt / 50.0f) * 50 + 200;
    float labelFontSize = ImMax(11.0f, winH * 0.018f);
    for (int alt = altStart; alt <= altEnd; alt += 50)
    {
        float y = centerY - ((float)alt - m_displayedAlt) * ppm;
        if (y < y0 - 2.0f || y > y1 + 2.0f) continue;
        bool major = (alt % 100 == 0);
        float len = major ? 14.0f : 7.0f;
        ImU32 col = major ? hudGreen() : IM_COL32(60, 120, 110, 200);
        dl->AddLine(ImVec2(tapeX, y), ImVec2(tapeX + len, y), col, major ? 2.0f : 1.0f);
        if (major)
        {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", alt);
            dl->AddText(ImGui::GetFont(), labelFontSize,
                        ImVec2(tapeX - 40.0f, y - labelFontSize * 0.5f),
                        hudGreen(), buf);
        }
    }

    // 固定参考线（中间高亮）
    dl->AddLine(ImVec2(tapeX - 18.0f, centerY), ImVec2(tapeX + 28.0f, centerY),
                accentCyan(), 3.0f);
    // 参考线两端小三角
    dl->AddTriangleFilled(ImVec2(tapeX + 28.0f, centerY - 5.0f),
                          ImVec2(tapeX + 28.0f, centerY + 5.0f),
                          ImVec2(tapeX + 36.0f, centerY), accentCyan());

    // 数值（参考线右侧）
    char valBuf[32];
    const char* valStr = data.valid ? (snprintf(valBuf, sizeof(valBuf), "%.1f", m_displayedAlt), valBuf) : "--";
    ImGui::SetWindowFontScale(4.0f);
    ImVec2 vsz = ImGui::CalcTextSize(valStr);
    ImGui::SetCursorScreenPos(ImVec2(tapeX + 60.0f, centerY - vsz.y * 0.5f));
    ImGui::TextColored(ImVec4(0.31f, 1.0f, 0.78f, 1.0f), "%s", valStr);
    ImGui::SetWindowFontScale(1.0f);

    // 单位
    ImGui::SetWindowFontScale(1.3f);
    ImVec2 usz = ImGui::CalcTextSize("m (MSL)");
    ImGui::SetCursorScreenPos(ImVec2(tapeX + 60.0f, centerY + vsz.y * 0.5f + 2.0f));
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.7f, 1.0f), "m (MSL)");
    ImGui::SetWindowFontScale(1.0f);
    (void)usz;

    // 状态徽章（数值下方）
    ImGui::SetWindowFontScale(1.0f);
    renderStatusBadge(st, tapeX + 100.0f, centerY + vsz.y * 0.5f + 30.0f);
}

// ---- 模式四：卡片强调 ----

void AltitudeDisplay::renderCard(const LocationData& data, LocationStatus st,
                                 float winW, float winH)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    float t = (float)ImGui::GetTime();
    float pulse = 0.5f + 0.5f * sinf(t * 2.5f);

    ImVec2 p_min(wp.x + winW * 0.10f, wp.y + winH * 0.16f);
    ImVec2 p_max(wp.x + winW * 0.90f, wp.y + winH * 0.60f);
    float rounding = 16.0f;

    // 渐变背景（左上深青 -> 右下深蓝）
    dl->AddRectFilledMultiColor(p_min, p_max,
                                IM_COL32(30, 60, 90, 230),
                                IM_COL32(20, 45, 75, 230),
                                IM_COL32(20, 40, 70, 230),
                                IM_COL32(30, 55, 85, 230));

    // 脉冲描边
    ImU32 border = IM_COL32(80, 180, 255, (int)(120 + 80 * pulse));
    dl->AddRect(p_min, p_max, border, rounding, 0, 2.5f);

    // 卡片内标题
    CenteredText(ICON_FA_MOUNTAIN "  海拔高度", 1.3f, ImVec4(0.7f, 0.8f, 0.95f, 1.0f),
                 winW, winH * 0.20f);

    // 卡片内数值
    char valBuf[32];
    const char* valStr = data.valid ? (snprintf(valBuf, sizeof(valBuf), "%.1f", m_displayedAlt), valBuf) : "--";
    CenteredText(valStr, 5.0f, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), winW, winH * 0.30f);

    // 单位
    CenteredText("m (MSL)", 1.3f, ImVec4(0.6f, 0.75f, 0.9f, 1.0f), winW, winH * 0.44f);

    // 状态进度条
    float barX = p_min.x + 20.0f;
    float barW = (p_max.x - p_min.x) - 40.0f;
    float barY = p_max.y - 50.0f;
    dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + 6.0f),
                      IM_COL32(40, 50, 65, 255), 3.0f);
    float fillRatio = 0.0f;
    if (st == LocationStatus::Active) fillRatio = 1.0f;
    else if (st == LocationStatus::Starting) fillRatio = 0.5f;
    if (fillRatio > 0.0f)
    {
        dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW * fillRatio, barY + 6.0f),
                          statusColor(st), 3.0f);
    }

    // 状态徽章
    renderStatusBadge(st, winW * 0.5f, barY + 14.0f);
}

// ---- 状态徽章（呼吸圆点 + 文案）----

void AltitudeDisplay::renderStatusBadge(LocationStatus st, float centerX, float posY)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();

    ImGui::SetWindowFontScale(1.0f);
    const char* txt = statusText(st);
    ImVec2 ts = ImGui::CalcTextSize(txt);

    float dotR = 5.0f;
    if (st == LocationStatus::Starting || st == LocationStatus::Active)
    {
        float t = (float)ImGui::GetTime();
        dotR = 4.0f + 2.0f * (0.5f + 0.5f * sinf(t * 3.0f));
    }
    float dotX = wp.x + centerX - ts.x * 0.5f - 14.0f;
    dl->AddCircleFilled(ImVec2(dotX, wp.y + posY + ts.y * 0.5f), dotR, statusColor(st), 16);

    ImGui::SetCursorScreenPos(ImVec2(wp.x + centerX - ts.x * 0.5f, wp.y + posY));
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(statusColor(st)), "%s", txt);
}

// ---- 次要信息展开 ----

void AltitudeDisplay::renderDetailsSection(const LocationData& data)
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
    }
    else
    {
        ImGui::TextDisabled("暂无定位数据");
    }
    ImGui::Separator();
}

// ---- 控制按钮 ----

bool AltitudeDisplay::renderControls(const LocationData& data, LocationStatus st, LocationProvider& loc)
{
    bool interacted = false;
    ImGui::Spacing();

    const char* detIcon = m_showDetails ? (ICON_FA_CHEVRON_UP "  收起") : (ICON_FA_CHEVRON_DOWN "  详情");
    if (ImGui::Button(detIcon, ImVec2(-1, 0)))
    {
        m_showDetails = !m_showDetails;
        interacted = true;
    }

    if (m_showDetails)
        renderDetailsSection(data);

    ImGui::Spacing();
    bool running = (st == LocationStatus::Active || st == LocationStatus::Starting);
    if (running)
    {
        if (ImGui::Button(ICON_FA_STOP "  停止", ImVec2(-1, 0)))
        {
            spdlog::info("[UI] 点击「停止」按钮");
            loc.stopUpdates();
            interacted = true;
        }
    }
    else
    {
        if (ImGui::Button(ICON_FA_PLAY "  开始测量", ImVec2(-1, 0)))
        {
            spdlog::info("[UI] 点击「开始测量」按钮，触发定位与权限请求");
            loc.startUpdates([](const LocationData&, LocationStatus) {});
            interacted = true;
        }
    }
    return interacted;
}

// ---- 模式五：渐变同心圆（复刻 Xnip 图片设计）----

void AltitudeDisplay::renderCircleGradient(const LocationData& data, LocationStatus st,
                                            float winW, float winH, double heading)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();

    // 大圆中心
    ImVec2 center(wp.x + winW * 0.5f, wp.y + winH * 0.45f);
    float minDim = ImMin(winW, winH);

    // 三层圆环半径
    float R1 = minDim * 0.42f;     // 最外层淡蓝绿
    float R2 = R1 * 0.82f;         // 中间棕黄
    float R3 = R1 * 0.68f;         // 内层红橙渐变

    // 绘制三层背景圆环
    dl->AddCircleFilled(center, R1, IM_COL32(120, 210, 200, 100), 64);
    dl->AddCircleFilled(center, R2, IM_COL32(200, 150, 80, 120), 64);

    // 内层红橙垂直渐变：分段绘制扇形实现渐变
    int segments = 32;
    for (int i = 0; i < segments; ++i)
    {
        float a0 = (float)i / segments * 2.0f * IM_PI;
        float a1 = (float)(i+1) / segments * 2.0f * IM_PI;
        float y0 = center.y + R3 * sinf(a0);
        float y1 = center.y + R3 * sinf(a1);
        float t0 = (y0 - (wp.y + R3)) / (2.0f * R3); // 0 = top, 1 = bottom
        float t1 = (y1 - (wp.y + R3)) / (2.0f * R3);
        ImU32 c0 = IM_COL32(255, 40, 40, (int)(220 - 100 * t0));
        ImU32 c1 = IM_COL32(255, 120, 40, (int)(220 - 100 * t1));
        // 绘制三角形扇形段
        dl->PathArcTo(center, R3, a0, a1);
        dl->PathLineTo(center);
        ImU32 colors[3] = {c0, c1, IM_COL32(0,0,0,0)};
        dl->PathFillConvex( (c0 + c1)/2 );
    }

    // N/S 方向箭头（按 heading 旋转）
    float arrowLen = 32.0f;
    float arrowBaseW = 22.0f;

    // N 箭头（指向北 = 0 + heading，位置在 center 上方）
    {
        float ang = (float)(-IM_PI/2.0 + heading); // 0=北朝上，-pi/2 就是朝上
        float dx = cosf(ang);
        float dy = sinf(ang);
        ImVec2 tip(center.x + dx * (R1 + 16), center.y + dy * (R1 + 16));
        ImVec2 baseMid(center.x + dx * (R1 + 16 - arrowLen), center.y + dy * (R1 + 16 - arrowLen));
        float perpDx = -dy * (arrowBaseW/2.0f);
        float perpDy = dx * (arrowBaseW/2.0f);
        ImVec2 p1(baseMid.x + perpDx, baseMid.y + perpDy);
        ImVec2 p2(baseMid.x - perpDx, baseMid.y - perpDy);
        dl->AddTriangleFilled(p1, p2, tip, IM_COL32(220, 30, 30, 255));
        // 绘制 N 文字在箭头中心
        float textSize = 16.0f;
        ImVec2 textSz = ImGui::CalcTextSize("N");
        dl->AddText(ImGui::GetFont(), textSize,
                    ImVec2((p1.x + p2.x + tip.x)/2.0f - textSz.x/2.0f,
                           (p1.y + p2.y + tip.y)/2.0f - textSz.y/2.0f),
                    IM_COL32(255, 255, 255, 255), "N");
    }

    // S 箭头（指向南 = pi + heading，位置在 center 下方）
    {
        float ang = (float)(IM_PI/2.0 + heading); // 南向下 = pi/2
        float dx = cosf(ang);
        float dy = sinf(ang);
        ImVec2 tip(center.x + dx * (R1 + 16), center.y + dy * (R1 + 16));
        ImVec2 baseMid(center.x + dx * (R1 + 16 - arrowLen), center.y + dy * (R1 + 16 - arrowLen));
        float perpDx = -dy * (arrowBaseW/2.0f);
        float perpDy = dx * (arrowBaseW/2.0f);
        ImVec2 p1(baseMid.x + perpDx, baseMid.y + perpDy);
        ImVec2 p2(baseMid.x - perpDx, baseMid.y - perpDy);
        dl->AddTriangleFilled(p1, p2, tip, IM_COL32(220, 220, 200, 180));
        // 绘制 S 文字
        float textSize = 16.0f;
        ImVec2 textSz = ImGui::CalcTextSize("S");
        dl->AddText(ImGui::GetFont(), textSize,
                    ImVec2((p1.x + p2.x + tip.x)/2.0f - textSz.x/2.0f,
                           (p1.y + p2.y + tip.y)/2.0f - textSz.y/2.0f),
                    IM_COL32(80, 80, 60, 200), "S");
    }

    // 顶部信息行：气压 + 海拔精度
    float infoY = wp.y + R1 * 0.10f;
    char infoBuf[64];
    snprintf(infoBuf, sizeof(infoBuf), "-- mmHg    海拔精度: %.0f米", data.valid ? data.horizontalAccuracy : 0.0);
    ImGui::SetCursorScreenPos(ImVec2(wp.x, infoY));
    CenteredText(infoBuf, 1.1f, ImVec4(1.0f, 1.0f, 1.0f, 0.85f), winW, infoY);

    // 中心文字：当前海拔
    float titleY = center.y - R3 * 0.48f;
    CenteredText("当前海拔", 1.5f, ImVec4(1.0f, 1.0f, 1.0f, 0.8f), winW, titleY);

    // 大字海拔数值 + 米
    char valBuf[32];
    const char* valStr = data.valid ? (snprintf(valBuf, sizeof(valBuf), "%.0f", m_displayedAlt), valBuf) : "--";
    float valY = center.y - R3 * 0.10f;
    CenteredText(valStr, 5.5f, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), winW, valY);

    float meterW = ImGui::CalcTextSize("米").x;
    float meterY = valY + 5.5f * 18.0f * 0.5f + 4.0f;
    CenteredText("米", 2.5f, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), winW, meterY);

    // 状态徽章（大圆环下方）
    renderStatusBadge(st, winW * 0.5f, center.y + R1 + 24.0f);
}
