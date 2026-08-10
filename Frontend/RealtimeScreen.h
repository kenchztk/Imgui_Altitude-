#pragma once
#include "imgui/imgui.h"
#include "Backend/LocationProvider.h"
#include "Frontend/Screen.h"
#include <deque>

// 实时屏：指标切换（海拔/气压/温度/趋势）+ 主数字 + 副信息条 + 控制区。
// 复刻设计稿的「主数字即主角」理念：单色语义化、无装饰渐变。
class RealtimeScreen
{
public:
    enum class Metric : int
    {
        Altitude = 0,  // 海拔
        Pressure = 1,  // 气压
        Temperature = 2, // 温度
        Trend = 3       // 趋势
    };

    // 渲染实时屏。返回 true 表示本帧有按钮交互（供 Frontend 刷新空闲计时）
    bool render(const LocationData& data, LocationStatus st,
                LocationProvider& loc, const ImVec2& displaySize);

    Metric currentMetric() const { return m_metric; }
    void setMetric(Metric m) { m_metric = m; }

private:
    Metric m_metric = Metric::Altitude;
    float m_displayedAlt = 0.0f;   // 平滑插值后的显示海拔
    bool m_altInit = false;
    bool m_showDetails = false;

    // 海拔历史（趋势图），约 1Hz 采样，上限 120 点
    std::deque<float> m_altHistory;
    double m_lastSampleT = 0.0;

    // 颜色常量（accentCyan 已收口到 UI::accentCyan，见 Frontend/UIHelpers.h）
    static ImU32 statusColor(LocationStatus st);
    static const char* statusText(LocationStatus st);

    // 数字平滑插值
    void updateDisplayedAltitude(float targetAlt);
    // 按 ~1Hz 把当前海拔压入历史环形缓冲
    void sampleHistory(float alt);

    // 指标切换分段控件（4 段：海拔/气压/温度/趋势）
    bool renderMetricSwitcher();
    // 主数字区，按当前指标分派
    void renderHero(const LocationData& data, LocationStatus st, float availW, float availH);
    void renderAltitudeHero(const LocationData& data, LocationStatus st, float availW, float availH);
    void renderPressureHero(const LocationData& data, LocationStatus st, float availW, float availH);
    void renderTemperatureHero(const LocationData& data, LocationStatus st, float availW, float availH);
    void renderTrendHero(const LocationData& data, LocationStatus st, float availW, float availH);

    // 状态徽章（呼吸圆点 + 文案）
    void renderStatusBadge(const LocationData& data, LocationStatus st, float centerX, float posY);
    // 副信息条：气压 + 垂直速率（所有指标共用）
    void renderSecondaryInfo(const LocationData& data, LocationStatus st);
    float estimateSecondaryHeight() const;

    // 底部控制区（详情 + 开始/停止）
    bool renderControls(const LocationData& data, LocationStatus st, LocationProvider& loc);
    float estimateControlsHeight(const LocationData& data) const;

    static constexpr float kSecondaryCardH = 60.0f;
};
