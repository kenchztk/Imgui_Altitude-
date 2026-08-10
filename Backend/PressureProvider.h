#pragma once
#include <memory>
#include <mutex>
#include <cstdint>

// 一次气压计采样
struct PressureSample
{
    double pressureHPa = 0.0; // 大气压强（百帕/hPa）
    int64_t timestampMs = 0;  // 采样时间戳（毫秒，单调时钟）
    bool valid = false;       // 是否已有有效采样
};

// 跨平台气压计抽象基类。
// 气压计属于环境传感器，无需运行时权限；大量中低端设备无此传感器，
// 此时 Create() 仍返回可用对象但 isAvailable() 为 false，融合层据此降级。
class PressureProvider
{
    public:
        virtual ~PressureProvider() = default;

        // 工厂：按平台返回具体子类；非目标平台返回 nullptr
        static std::unique_ptr<PressureProvider> Create();

        // -- 子类实现 --
        virtual void start() = 0;              // 开始采样
        virtual void stop() = 0;               // 停止采样
        virtual bool isAvailable() const = 0;  // 设备是否具备气压计

        // -- 基类通用（线程安全）--
        PressureSample last() const;           // 读取最新采样（主线程每帧调用）

    protected:
        // 子类在采样线程内更新缓存
        void setSample(double hPa, int64_t tsMs);

        mutable std::mutex m_mutex;
        PressureSample m_last;
};
