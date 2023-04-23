#pragma once
#include "slice.h"

namespace jot
{
    template<typename T, isize size_>
    struct Static_Array
    {
        static constexpr isize size = size_;
        T data[size > 0 ? size : 1];

        constexpr T*       begin() noexcept       { return data; }
        constexpr const T* begin() const noexcept { return data; }
        constexpr T*       end() noexcept         { return data + size; }
        constexpr const T* end() const noexcept   { return data + size; }

        constexpr T const& operator[](isize index) const noexcept  
        { 
            assert(0 <= index && index < size && "index out of range"); 
            return data[index]; 
        }
        constexpr T& operator[](isize index) noexcept 
        { 
            assert(0 <= index && index < size && "index out of range"); 
            return data[index]; 
        }
    };

    template<typename T, isize N> constexpr
    Slice<const T> slice(Static_Array<T, N> const& arr) 
    {
        return Slice<const T>{arr.data, N};
    }

    template<typename T, isize N> constexpr
    Slice<T> slice(Static_Array<T, N>* arr) 
    {
        return Slice<T>{arr->data, N};
    }
}
