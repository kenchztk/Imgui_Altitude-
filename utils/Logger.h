#pragma once

// 在包含 spdlog 之前定义活跃日志级别，确保 LOG_DEBUG/LOG_TRACE 宏被编译进来，
// 运行时再由 logger->set_level() 控制是否输出。1 = SPDLOG_LEVEL_DEBUG。
#ifndef SPDLOG_ACTIVE_LEVEL
#  define SPDLOG_ACTIVE_LEVEL 1
#endif

#include <spdlog/spdlog.h>
#include <filesystem>
#include <string>

/**
 * @brief   日志管理器单例
 * @details 基于 spdlog 封装，提供「彩色控制台 + 轮转文件」双 sink 的异步日志。
 *          跨平台日志目录：macOS ~/Library/Logs/NativeApp/，
 *          Windows %APPDATA%/NativeApp/logs/。
 *          通过 init() 设置 spdlog 默认 logger，之后可用 LOG_xxx 宏或
 *          spdlog::info/warn/... 直接调用。
 * @author  huangc
 * @date    2026-07-07
 */
class Logger
{
public:
    static Logger &Instance();

    /**
     * @brief 初始化日志系统，须在 main 早期、任何业务调用前调用
     * @param vLevel 日志级别，默认 info；debug 构建可传 debug
     */
    void init(spdlog::level::level_enum vLevel = spdlog::level::info);

    /// 关闭日志系统，flush 异步队列，程序退出前调用
    void shutdown();

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    /// 获取平台相关日志目录，不存在则递归创建
    static std::filesystem::path getLogDir();
};