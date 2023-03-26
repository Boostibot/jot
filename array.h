#pragma once

#include <cassert>
#include <cstdint>

#if __cplusplus >= 202002L
#include <iterator>
#endif

#define JOT_ARRAY_INCLUDED

namespace jot
{
    using isize = ptrdiff_t;

    template<typename T, isize size_>
    struct Array
    {
        static constexpr isize size = size_;
        static constexpr isize capacity = size;
        T data[size > 0 ? size : 1];
        
        #include "slice_operator_text.h"
    };

    //deduction guide
    template <class First, class... Rest>
    Array(First, Rest...) -> Array<First, 1 + sizeof...(Rest)>;

    #ifdef JOT_SLICE_INCLUDED
    template<typename T, isize N> [[nodiscard]]
    Slice<const T> slice(Array<T, N> const& arr) 
    {
        return Slice<const T>{arr.data, N};
    }

    template<typename T, isize N> [[nodiscard]]
    Slice<T> slice(Array<T, N>* arr) 
    {
        return Slice<T>{arr->data, N};
    }
    #endif // JOT_SLICE_INCLUDED
}

namespace std
{
    template<typename T, size_t N> [[nodiscard]] constexpr 
    size_t size(jot::Array<T, N> const& arr)   noexcept {return (size_t) arr.size;}

    template<typename T, size_t N> [[nodiscard]] constexpr 
    auto data(jot::Array<T, N> const& arr)     noexcept {return arr.data;}
}

