#pragma once
#include <functional>
#include <memory>
#include <mutex>
#include <cstdint>

// 一次定位采样的数据载体
struct LocationData
{
    double altitudeMSL = 0.0;        // 海拔（MSL/正高，米）—— UI 主显示
    double altitudeEllipsoid = 0.0;  // WGS84 椭球高（米）—— Android 原始值，macOS 为 0
    double latitude = 0.0;           // 纬度（度）
    double longitude = 0.0;          // 经度（度）
    double horizontalAccuracy = 0.0; // 水平精度（米）
    double verticalAccuracy = 0.0;   // 垂直精度（米）
    double heading = 0.0;            // 方位角（真北为 0，顺时针 0~2π，弧度）
    int64_t timestampMs = 0;         // 采样时间戳（毫秒）
    bool valid = false;              // 是否已有有效数据
};

// 定位状态机
enum class LocationStatus
{
    Idle,      // 未开始
    Starting,  // 已请求，等待权限/首次定位
    Active,    // 正在持续接收定位
    Denied,    // 权限被拒绝
    Error      // 其他错误
};

// 观察者回调：数据或状态变化时触发（子类在更新后调用 updateAndNotify）
using LocationCallback = std::function<void(const LocationData&, LocationStatus)>;

// 跨平台定位抽象基类。由各平台子类实现具体获取逻辑，Backend 持有单例
class LocationProvider
{
    public:
        virtual ~LocationProvider() = default;

        // 工厂：按平台返回具体子类，非目标平台返回 nullptr
        static std::unique_ptr<LocationProvider> Create();

        // -- 子类实现 --
        virtual void startUpdates(LocationCallback cb) = 0; // 开始连续定位
        virtual void stopUpdates() = 0;                     // 停止连续定位
        virtual void requestPermission() = 0;               // 触发系统权限请求（UI 按钮调用）

        // -- 基类通用（线程安全）--
        LocationData lastKnown() const; // Frontend 每帧读取最新缓存
        LocationStatus status() const;  // 当前状态
        virtual double lastHeading() const { return 0.0; } // 最新方位角（弧度），默认 0（北朝上）

    protected:
        // 子类更新数据/状态后调用：缓存并触发 callback
        void updateAndNotify(const LocationData& d, LocationStatus s);

        mutable std::mutex m_mutex;
        LocationData m_last;
        LocationStatus m_status = LocationStatus::Idle;
        LocationCallback m_callback;
};
