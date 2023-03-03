#pragma once
#include <cstdint>

#ifndef _MSC_VER
#define __FUNCTION__ __func__
#endif

namespace jot
{
    struct Line_Info
    {
        const char* file;
        const char* func;
        uint32_t line;
    };

    #define GET_LINE_INFO() Line_Info{__FILE__, __FUNCTION__, (uint32_t) (__LINE__)}

    struct Panic
    {
        const char* message;
        Line_Info line_info;
    };

    template<typename T>
    struct Any_Panic : Panic
    {
        T value;
    };

    #define panic(message)          throw Panic{message, GET_LINE_INFO()}
    #define panic_any(val, message) throw Any_Panic<decltype(val)>{Panic{message, GET_LINE_INFO()}, val}
}
