#include "Backend.h"
#include <fstream>
#include <filesystem>

#ifdef __ANDROID__
#else
#endif

bool Backend::init()
{

    return true;
}

bool Backend::tryRefreshUsage()
{
    bool needRefresh = false;
    auto now = std::chrono::steady_clock::now();
    if (!m_spLastUpdateTime)
    {
        needRefresh = true;
        m_spLastUpdateTime = std::make_shared<std::chrono::steady_clock::time_point>(now);
        //LOG_INFO("Usage data is null, refreshing. Elapsed time: 0 seconds");
    }
    else
    {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - *m_spLastUpdateTime).count();
        if (elapsed >= 120)
        {
            needRefresh = true;
            //LOG_INFO("Usage data is stale, refreshing. Elapsed time: {} seconds", elapsed);
        }
    }

    if (needRefresh)
    {
        std::string result;
        if (reflashUsage(result))
        {
            updateUsage(result);
        }
        // 失败也更新下，避免异常时请求过于频繁
        *m_spLastUpdateTime = now;
    }
    return needRefresh;
}

void Backend::getCachedUsage(float& session_usage, float& weekly_usage, float& monthly_usage) const
{
    // 直接返回缓存的解析结果，避免每帧加锁遍历 JSON
    std::lock_guard<std::mutex> lock(m_mtxUsage);
    session_usage = m_sessionUsage;
    weekly_usage = m_weeklyUsage;
    monthly_usage = m_monthlyUsage;
}

bool Backend::reflashUsage(std::string& result)
{
    std::string strCookies;
    {
        // 复用 CookieManager 统一管理的平台数据目录路径，避免双击 .app 启动时
        // 因工作目录为 "/" 而找不到 cookies.json。
        // auto cookiesPath = CookieManager::Instance().GetCookiesFilePath();
        // //LOG_INFO("Reading cookies from: {}", cookiesPath.string());
        // std::ifstream ifs(cookiesPath.string());
        // strCookies.assign((std::istreambuf_iterator<char>(ifs)),
        //                 std::istreambuf_iterator<char>());
        // ifs.close();
    }

    if (strCookies.empty())
    {
        //LOG_WARN("Cookies file is empty or not found.");
        return false;
    }

    auto j = nlohmann::json::parse(strCookies);
    if (j.is_discarded())
    {
        //LOG_ERROR("Failed to parse cookies JSON.");
        return false;
    }

    // std::map<std::string, HttpClient::Cookie> mapCookies;
    // for (const auto& item : j)
    // {
    //      // 跳过缺少必填字段的 cookie
    //     if (!item.contains("name") || !item.contains("value") || !item.contains("domain"))
    //         continue;
    //     if (strncmp(".volcengine.com", item.value("domain", "").c_str(), strlen(".volcengine.com")))
    //         continue;
    //     try
    //     {
    //         HttpClient::Cookie cookie;
    //         cookie.name = item.value("name", "");
    //         cookie.value = item.value("value", "");
    //         cookie.domain = item.value("domain", "");
    //         cookie.path = item.value("path", "");
    //         cookie.secure = item.value("secure", false);
    //         mapCookies[item.value("name", "")] = cookie;
    //     }
    //     catch (const nlohmann::json::exception& e)
    //     {
    //         // 处理解析异常，例如记录日志或输出错误信息
    //         //LOG_ERROR("JSON parsing error: {}", e.what());
    //     }
    // }



    // // 获取数据
    // // https://console.volcengine.com/api/top/ark/cn-beijing/2024-01-01/GetCodingPlanUsage
    // auto httpRequest = std::make_shared<HttpClient>();
    // httpRequest->setTimeout(15); // 设置整体超时为 10 秒
    // httpRequest->setConnectTimeout(10); // 设置连接超时为 5 秒
    // httpRequest->setVerifySSL(false); // 启用 SSL 证书验证
    // httpRequest->setUserAgent("NativeApp/0.9.8"); // 设置 User-Agent
    // httpRequest->setDefaultHeaders({{"accept", "application/json, text/plain, */*"}, {"accept-language", "zh"},
    //     {"content-type", "application/json"}});
    // httpRequest->setCookies(mapCookies); // 设置 cookies
    // std::string csrf_token;
    // auto itF = mapCookies.find("csrfToken");
    // if (itF != mapCookies.end())
    // {
    //     csrf_token = itF->second.value;
    // }
    // //LOG_DEBUG("csrf_token: {}", csrf_token);
    // auto httpResponse = httpRequest->get("https://console.volcengine.com/api/top/ark/cn-beijing/2024-01-01/GetCodingPlanUsage",
    //     {{"x-csrf-token", csrf_token}}
    // );
    // if(httpResponse.ok())
    // {
    //     //LOG_INFO("HTTP Request succeeded with status code: {}, data: {}", httpResponse.code, httpResponse.body);
    //     // 处理响应数据
    //     // ...
    //     result = httpResponse.body;
    //     return true;
    // }
    // else
    // {
    //     //LOG_ERROR("HTTP Request failed with status code: {}, body: {}, error: {}", httpResponse.code, httpResponse.body, httpResponse.error);
    // }
    return false;
}

void Backend::updateUsage(const std::string &data)
{
    auto j = nlohmann::json::parse(data);
    if (j.is_discarded())
    {
        //LOG_ERROR("Failed to parse usage data JSON.");
        return;
    }

    if (!j.contains("Result"))
    {
        //LOG_ERROR("Usage data JSON does not contain expected fields.");
        return;
    }
    if (!j["Result"].contains("QuotaUsage"))
    {
        //LOG_ERROR("Usage data JSON does not contain expected fields.");
        return;
    }

    // 解析后直接缓存三个百分比值，避免每帧重复遍历
    float session = 0.0f, weekly = 0.0f, monthly = 0.0f;
    for (const auto& item : j["Result"]["QuotaUsage"])
    {
        auto level = item.value("Level", "");
        auto percent = item.value("Percent", 0.0f);
        if (level == "session")
            session = percent;
        else if (level == "weekly")
            weekly = percent;
        else if (level == "monthly")
            monthly = percent;
    }

    std::lock_guard<std::mutex> lock(m_mtxUsage);
    m_sessionUsage = session;
    m_weeklyUsage = weekly;
    m_monthlyUsage = monthly;
}

Backend::Backend()
    : m_spLastUpdateTime(nullptr)
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
