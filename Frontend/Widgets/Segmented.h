#pragma once
#include "imgui/imgui.h"

// 分段控件：NavBar / 指标切换 / 设置单位·主题 复用。
// 仿 imgui-knobs 形态：单一命名空间 + 立即模式函数，调用方只关心返回值。
namespace Segmented {

// 分段控件外观配置（默认值对应「指标切换 / 设置分段」的通用外观）
struct Style
{
    ImU32 bg       = IM_COL32(21, 27, 39, 255);   // 整条背景胶囊色（drawBg=false 时不绘制）
    ImU32 selBg    = IM_COL32(80, 180, 255, 255);  // 选中段背景
    ImU32 selText  = IM_COL32(8, 15, 30, 255);     // 选中段文字
    ImU32 idleText = IM_COL32(100, 116, 139, 255); // 未选中段文字
    float gap      = 4.0f;   // 段间距
    float inset    = 0.0f;   // 选中高亮在段内收缩像素（NavBar 用 4）
    float rounding = 0.0f;   // 选中段圆角，0 = 自动取 h*0.5
    bool  drawBg   = true;   // 是否绘制整条背景胶囊
    bool  hover    = false;  // 未选中段 hover 时画一层白色叠色
};

// 返回点击后的选中索引；未点击则返回 current。
int Render(const char** labels, int count, int current, float totalW, float h,
           const Style& style = {});

} // namespace Segmented
