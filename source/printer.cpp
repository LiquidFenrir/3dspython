#include "printer.h"

#include <cstdio>
#include <cstdarg>

extern "C" void app_recv_str(const char* str, size_t len)
{
    if(Printer::callback)
    {
        Printer::callback(Printer::payload, {str, len});
    }
}

extern "C" int DEBUG_printf(const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    const int ret = vfprintf(stderr, format, vl);
    va_end(vl);
    return ret;
}
