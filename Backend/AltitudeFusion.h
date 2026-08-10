#pragma once
#include "LocationProvider.h"
#include <cstdint>

// 海拔融合器（纯 C++，无平台依赖，可在任意平台编译/单测）。
//
// 融合策略（互补滤波）：
//   - 气压计提供高灵敏、平滑的【相对高度变化】（高频分量）
//   - GPS+EGM96 提供【绝对海拔基准】（低频分量，抗长期漂移）
//   - 互补滤波：fused = α·(fused_prev + 气压增量) + (1-α)·GPS海拔
//   - 海平面参考气压 P0 由达标 GPS 定点反解并慢速校准，消除天气变化引入的漂移
//
// 降级：无气压计时 fusedAltitude 直接回退为 GPS+EGM 的 altitudeMSL（零行为回归）。
class AltitudeFusion
{
    public:
        // 每帧调用。gps 为最新 GPS+EGM 数据；hasPressure/pressureHPa/pressureTsMs
        // 为最新气压采样（无气压计时 hasPressure=false）。
        // 返回填充了 fusedAltitude / pressureHPa / climbRate / hasPressure 的数据副本。
        LocationData update(const LocationData& gps, bool hasPressure,
                            double pressureHPa, int64_t pressureTsMs);

        // 重置内部状态（如停止测量后重新开始）
        void reset();

    private:
        // 按国际气压高度公式，用当前 P0 把气压换算为海拔（米）
        double baroAltitude(double pressureHPa) const;

        double  m_p0 = 1013.25;        // 海平面参考气压（hPa），标准大气初值
        bool    m_p0Init = false;      // P0 是否已由 GPS 校准过
        double  m_smoothedPressure = 0.0; // 去抖后的气压
        bool    m_baroInit = false;    // 气压/baroAlt 状态是否已初始化
        double  m_lastBaroAlt = 0.0;   // 上一气压海拔（用于增量与速率差分）
        int64_t m_lastPressureTs = 0;  // 上一气压时间戳（毫秒）
        double  m_fusedAlt = 0.0;      // 融合后海拔
        bool    m_fusedInit = false;   // 融合值是否已初始化
        double  m_climbRate = 0.0;     // 平滑后的垂直速率（米/秒）
        int64_t m_lastGpsTs = 0;       // 上一处理过的 GPS 时间戳（去重用）
        double  m_lastGpsAlt = 0.0;    // 上一 GPS 海拔（无气压计时估算垂直速率用）
        int64_t m_lastGpsClimbTs = 0;  // 上一 GPS 垂直速率采样时间戳（毫秒）
};
