#include "printer.h"

#include <cstdio>
#include <cstdarg>

void Printer::print(std::string_view str)
{
    if(Printer::callback && Printer::payload)
    {
        Printer::callback(Printer::payload, str);
    }
}

extern "C" void my_stdout_strn(const char * str, size_t len)
{
    Printer::print({str, len});
}

extern "C" int DEBUG_printf(const char* format, ...)
{
    va_list vl;
    va_start(vl, format);
    const int ret = vfprintf(stderr, format, vl);
    va_end(vl);
    return ret;
}