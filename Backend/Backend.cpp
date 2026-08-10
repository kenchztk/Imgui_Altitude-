#include "Backend.h"
#include <fstream>
#include <filesystem>
#include <cmath>

#ifdef __ANDROID__
#else
#endif

namespace {
// 非目标平台（或定位不可用）的空实现，避免上层判空
class NoOpLocationProvider : public LocationProvider
{
    public:
        void startUpdates(LocationCallback /*cb*/) override {}
        void stopUpdates() override {}
        void requestPermission() override {}
};
}

bool Backend::init()
{
    // 按平台创建定位提供者；非目标平台返回 nullptr，location() 会兜底为 NoOp
    m_location = LocationProvider::Create();

    // 气压计：无此传感器/非目标平台时为 nullptr 或 isAvailable()==false，融合层自动降级
    m_pressure = PressureProvider::Create();
    if (m_pressure && m_pressure->isAvailable())
        m_pressure->start();

    return true;
}

LocationProvider& Backend::location()
{
    if (m_location)
        return *m_location;
    static NoOpLocationProvider s_noop;
    return s_noop;
}

bool Backend::hasBarometer() const
{
    return m_pressure && m_pressure->isAvailable();
}

LocationData Backend::currentFused()
{
    // 取 GPS+EGM 绝对基准
    LocationData gps = location().lastKnown();

    // 取最新气压采样（若有气压计）
    bool hasPressure = false;
    double hPa = 0.0;
    int64_t tsMs = 0;
    if (m_pressure && m_pressure->isAvailable())
    {
        PressureSample s = m_pressure->last();
        if (s.valid)
        {
            hasPressure = true;
            hPa = s.pressureHPa;
            tsMs = s.timestampMs;
        }
    }

    LocationData fused = m_fusion.update(gps, hasPressure, hPa, tsMs);

    // 桌面/无气压计时：按当前海拔反推一个【估算】气压供 UI 展示。
    // 仅填充 pressureHPa，hasPressure 已被 fusion 置 false，不影响融合海拔基准；
    // Android 上有真实气压计时 hasPressure==true，此分支不触发。
    if (!fused.hasPressure && fused.valid)
        fused.pressureHPa = 1013.25 * std::pow(1.0 - fused.altitudeMSL / 44330.0, 5.255);

    return fused;
}

Backend::Backend()
{
}

Backend::~Backend()
{
}

Backend &Backend::Instance()
{
    static Backend sl_Instance;
    return sl_Instance;
}
