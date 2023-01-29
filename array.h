#pragma once

#include <cassert>
#include <cstdint>

//I am sorry I have to do this...
#if __cplusplus >= 202002L
#include <compare>
#endif
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

        #if __cplusplus >= 202002L
        constexpr bool operator==(const Array&) const noexcept = default;
        constexpr auto operator<=>(const Array&) const noexcept = default;
        #endif
        
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
        
        #if __cplusplus >= 202002L
        constexpr bool operator==(const Array&) const noexcept   {return true;};
        constexpr auto operator<=>(const Array&) const noexcept  {return 0;};
        #endif
        
        #include "slice_op_text.h"
        #undef DATA
        #undef SIZE
    };

    //deduction guide
    template <class First, class... Rest>
    Array(First, Rest...) -> Array<First, 1 + sizeof...(Rest)>;
}

namespace std
{
    template<typename T, size_t N> [[nodiscard]] constexpr 
    size_t size(jot::Array<T, N> const& arr)   noexcept {return (size_t) arr.size;}

    template<typename T, size_t N> [[nodiscard]] constexpr 
    auto data(jot::Array<T, N> const& arr)     noexcept {return arr.data;}
}

#include "undefs.h"