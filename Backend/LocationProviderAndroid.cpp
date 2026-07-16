#if defined(__ANDROID__)
#include "LocationProvider.h"
#include <GeographicLib/Geoid.hpp>
#include <jni.h>
#include <android_native_app_glue.h>
#include <android/asset_manager.h>
#include <android/log.h>
#include <filesystem>
#include <fstream>
#include <sys/stat.h>
#include <cstring>
#include <memory>
#include <spdlog/spdlog.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "NativeApp", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "NativeApp", __VA_ARGS__)

// 由 mainAndroid.cpp 的 Init() -> RegisterLocationNatives() 设置，供本文件访问 app/activity
static android_app* g_AndroidApp = nullptr;
static android_app* GetAndroidApp() { return g_AndroidApp; }

class LocationProviderAndroid : public LocationProvider
{
    public:
        LocationProviderAndroid();
        ~LocationProviderAndroid() override;

        void startUpdates(LocationCallback cb) override;
        void stopUpdates() override;
        void requestPermission() override;

        double lastHeading() const override { return m_heading; }

        // 由 JNI native 函数路由调用
        void onLocationPushed(double lat, double lon, double alt, double acc, int64_t ts);
        void onPermissionResult(bool granted);
        void onHeadingPushed(double headingRadians);

    private:
        // 把 assets/geoid/egm96-5.pgm 拷贝到内部存储，返回 geoid 数据目录（失败返回空）
        std::string prepareGeoidData();
        // 调用 MainActivity 的无参 void 方法
        void callKotlinVoid(const char* methodName);

        std::unique_ptr<GeographicLib::Geoid> m_geoid;
        double m_heading = 0.0; // 最新方位角（弧度）
};

// 全局实例指针，供 native 函数路由（生命周期与 Backend 一致）
static LocationProviderAndroid* g_Inst = nullptr;

LocationProviderAndroid::LocationProviderAndroid()
{
    g_Inst = this;
    std::string dir = prepareGeoidData();
    if (!dir.empty())
    {
        try
        {
            // threadsafe=true：数据读入内存，多线程安全（EGM96 5' 约 18MB）
            m_geoid = std::make_unique<GeographicLib::Geoid>("egm96-5", dir, true, true);
            LOGI("Geoid egm96-5 loaded from %s", dir.c_str());
        }
        catch (const std::exception& e)
        {
            LOGE("Geoid load failed: %s", e.what());
            m_geoid.reset();
        }
    }
    else
    {
        LOGE("Geoid data dir unavailable, fallback to ellipsoid height");
    }
}

LocationProviderAndroid::~LocationProviderAndroid()
{
    if (g_Inst == this)
        g_Inst = nullptr;
}

std::string LocationProviderAndroid::prepareGeoidData()
{
    android_app* app = GetAndroidApp();
    if (!app || !app->activity)
        return "";
    AAssetManager* mgr = app->activity->assetManager;
    const char* dataPath = app->activity->internalDataPath;
    if (!mgr || !dataPath || dataPath[0] == '\0')
        return "";

    std::string geoidDir = std::string(dataPath) + "/geoid";
    std::string dst = geoidDir + "/egm96-5.pgm";
    std::error_code ec;
    std::filesystem::create_directories(geoidDir, ec);

    // 已存在则跳过拷贝（避免每次启动重复读写）
    struct stat st;
    if (stat(dst.c_str(), &st) == 0)
        return geoidDir;

    AAsset* asset = AAssetManager_open(mgr, "geoid/egm96-5.pgm", AASSET_MODE_BUFFER);
    if (!asset)
    {
        LOGE("assets/geoid/egm96-5.pgm not found");
        return "";
    }
    const void* buf = AAsset_getBuffer(asset);
    off_t len = AAsset_getLength(asset);
    std::ofstream f(dst, std::ios::binary | std::ios::trunc);
    if (f)
    {
        f.write(static_cast<const char*>(buf), len);
        f.close();
        LOGI("copied egm96-5.pgm -> %s (%ld bytes)", dst.c_str(), (long)len);
    }
    else
    {
        LOGE("cannot write %s", dst.c_str());
        dst.clear();
    }
    AAsset_close(asset);
    return dst.empty() ? std::string() : geoidDir;
}

void LocationProviderAndroid::callKotlinVoid(const char* methodName)
{
    android_app* app = GetAndroidApp();
    if (!app || !app->activity)
    {
        spdlog::warn("[Location] callKotlinVoid({}) 失败：app/activity 不可用", methodName);
        return;
    }
    JavaVM* vm = app->activity->vm;
    JNIEnv* env = nullptr;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK)
    {
        spdlog::error("[Location] callKotlinVoid({}) GetEnv 失败", methodName);
        return;
    }
    if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK)
    {
        spdlog::error("[Location] callKotlinVoid({}) AttachCurrentThread 失败", methodName);
        return;
    }

    // 复用 ShowSoftKeyboardInput 的 JNI 模式：GetObjectClass -> GetMethodID -> CallVoidMethod
    jclass clazz = env->GetObjectClass(app->activity->clazz);
    jmethodID mid = env->GetMethodID(clazz, methodName, "()V");
    if (mid)
    {
        spdlog::info("[Location] JNI 调用 Kotlin.{}()", methodName);
        env->CallVoidMethod(app->activity->clazz, mid);
    }
    else
    {
        spdlog::error("[Location] Kotlin 方法 {} 未找到", methodName);
        LOGE("Kotlin method %s not found", methodName);
    }
    env->DeleteLocalRef(clazz);
    vm->DetachCurrentThread();
}

