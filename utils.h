#pragma once

//for windows.h which defines min max macros
#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

#include <stddef.h>
#define JOT_API [[nodiscard]] constexpr

namespace jot 
{
    typedef ptrdiff_t isize;

    JOT_API isize max(isize a, isize b)
    {
        return a > b ? a : b;
    }

    JOT_API isize min(isize a, isize b)
    {
        return a < b ? a : b;
    }

    JOT_API isize clamp(isize val, isize lo, isize hi)
    {
        return max(lo, min(val, hi));
    }

    JOT_API float lerp(float lo, float hi, float t) 
    {
        return lo * (1.0f - t) + hi * t;
    }

    JOT_API double lerp(double lo, double hi, double t) 
    {
        return lo * (1.0 - t) + hi * t;
    }

    JOT_API isize div_round_up(isize value, isize to_multiple_of)
    {
        return (value + to_multiple_of - 1) / to_multiple_of;
    }
}

#undef JOT_API
