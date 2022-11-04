#pragma once

#include <cassert>

#include "types.h"
#include "meta.h"
#include "slice.h"
#include "array.h"

#include "defines.h"

namespace jot 
{
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
        requires (std::convertible_to<Ts, T> && ...)
    func max(T first, Ts... values) -> T
    {
        let rest_max = max(values...);
        if(rest_max > first)
            return cast(T) rest_max;
        else
            return cast(T) first;
    }

    template<typename T, typename ...Ts>
        requires (std::convertible_to<Ts, T> && ...)
    func min(T first, Ts... values...) -> T
    {
        let rest_max = max(values...);
        if(rest_max < first)
            return cast(T) rest_max;
        else
            return cast(T) first;
    }

    template<typename T>
    func div_round_up(T value, no_infer(T) to_multiple_of) -> auto
    {
        return (value + to_multiple_of - 1) / to_multiple_of;
    }
}


#include "undefs.h"