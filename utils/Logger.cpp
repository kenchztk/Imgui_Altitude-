#include "Logger.h"
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>


Logger &Logger::Instance()
{
    static Logger sl_Instance;
    return sl_Instance;
}

void Logger::init(spdlog::level::level_enum vLevel)
{
    // 初始化异步线程池：队列 8192 项，1 个后台写入线程
    spdlog::init_thread_pool(8192, 1);

    // 控制台 sink（带颜色，_mt 后缀表示多线程安全）
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(vLevel);

    // 文件 sink（轮转：单文件 5MB，保留 5 个历史文件）
    auto logFile = getLogDir() / "app.log";
    auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        logFile.string(), 5 * 1024 * 1024, 5);
    fileSink->set_level(vLevel);

    // 异步 logger，双 sink；队列满时阻塞写入线程以防丢日志
    auto logger = std::make_shared<spdlog::async_logger>(
        "main",
        spdlog::sinks_init_list{consoleSink, fileSink},
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::block);

    logger->set_level(vLevel);
    // 格式：[时间] [级别] [线程ID] 内容
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");

    spdlog::set_default_logger(logger);
    // 定时 flush，兼顾性能与异常退出时少丢日志
    spdlog::flush_every(std::chrono::seconds(3));
}

void Logger::shutdown()
{
    // flush 所有 logger 并关闭线程池，等待异步队列排空
    spdlog::shutdown();
}

Logger::~Logger()
{
    // 兜底：未显式 shutdown 时，析构确保 flush（重复调用安全）
    try { spdlog::shutdown(); } catch (...) {}
}

std::filesystem::path Logger::getLogDir()
{
    std::filesystem::path dir;
#ifdef _WIN32
    // Windows：放 %APPDATA% 下，符合系统规范
    const char *appdata = std::getenv("APPDATA");
    dir = (appdata ? appdata : ".") + std::string("/NativeApp/logs");
#elif defined(__APPLE__)
    // macOS：放 ~/Library/Logs 下，符合系统规范
    const char *home = std::getenv("HOME");
    dir = (home ? home : ".") + std::string("/Library/Logs/NativeApp");
#else
    // 其他平台（如 Android/Linux）：退回 HOME 下的 logs 目录
    const char *home = std::getenv("HOME");
    dir = (home ? home : ".") + std::string("/logs");
#endif
    std::filesystem::create_directories(dir); // 不存在则递归创建
    return dir;
}
