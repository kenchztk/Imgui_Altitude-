#include "AltitudeFusion.h"
#include <cmath>

namespace {
// -- 融合参数（经验值，真机调参入口）--
constexpr double kPressureEmaAlpha = 0.2;   // 原始气压去抖强度（越大越跟手）
constexpr double kCompAlpha        = 0.98;  // 互补滤波：气压项权重（越大越平滑、越信气压）
constexpr double kP0EmaAlpha       = 0.02;  // P0 慢校准速度（越小越稳、抗天气抖动）
constexpr double kGpsAccGate       = 20.0;  // GPS 水平精度门限（米），超过则不用于校准
constexpr double kClimbEmaAlpha    = 0.3;   // 垂直速率平滑强度
constexpr double kBaroExp          = 5.255; // 国际气压高度公式指数
constexpr double kBaroScale        = 44330.0;
} // namespace

double AltitudeFusion::baroAltitude(double pressureHPa) const
{
    // h = 44330 * (1 - (P/P0)^(1/5.255))
    return kBaroScale * (1.0 - std::pow(pressureHPa / m_p0, 1.0 / kBaroExp));
}

void AltitudeFusion::reset()
{
    *this = AltitudeFusion();
}

LocationData AltitudeFusion::update(const LocationData& gps, bool hasPressure,
                                    double pressureHPa, int64_t pressureTsMs)
{
    LocationData out = gps;

    // -- 降级：无气压计，直接回退纯 GPS+EGM 海拔；仍用 GPS 海拔差分估算垂直速率 --
    if (!hasPressure || pressureHPa <= 0.0)
    {
        out.hasPressure = false;
        out.pressureHPa = 0.0;

        const double gpsAlt = gps.valid ? gps.altitudeMSL : 0.0;
        if (gps.valid && m_lastGpsClimbTs > 0)
        {
            const double dt = (gps.timestampMs - m_lastGpsClimbTs) / 1000.0;
            if (dt > 0.0 && dt < 5.0)
            {
                const double rawRate = (gpsAlt - m_lastGpsAlt) / dt;  // 米/秒
                m_climbRate += (rawRate - m_climbRate) * kClimbEmaAlpha;
            }
        }
        if (gps.valid) { m_lastGpsAlt = gpsAlt; m_lastGpsClimbTs = gps.timestampMs; }
        out.climbRate = m_climbRate;

        out.fusedAltitude = gpsAlt;
        return out;
    }

    // 1) 原始气压去抖（EMA 低通，抑制气流/空调造成的 ±0.1hPa 抖动）
    if (!m_baroInit)
        m_smoothedPressure = pressureHPa;
    else
        m_smoothedPressure += (pressureHPa - m_smoothedPressure) * kPressureEmaAlpha;

    // 2) 判定 GPS 样本是否可用于校准/融合：新样本 + 精度达标
    const bool gpsFresh = gps.valid && gps.timestampMs != m_lastGpsTs;
    const bool accGood  = gps.horizontalAccuracy > 0.0 && gps.horizontalAccuracy <= kGpsAccGate;
    const bool gpsUsable = gpsFresh && accGood;

    // 3) 用达标 GPS 反解并校准海平面参考气压 P0（消除天气漂移）
    if (gpsUsable && gps.altitudeMSL < kBaroScale)
    {
        double denom = std::pow(1.0 - gps.altitudeMSL / kBaroScale, kBaroExp);
        if (denom > 1e-6)
        {
            double p0Target = m_smoothedPressure / denom;
            if (!m_p0Init) { m_p0 = p0Target; m_p0Init = true; }
            else           { m_p0 += (p0Target - m_p0) * kP0EmaAlpha; }
        }
    }

    const double baroAlt = baroAltitude(m_smoothedPressure);

    // 4) 首个气压样本：仅初始化状态，不产生增量/速率
    if (!m_baroInit)
    {
        m_baroInit = true;
        m_lastBaroAlt = baroAlt;
        m_lastPressureTs = pressureTsMs;
        m_fusedAlt = gps.valid ? gps.altitudeMSL : baroAlt;
        m_fusedInit = true;
        m_climbRate = 0.0;
        if (gpsFresh) m_lastGpsTs = gps.timestampMs;

        out.hasPressure = true;
        out.pressureHPa = m_smoothedPressure;
        out.fusedAltitude = m_fusedAlt;
        out.climbRate = 0.0;
        return out;
    }

    const double baroDelta = baroAlt - m_lastBaroAlt;

    // 5) 垂直速率：气压海拔差分 + EMA 平滑
    if (m_lastPressureTs > 0)
    {
        double dt = (pressureTsMs - m_lastPressureTs) / 1000.0;
        if (dt > 0.0 && dt < 5.0)
        {
            double rawRate = baroDelta / dt;
            m_climbRate += (rawRate - m_climbRate) * kClimbEmaAlpha;
        }
    }

    // 6) 互补滤波：气压增量提供高频响应，GPS 绝对值缓慢拉正
    double predicted = m_fusedAlt + baroDelta;
    if (gpsUsable)
        m_fusedAlt = kCompAlpha * predicted + (1.0 - kCompAlpha) * gps.altitudeMSL;
    else
        m_fusedAlt = predicted; // 无新 GPS（峡谷/室内）：纯气压航位推算

    // 7) 推进状态
    m_lastBaroAlt = baroAlt;
    m_lastPressureTs = pressureTsMs;
    if (gpsFresh) m_lastGpsTs = gps.timestampMs;

    out.hasPressure = true;
    out.pressureHPa = m_smoothedPressure;
    out.fusedAltitude = m_fusedAlt;
    out.climbRate = m_climbRate;
    return out;
}
