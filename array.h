#pragma once

#include <cassert>
#include <cstdint>
#include <iterator>

#include "slice.h"

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
        T data[size > 0 ? size : 1];
        
        #include "slice_op_text.h"
    };

    //deduction guide
    template <class First, class... Rest>
    Array(First, Rest...) -> Array<First, 1 + sizeof...(Rest)>;

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
}

namespace std
{
    template<typename T, size_t N> [[nodiscard]] constexpr 
    size_t size(jot::Array<T, N> const& arr)   noexcept {return (size_t) arr.size;}

    template<typename T, size_t N> [[nodiscard]] constexpr 
    auto data(jot::Array<T, N> const& arr)     noexcept {return arr.data;}
}

