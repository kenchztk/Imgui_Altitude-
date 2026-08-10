#include "PressureProvider.h"

std::unique_ptr<PressureProvider> PressureProvider::Create()
{
#if defined(__ANDROID__)
    extern std::unique_ptr<PressureProvider> CreatePressureProviderAndroid();
    return CreatePressureProviderAndroid();
#else
    // macOS/其他平台暂无气压计（未来 iOS 可经 CoreMotion CMAltimeter 接入）
    return nullptr;
#endif
}

PressureSample PressureProvider::last() const
{
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_last;
}

void PressureProvider::setSample(double hPa, int64_t tsMs)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    m_last.pressureHPa = hPa;
    m_last.timestampMs = tsMs;
    m_last.valid = true;
}
