#pragma once

// 全局用户偏好（单位体系 + 开关）。单例，供各屏读取；单位切换实时影响显示格式。
struct AppSettings
{
    enum class LengthUnit { Meter = 0, Foot = 1 };
    enum class PressureUnit { hPa = 0, mmHg = 1 };
    enum class TempUnit { Celsius = 0, Fahrenheit = 1 };

    LengthUnit length = LengthUnit::Meter;
    PressureUnit pressure = PressureUnit::hPa;
    TempUnit temp = TempUnit::Celsius;

    bool autoCalibrate = true;   // 自动校准
    bool highPrecision = false;  // 高精度模式
    bool egm96 = true;           // EGM96 大地水准面修正
    bool frosted = true;         // 毛玻璃背景
    int theme = 1;               // 0 浅色 / 1 深色 / 2 跟随系统（本版仅保存偏好）

    static AppSettings& Instance();

    // 单位换算 + 单位字符串
    double displayLength(double meters) const
    {
        return (length == LengthUnit::Foot) ? meters * 3.280839895013123 : meters;
    }
    const char* lengthUnit() const { return (length == LengthUnit::Foot) ? "ft" : "m"; }

    double displayPressure(double hpa) const
    {
        return (pressure == PressureUnit::mmHg) ? hpa * 0.7500616827 : hpa;
    }
    const char* pressureUnit() const { return (pressure == PressureUnit::mmHg) ? "mmHg" : "hPa"; }

    double displayTemp(double celsius) const
    {
        return (temp == TempUnit::Fahrenheit) ? celsius * 9.0 / 5.0 + 32.0 : celsius;
    }
    const char* tempUnit() const { return (temp == TempUnit::Fahrenheit) ? "°F" : "°C"; }
};
