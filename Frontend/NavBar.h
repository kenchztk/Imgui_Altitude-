#pragma once
#include "Frontend/Screen.h"

// 底部 pill 导航栏：四个等宽分段（图标 + 文字），选中段用强调色填充。
// 返回点击后应选中的屏幕（仅在点击时改变）。
class NavBar
{
public:
    static Screen render(Screen current);
};
