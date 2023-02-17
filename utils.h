#pragma once

//for windows.h which defines min max macros
#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

namespace jot 
{
    //stops infering of arguments 
    // without it for example min(cast(u32) 1, 1) doesnt compile 
    // since the first arg is u32 and second arg is infered as int
    // which conflict
    template<typename T>
    struct No_Infer_Impl { using type = T; };

    template<typename T>
    using No_Infer = typename No_Infer_Impl<T>::type;

    #define no_infer(...) No_Infer<__VA_ARGS__> 
    #define nodisc [[nodiscard]]

    template<typename T> nodisc constexpr 
    T max(T a, no_infer(T) b) noexcept
    {
        return a > b ? a : b;
    }

    template<typename T> nodisc constexpr 
    T min(T a, no_infer(T) b) noexcept
    {
        return a < b ? a : b;
    }

    template<typename T> nodisc constexpr 
    T clamp(T val, no_infer(T) lo, no_infer(T) hi) noexcept
    {
        return max(lo, min(val, hi));
    }

    float lerp(float lo, float hi, float t) noexcept 
    {
        return lo * (1.0f - t) + hi * t;
    }

    double lerp(double lo, double hi, double t) noexcept 
    {
        return lo * (1.0 - t) + hi * t;
    }

    template<typename T> nodisc constexpr 
    T div_round_up(T value, no_infer(T) to_multiple_of) noexcept
    {
        return (value + to_multiple_of - 1) / to_multiple_of;
    }

    #undef nodisc
}
