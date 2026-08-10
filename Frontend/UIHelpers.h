#pragma once
#include "imgui/imgui.h"

// ---------------------------------------------------------------------------
// UIHelpers：应用层共享控件/绘制 helper。
// 目的：把 Frontend 各屏里重复手绘的 ImDrawList 代码收口到一处，
//       保持与现有像素一致的观感，零外部依赖。
// 用法：各屏 `#include "Frontend/UIHelpers.h"` 后调用 `UI::xxx(...)`。
// ---------------------------------------------------------------------------

namespace UI {

// 居中文本：基于当前窗口坐标，posY 为相对窗口顶部的垂直偏移（像素）。
// 复刻原 RealtimeScreen 内联 CenteredText 的语义。
void CenteredText(const char* text, float scale, const ImVec4& col, float winW, float posY);

// 圆角矩形：一行替代各屏重复的 `dl->AddRectFilled(..., rounding, RoundCornersAll)`。
void DrawRoundedRect(const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float rounding);

// 主题强调色（青蓝 accent）。
ImU32 accentCyan();

// 背景细圆环（各 hero 共用的装饰环）。
void DrawRing(ImVec2 center, float R, ImU32 col, float thickness = 1.5f, int segments = 80);

// 进度弧（海拔 hero 的圆环进度；其余 hero 仅用 DrawRing 即可）。
void DrawGaugeArc(ImVec2 center, float R, float ratio, float a0, float sweep,
                  ImU32 col, float thickness = 6.0f, int segments = 80);

} // namespace UI
