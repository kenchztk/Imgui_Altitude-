#include "Frontend/RecordsScreen.h"
#include "Frontend/UIHelpers.h"
#include "Frontend/Widgets/StatStrip.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "IconsFontAwesome6.h"
#include <spdlog/spdlog.h>

const RecordsScreen::Track RecordsScreen::kTracks[3] = {
    { "玉龙雪山徒步", "2026-07-28", 12.6f, 1240, "3h05m" },
    { "哈巴村环线",   "2026-07-20",  9.8f,  520, "2h10m" },
    { "蓝月谷漫步",   "2026-07-12",  6.0f,  160, "0h57m" },
};

bool RecordsScreen::render(const LocationData& /*data*/, LocationStatus /*st*/,
                           LocationProvider& /*loc*/, const ImVec2& /*displaySize*/)
{
    bool interacted = false;
    const ImGuiStyle& sty = ImGui::GetStyle();
    ImVec2 wp = ImGui::GetWindowPos();
    float w = ImGui::GetContentRegionAvail().x;

    // ---- 统计概览卡 ----
    float sumKm = 0.0f, sumUp = 0.0f;
    float sumH = 0.0f, sumM = 0.0f;
    for (const auto& t : kTracks)
    {
        sumKm += t.distKm;
        sumUp += t.upM;
        // 时长字符串 "XhYm" -> 累计分钟
        int h = 0, m = 0;
        sscanf(t.dur, "%dh%dm", &h, &m);
        sumH += h; sumM += m;
    }
    int totalMin = (int)sumH * 60 + (int)sumM;
    int totalH = totalMin / 60, remM = totalMin % 60;

    float statH = 72.0f;
    char vals[3][16];
    snprintf(vals[0], sizeof(vals[0]), "%.1f", sumKm);
    snprintf(vals[1], sizeof(vals[1]), "%.0f", sumUp);
    snprintf(vals[2], sizeof(vals[2]), "%dh%02dm", totalH, remM);
    StatStrip::Item items[3] = {
        { vals[0], "公里" }, { vals[1], "米上升" }, { vals[2], "总时长" },
    };
    StatStrip::Render(ImVec2(wp.x + sty.WindowPadding.x, wp.y + sty.WindowPadding.y),
                      ImVec2(w - sty.WindowPadding.x * 2.0f, statH), 16.0f, items, 3);

    ImGui::SetCursorScreenPos(ImVec2(wp.x, wp.y + sty.WindowPadding.y + statH + 12.0f));

    // ---- 轨迹卡片 ----
    float cardH = 64.0f;
    float cardGap = 10.0f;
    for (int i = 0; i < 3; ++i)
    {
        const auto& t = kTracks[i];
        ImVec2 cp = ImGui::GetCursorScreenPos();
        UI::DrawRoundedRect(cp, ImVec2(cp.x + w, cp.y + cardH),
                            IM_COL32(21, 27, 39, 255), 16.0f);

        // 信息
        ImGui::SetCursorScreenPos(ImVec2(cp.x + 14.0f, cp.y + 12.0f));
        ImGui::TextColored(ImVec4(0.97f, 0.98f, 0.99f, 1.0f), "%s", t.name);
        ImGui::SetCursorScreenPos(ImVec2(cp.x + 14.0f, cp.y + 36.0f));
        char meta[64];
        snprintf(meta, sizeof(meta), "%s · %.1f km · ↑%.0f m", t.date, t.distKm, (float)t.upM);
        ImGui::TextColored(ImVec4(0.39f, 0.46f, 0.56f, 1.0f), "%s", meta);

        // 时长
        ImGui::SetWindowFontScale(1.4f);
        ImVec2 dsz = ImGui::CalcTextSize(t.dur);
        ImGui::SetCursorScreenPos(ImVec2(cp.x + w - 14.0f - dsz.x, cp.y + cardH * 0.5f - dsz.y * 0.5f));
        ImGui::TextColored(ImVec4(0.31f, 0.71f, 1.0f, 1.0f), "%s", t.dur);
        ImGui::SetWindowFontScale(1.0f);

        // 命中区
        ImGui::SetCursorScreenPos(cp);
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##track", ImVec2(w, cardH)))
        {
            spdlog::info("[UI] 点击轨迹记录: {}", t.name);
            interacted = true;
        }
        if (ImGui::IsItemHovered())
            UI::DrawRoundedRect(cp, ImVec2(cp.x + w, cp.y + cardH),
                                IM_COL32(255, 255, 255, 10), 16.0f);
        ImGui::PopID();

        ImGui::SetCursorScreenPos(ImVec2(cp.x, cp.y + cardH + cardGap));
    }

    // 末尾补一个 0 尺寸占位项：消除“用 SetCursorPos 扩展父窗口边界”断言，并使 ##body 内容高度正确（滚动范围正确）
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
    return interacted;
}
