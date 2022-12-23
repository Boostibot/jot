#pragma once

#include <cassert>

#include "traits.h"
#include "defines.h"

namespace jot 
{
    #define templ_func template<typename T> constexpr func

    templ_func max(T in first) noexcept -> T in {   
        return first;
    }

    templ_func min(T in first) noexcept -> T in {   
        return first;
    }

    templ_func max(T in a, no_infer(T) in b) noexcept -> T in {
        return a > b ? a : b;
    }

    templ_func min(T in a, no_infer(T) in b) noexcept -> T in {
        return a < b ? a : b;
    }

    templ_func div_round_up(T in value, no_infer(T) in to_multiple_of) noexcept -> T {
        return (value + to_multiple_of - 1) / to_multiple_of;
    }

    #undef templ_func
}


#include "undefs.h"