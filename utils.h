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
    using No_Infer = No_Infer_Impl<T>::type;

    #define no_infer(...) No_Infer<__VA_ARGS__> 


    #define templ template<typename T> constexpr [[nodiscard]]

    templ T max(T a, no_infer(T) b) noexcept{
        return a > b ? a : b;
    }

    templ T min(T a, no_infer(T) b) noexcept{
        return a < b ? a : b;
    }

    templ T clamp(T val, no_infer(T) lo, no_infer(T) hi) noexcept{
        return max(lo, min(val, hi));
    }

    templ float lerp(float lo, float hi, float t) noexcept {
        return lo * (1.0f - t) + hi * t;
    }

    templ double lerp(double lo, double hi, double t) noexcept {
        return lo * (1.0 - t) + hi * t;
    }

    templ T div_round_up(T value, no_infer(T) to_multiple_of) noexcept{
        return (value + to_multiple_of - 1) / to_multiple_of;
    }

    #undef templ
}