void LocationProviderAndroid::startUpdates(LocationCallback cb)
{
    spdlog::info("[Location] startUpdates 请求开始定位，先请求运行时权限");
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_callback = cb;
        m_status = LocationStatus::Starting;
    }
    // 先请求权限；授权结果回调 onPermissionResult 后再启动定位，状态机由 C++ 主导
    callKotlinVoid("requestLocationPermission");
}

void LocationProviderAndroid::stopUpdates()
{
    spdlog::info("[Location] stopUpdates 停止定位更新");
    callKotlinVoid("stopLocationUpdates");
    std::lock_guard<std::mutex> lk(m_mutex);
    m_status = LocationStatus::Idle;
}

void LocationProviderAndroid::requestPermission()
{
    spdlog::info("[Location] requestPermission 请求运行时权限");
    callKotlinVoid("requestLocationPermission");
}

void LocationProviderAndroid::onLocationPushed(double lat, double lon, double alt, double acc, int64_t ts)
{
    LocationData d;
    d.latitude = lat;
    d.longitude = lon;
    d.altitudeEllipsoid = alt;  // Android Location.getAltitude() 返回 WGS84 椭球高
    d.horizontalAccuracy = acc;
    d.heading = m_heading;       // 最新方位角
    d.timestampMs = ts;

    // 用 EGM96 geoid 将椭球高修正为 MSL（正高）
    if (m_geoid)
    {
        try
        {
            d.altitudeMSL = m_geoid->ConvertHeight(lat, lon, alt, GeographicLib::Geoid::ELLIPSOIDTOGEOID);
        }
        catch (const std::exception& e)
        {
            LOGE("ConvertHeight failed: %s", e.what());
            d.altitudeMSL = alt;  // 退化：直接用椭球高
        }
    }
    else
    {
        d.altitudeMSL = alt;  // 无 geoid 数据，退化用椭球高
    }
    d.valid = true;
    updateAndNotify(d, LocationStatus::Active);
}

void LocationProviderAndroid::onHeadingPushed(double headingRadians)
{
    m_heading = headingRadians;
    spdlog::debug("[Location] Android 方位更新: {:.2f}rad", m_heading);
}

void LocationProviderAndroid::onPermissionResult(bool granted)
{
    spdlog::info("[Location] 权限请求结果: granted={}", granted);
    if (granted)
    {
        // 授权后真正启动定位更新
        spdlog::info("[Location] 用户已授权，启动定位更新");
        callKotlinVoid("startLocationUpdates");
    }
    else
    {
        spdlog::warn("[Location] 用户拒绝授权，无法定位");
        updateAndNotify(m_last, LocationStatus::Denied);
    }
}

// ---- JNI native 方法实现（由 RegisterNatives 绑定到 MainActivity）----
// 签名 (DDDDJ)V：lat, lon, alt, acc 为 double，ts 为 long
static void JNICALL nativeOnLocation(JNIEnv* /*env*/, jobject /*thiz*/,
    jdouble lat, jdouble lon, jdouble alt, jdouble acc, jlong ts)
{
    if (g_Inst)
        g_Inst->onLocationPushed(lat, lon, alt, acc, (int64_t)ts);
}

// 签名 (Z)V：granted 为 boolean
static void JNICALL nativeOnPermissionResult(JNIEnv* /*env*/, jobject /*thiz*/, jboolean granted)
{
    spdlog::info("[Location] nativeOnPermissionResult 收到 Kotlin 回调, granted={}", granted == JNI_TRUE);
    if (g_Inst)
        g_Inst->onPermissionResult(granted == JNI_TRUE);
    else
        spdlog::warn("[Location] nativeOnPermissionResult 时 g_Inst 为空");
}

// 签名 (D)V：heading 弧度
static void JNICALL nativeOnHeading(JNIEnv* /*env*/, jobject /*thiz*/, jdouble headingRadians)
{
    if (g_Inst)
        g_Inst->onHeadingPushed(headingRadians);
}

// 由 mainAndroid.cpp 的 Init() 调用：记录 app 并把 native 方法注册到 MainActivity 类
bool RegisterLocationNatives(android_app* app)
{
    g_AndroidApp = app;
    if (!app || !app->activity)
        return false;
    JavaVM* vm = app->activity->vm;
    JNIEnv* env = nullptr;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK)
        return false;
    if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK)
        return false;

    // 用 activity->clazz 的实际类注册，解耦 Kotlin 包名
    jclass clazz = env->GetObjectClass(app->activity->clazz);
    static const JNINativeMethod methods[] = {
        {"nativeOnLocation",         "(DDDDJ)V", (void*)nativeOnLocation},
        {"nativeOnPermissionResult", "(Z)V",     (void*)nativeOnPermissionResult},
        {"nativeOnHeading",          "(D)V",     (void*)nativeOnHeading},
    };
    jint res = env->RegisterNatives(clazz, methods, sizeof(methods) / sizeof(methods[0]));
    if (res != 0)
        LOGE("RegisterNatives failed: %d", res);
    else
        LOGI("Location natives registered to MainActivity");
    env->DeleteLocalRef(clazz);
    vm->DetachCurrentThread();
    return res == 0;
}

std::unique_ptr<LocationProvider> CreateLocationProviderAndroid()
{
    return std::make_unique<LocationProviderAndroid>();
}

#endif // __ANDROID__
