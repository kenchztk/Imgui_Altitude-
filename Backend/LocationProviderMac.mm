#import <CoreLocation/CoreLocation.h>
#import <CoreMotion/CoreMotion.h>
#include "LocationProvider.h"
#include <memory>
#include <spdlog/spdlog.h>

// 把 CLAuthorizationStatus 转为可读字符串，便于日志输出
static const char* AuthStatusString(CLAuthorizationStatus s)
{
    switch (s)
    {
        case kCLAuthorizationStatusNotDetermined:    return "NotDetermined";
        case kCLAuthorizationStatusRestricted:       return "Restricted";
        case kCLAuthorizationStatusDenied:           return "Denied";
        case kCLAuthorizationStatusAuthorizedAlways: return "AuthorizedAlways";
        default:                                     return "Unknown";
    }
}

// CLLocationManager 的 delegate，反向持有 C++ provider 指针（assign，不参与引用计数）
@interface LPMDelegate : NSObject <CLLocationManagerDelegate>
@property (assign) class LocationProviderMac* provider;
@end

// macOS 定位实现：CoreLocation 的 CLLocation.altitude 本身即 MSL，无需 geoid 修正
class LocationProviderMac : public LocationProvider
{
    public:
        LocationProviderMac() : m_mgr(nullptr), m_delegate(nullptr), m_heading(0.0) {}
        ~LocationProviderMac() override;

        void startUpdates(LocationCallback cb) override;
        void stopUpdates() override;
        void requestPermission() override;

        double lastHeading() const override { return m_heading; }

        // delegate 回调入口
    void onLocation(CLLocation* loc);
    void onAuthChange(CLLocationManager* mgr);
#if TARGET_OS_IOS
    void onHeading(CLLocation* newHeading);
#elif TARGET_OS_OSX
    // macOS 上 CoreLocation/CoreMotion 不支持方向传感器，固定 heading = 0（N 朝上）
#endif

    private:
        // 懒创建 manager 与 delegate，避免未用定位时申请权限
        void ensureManager();

        CLLocationManager* m_mgr;
        id<CLLocationManagerDelegate> m_delegate;
        double m_heading; // 最新方位角（弧度，真北 0，顺时针）
#if TARGET_OS_IOS
        // iOS 支持 heading
#endif
};

@implementation LPMDelegate
- (void)locationManager:(CLLocationManager*)mgr didUpdateLocations:(NSArray<CLLocation*>*)locations
{
    CLLocation* loc = locations.lastObject;
    if (loc != nil && self.provider != nullptr)
        self.provider->onLocation(loc);
}

- (void)locationManagerDidChangeAuthorization:(CLLocationManager*)mgr
{
    if (self.provider != nullptr)
        self.provider->onAuthChange(mgr);
}
@end

void LocationProviderMac::ensureManager()
{
    if (m_mgr != nil)
        return;
    m_mgr = [[CLLocationManager alloc] init];
    m_delegate = [[LPMDelegate alloc] init];
    ((LPMDelegate*)m_delegate).provider = this;
    m_mgr.delegate = m_delegate;
    m_mgr.desiredAccuracy = kCLLocationAccuracyBest;
}

LocationProviderMac::~LocationProviderMac()
{
    if (m_mgr != nil)
    {
        m_mgr.delegate = nil;
        [m_mgr release];
        m_mgr = nil;
    }
    if (m_delegate != nil)
    {
        [m_delegate release];
        m_delegate = nil;
    }
}

void LocationProviderMac::startUpdates(LocationCallback cb)
{
    spdlog::info("[Location] startUpdates 请求开始定位");
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_callback = cb;
        m_status = LocationStatus::Starting;
    }
    // CLLocationManager 必须在主线程使用；本函数由 UI 按钮在主线程触发
    ensureManager();
    CLAuthorizationStatus auth = m_mgr.authorizationStatus;
    spdlog::info("[Location] 当前授权状态: {}", AuthStatusString(auth));
    if (auth == kCLAuthorizationStatusAuthorizedAlways)
    {
        spdlog::info("[Location] 已授权，直接启动定位更新");
        [m_mgr startUpdatingLocation];
    }
    else if (auth == kCLAuthorizationStatusDenied ||
             auth == kCLAuthorizationStatusRestricted)
    {
        spdlog::warn("[Location] 权限被拒绝/受限，无法定位");
        updateAndNotify(m_last, LocationStatus::Denied);
    }
    else
    {
        // NotDetermined：先请求授权，授权变更后在 onAuthChange 中启动
        spdlog::info("[Location] 首次使用，请求 WhenInUse 授权");
        [m_mgr requestWhenInUseAuthorization];
    }
}

void LocationProviderMac::requestPermission()
{
    spdlog::info("[Location] requestPermission 请求 WhenInUse 授权");
    ensureManager();
    [m_mgr requestWhenInUseAuthorization];
}

void LocationProviderMac::stopUpdates()
{
    spdlog::info("[Location] stopUpdates 停止定位更新");
    if (m_mgr != nil)
        [m_mgr stopUpdatingLocation];
    std::lock_guard<std::mutex> lk(m_mutex);
    m_status = LocationStatus::Idle;
}

void LocationProviderMac::onLocation(CLLocation* loc)
{
    LocationData d;
    d.altitudeMSL = loc.altitude;                 // CoreLocation 直接给出 MSL（正高）
    d.altitudeEllipsoid = 0.0;                    // CoreLocation 不提供椭球高
    d.latitude = loc.coordinate.latitude;
    d.longitude = loc.coordinate.longitude;
    d.horizontalAccuracy = loc.horizontalAccuracy;
    d.verticalAccuracy = loc.verticalAccuracy;
    d.heading = m_heading;                        // 最新方位角
    d.timestampMs = (int64_t)([loc.timestamp timeIntervalSince1970] * 1000.0);
    d.valid = true;
    spdlog::info("[Location] 定位更新: MSL={:.1f}m lat={:.6f} lon={:.6f} hAcc={:.1f}m heading={:.2f}rad",
                 d.altitudeMSL, d.latitude, d.longitude, d.horizontalAccuracy, d.heading);
    updateAndNotify(d, LocationStatus::Active);
}

void LocationProviderMac::onAuthChange(CLLocationManager* mgr)
{
    CLAuthorizationStatus auth = mgr.authorizationStatus;
    spdlog::info("[Location] 授权状态变更: {}", AuthStatusString(auth));
    if (auth == kCLAuthorizationStatusAuthorizedAlways)
    {
        spdlog::info("[Location] 用户已授权，启动定位更新");
        [mgr startUpdatingLocation];
    }
    else if (auth == kCLAuthorizationStatusDenied ||
             auth == kCLAuthorizationStatusRestricted)
    {
        spdlog::warn("[Location] 用户拒绝授权/受限");
        updateAndNotify(m_last, LocationStatus::Denied);
    }
    // NotDetermined 状态：等待用户在系统弹窗中操作，不在此处理
}

std::unique_ptr<LocationProvider> CreateLocationProviderMac()
{
    return std::make_unique<LocationProviderMac>();
}
