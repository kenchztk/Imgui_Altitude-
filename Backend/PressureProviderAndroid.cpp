#if defined(__ANDROID__)
#include "PressureProvider.h"
#include <android/sensor.h>
#include <android/looper.h>
#include <spdlog/spdlog.h>
#include <thread>
#include <atomic>

// Android 气压计实现：用 NDK ASensorManager 在独立线程采样，
// 完全自包含（不依赖 Kotlin/JNI，气压计无需运行时权限）。
class PressureProviderAndroid : public PressureProvider
{
    public:
        PressureProviderAndroid();
        ~PressureProviderAndroid() override;

        void start() override;
        void stop() override;
        bool isAvailable() const override { return m_sensor != nullptr; }

    private:
        void threadMain();

        ASensorManager*      m_mgr = nullptr;
        const ASensor*       m_sensor = nullptr;
        std::thread          m_thread;
        std::atomic<bool>    m_running{false};

        static constexpr int kLooperId = 0x50524553; // 'PRES'，事件队列标识
        static constexpr int kSampleRateUs = 100000;  // 采样周期 100ms（10Hz）
};

PressureProviderAndroid::PressureProviderAndroid()
{
    // getInstanceForPackage 需 API 26+；低版本回退到旧 API getInstance
#if __ANDROID_API__ >= 26
    m_mgr = ASensorManager_getInstanceForPackage("");
#else
    m_mgr = ASensorManager_getInstance();
#endif
    if (!m_mgr)
    {
        spdlog::error("[Pressure] ASensorManager 获取失败");
        return;
    }
    m_sensor = ASensorManager_getDefaultSensor(m_mgr, ASENSOR_TYPE_PRESSURE);
    if (!m_sensor)
        spdlog::warn("[Pressure] 设备无气压计，融合层将降级为纯 GPS+EGM");
    else
        spdlog::info("[Pressure] 气压计就绪: {}", ASensor_getName(m_sensor));
}

PressureProviderAndroid::~PressureProviderAndroid()
{
    stop();
}

void PressureProviderAndroid::start()
{
    if (!m_sensor || m_running.load())
        return;
    m_running.store(true);
    m_thread = std::thread(&PressureProviderAndroid::threadMain, this);
    spdlog::info("[Pressure] 采样线程已启动");
}

void PressureProviderAndroid::stop()
{
    if (!m_running.exchange(false))
        return;
    if (m_thread.joinable())
        m_thread.join();
    spdlog::info("[Pressure] 采样线程已停止");
}

void PressureProviderAndroid::threadMain()
{
    // 采样线程需自己的 Looper 承载事件队列
    ALooper* looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
    if (!looper)
    {
        spdlog::error("[Pressure] ALooper_prepare 失败");
        return;
    }
    ASensorEventQueue* queue =
        ASensorManager_createEventQueue(m_mgr, looper, kLooperId, nullptr, nullptr);
    if (!queue)
    {
        spdlog::error("[Pressure] createEventQueue 失败");
        return;
    }
    ASensorEventQueue_enableSensor(queue, m_sensor);
    ASensorEventQueue_setEventRate(queue, m_sensor, kSampleRateUs);

    while (m_running.load())
    {
        // 250ms 超时轮询：既能及时响应 stop()，又避免忙等
        int ident = ALooper_pollOnce(250, nullptr, nullptr, nullptr);
        if (ident != kLooperId)
            continue;
        ASensorEvent event;
        while (ASensorEventQueue_getEvents(queue, &event, 1) > 0)
        {
            // event.pressure 单位 hPa；event.timestamp 为纳秒（单调时钟）
            setSample(event.pressure, event.timestamp / 1000000);
        }
    }

    ASensorEventQueue_disableSensor(queue, m_sensor);
    ASensorManager_destroyEventQueue(m_mgr, queue);
}

std::unique_ptr<PressureProvider> CreatePressureProviderAndroid()
{
    return std::make_unique<PressureProviderAndroid>();
}

#endif // __ANDROID__
