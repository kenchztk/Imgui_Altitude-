#pragma once
#include "nlohmann/json.hpp"
#include <optional>
#include <chrono>
#include <string>
#include <mutex>
#include <any>
#include <map>

class Backend
{
    public:
        Backend();
        ~Backend();
        
        static Backend& Instance();

        bool init();

        // 检查是否到达刷新周期，必要时发起请求并解析（仅到点时真正工作）
        bool tryRefreshUsage();
        // 返回最近一次解析缓存的用量百分比（每帧调用开销极小，不再遍历 JSON）
        void getCachedUsage(float& session_usage, float& weekly_usage, float& monthly_usage) const;

    protected:
        bool reflashUsage(std::string& result);
        void updateUsage(const std::string &data);

    private:
        mutable std::mutex m_mtxUsage;
        // 解析后缓存的最终展示值，避免每帧重复遍历 JSON
        float m_sessionUsage = 0.0f;
        float m_weeklyUsage = 0.0f;
        float m_monthlyUsage = 0.0f;

        std::shared_ptr<std::chrono::steady_clock::time_point> m_spLastUpdateTime;
};