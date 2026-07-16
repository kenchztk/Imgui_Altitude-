#pragma once
#include "imgui/imgui.h"
#include "Backend/LocationProvider.h"

// 海拔高度醒目显示模块：封装 4 种可切换渲染模式 + 动效
class AltitudeDisplay
{
public:
    enum class Mode : int
    {
        MinimalCenter = 0,  // 极简居中大字
        Gauge,              // 航空仪表盘
        HUD,                // HUD 刻度尺
        Card,               // 卡片强调
        CircleGradient      // 渐变同心圆（复刻 Xnip 图片设计）
    };

    // 渲染海拔显示区。返回 true 表示本帧有按钮交互（供 Frontend 刷新空闲计时）
    bool render(const LocationData& data, LocationStatus st,
                LocationProvider& loc, const ImVec2& displaySize);

    Mode currentMode() const { return m_mode; }
    void setMode(Mode m) { m_mode = m; }

private:
    Mode  m_mode = Mode::MinimalCenter;
    float m_displayedAlt = 0.0f;   // 平滑插值后的显示值
    bool  m_altInit = false;       // 首帧是否已初始化 m_displayedAlt
    bool  m_showDetails = false;   // 次要信息展开状态

    // 颜色常量
    static ImU32 accentCyan();         // 主强调青蓝 (80,180,255)
    static ImU32 accentAmber();        // 次强调琥珀 (255,180,60)
    static ImU32 hudGreen();           // HUD 青绿 (80,255,200)
    static ImU32 statusColor(LocationStatus st);
    static const char* statusText(LocationStatus st);
    static const char* statusIcon(LocationStatus st);

    // 数字平滑插值
    void updateDisplayedAltitude(float targetAlt);

    // 4 种模式渲染
    void renderMinimalCenter(const LocationData& data, LocationStatus st, float winW, float winH);
    void renderGauge(const LocationData& data, LocationStatus st, float winW, float winH);
    void renderHUD(const LocationData& data, LocationStatus st, float winW, float winH);
    void renderCard(const LocationData& data, LocationStatus st, float winW, float winH);
    void renderCircleGradient(const LocationData& data, LocationStatus st, float winW, float winH, double heading);

    // 公用子绘制
    void renderStatusBadge(LocationStatus st, float centerX, float posY);   // 状态徽章（带旋转/脉冲）
    void renderDetailsSection(const LocationData& data);                     // 展开的次要信息
    bool renderModeSwitcher(float winW);                                     // 顶部模式切换按钮组
    bool renderControls(const LocationData& data, LocationStatus st, LocationProvider& loc);           // 开始/停止按钮
};
