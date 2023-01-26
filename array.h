#pragma once

#include <cassert>
#include <cstdint>

//I am sorry I ahve to do this...
#include <compare>
#include <iterator>

namespace jot
{
    #ifndef JOT_SIZE_T
        using isize = ptrdiff_t;
    #endif

    template<typename T, isize size_>
    struct Array
    {
        static constexpr isize size = size_;
        static constexpr isize capacity = size;
        T data[size];

        constexpr bool operator==(const Array&) const noexcept = default;
        constexpr auto operator<=>(const Array&) const noexcept = default;
        
        #define DATA data
        #define SIZE size
        #include "slice_op_text.h"
    };

    template<typename T>
    struct Array<T, 0>
    {
        static constexpr isize size = 0;
        static constexpr isize capacity = 0;
        T data[1];

        constexpr bool operator==(const Array&) const noexcept   {return true;};
        constexpr auto operator<=>(const Array&) const noexcept  {return 0;};
        
        #include "slice_op_text.h"
        #undef DATA
        #undef SIZE
    };

    //deduction guide
    template <class First, class... Rest>
    Array(First, Rest...) -> Array<First, 1 + sizeof...(Rest)>;
}

#include "undefs.h"