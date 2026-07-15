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

Backend::Backend()
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
