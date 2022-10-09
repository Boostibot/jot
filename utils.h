#pragma once

#include <cassert>

#include "types.h"
#include "meta.h"
#include "slice.h"
#include "array.h"

#include "defines.h"

namespace jot 
{
    template <typename Fn>
        requires std::is_invocable_v<Fn>
    proc run_in_mode(Fn fn, bool conmptime) -> void
    {
        if(std::is_constant_evaluated() == conmptime)
            fn();
    }

    #define runtime_only(...) run_in_mode([&]{__VA_ARGS__}, false);
    #define comptime_only(...) run_in_mode([&]{__VA_ARGS__}, true);

    //@TODO: move min max out of this file and add to utils (add clamp and circular modulo)
    template<typename T>
    func max() -> T
    {   
        return T();
    }

    template<typename T>
    func min() -> T
    {   
        return T();
    }

    template<typename T>
    func max(T first) -> T
    {   
        return first;
    }

    template<typename T>
    func min(T first) -> T
    {   
        return first;
    }

    template<typename T, typename ...Ts>
    func max(T first, Ts... values)
        requires (std::convertible_to<Ts, T> && ...)
    {
        let rest_max = max(values...);
        if(rest_max > first)
            return cast(T) rest_max;
        else
            return cast(T) first;
    }

    template<typename T, typename ...Ts>
    func min(T first, Ts... values...)
        requires (std::convertible_to<Ts, T> && ...)
    {
        let rest_max = max(values...);
        if(rest_max < first)
            return cast(T) rest_max;
        else
            return cast(T) first;
    }

    func div_round_up(auto value, auto to_multiple_of) -> auto
    {
        return (value + to_multiple_of - 1) / to_multiple_of;
    }
}


#include "undefs.h"