#include "Frontend/AppSettings.h"

AppSettings& AppSettings::Instance()
{
    static AppSettings s;
    return s;
}
