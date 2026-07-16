#include "Backend.h"
#include <fstream>
#include <filesystem>

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
    return true;
}

LocationProvider& Backend::location()
{
    if (m_location)
        return *m_location;
    static NoOpLocationProvider s_noop;
    return s_noop;
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
