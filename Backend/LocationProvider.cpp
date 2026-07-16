#include "LocationProvider.h"

std::unique_ptr<LocationProvider> LocationProvider::Create()
{
#if defined(__APPLE__)
    extern std::unique_ptr<LocationProvider> CreateLocationProviderMac();
    return CreateLocationProviderMac();
#elif defined(__ANDROID__)
    extern std::unique_ptr<LocationProvider> CreateLocationProviderAndroid();
    return CreateLocationProviderAndroid();
#else
    return nullptr;
#endif
}

LocationData LocationProvider::lastKnown() const
{
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_last;
}

LocationStatus LocationProvider::status() const
{
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_status;
}

void LocationProvider::updateAndNotify(const LocationData& d, LocationStatus s)
{
    // 先在锁内更新缓存并取出 callback，再在锁外触发回调，避免回调中再次加锁导致死锁
    LocationCallback cb;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_last = d;
        m_status = s;
        cb = m_callback;
    }
    if (cb)
        cb(d, s);
}
