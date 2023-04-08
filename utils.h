#pragma once

//for windows.h which defines min max macros
#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

#include <cstddef>
#define nodisc [[nodiscard]]

namespace jot 
{
    using isize = ptrdiff_t;

    nodisc constexpr 
    isize max(isize a, isize b)
    {
        return a > b ? a : b;
    }

    nodisc constexpr 
    isize min(isize a, isize b)
    {
        return a < b ? a : b;
    }

    nodisc constexpr 
    isize clamp(isize val, isize lo, isize hi)
    {
        return max(lo, min(val, hi));
    }

    nodisc constexpr
    float lerp(float lo, float hi, float t) 
    {
        return lo * (1.0f - t) + hi * t;
    }

    nodisc constexpr
    double lerp(double lo, double hi, double t) 
    {
        return lo * (1.0 - t) + hi * t;
    }

    nodisc constexpr 
    isize div_round_up(isize value, isize to_multiple_of)
    {
        return (value + to_multiple_of - 1) / to_multiple_of;
    }

}

#undef nodisc
