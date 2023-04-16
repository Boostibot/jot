#pragma once

#include "slice.h"

namespace jot
{
    template<typename T, isize size_>
    struct Static_Array
    {
        static constexpr isize size = size_;
        T data[size > 0 ? size : 1];
        
        using value_type      = T;
        using size_type       = size_t;
        using difference_type = ptrdiff_t;
        using reference       = T&;
        using const_reference = const T&;
        using iterator       = T*;
        using const_iterator = const T*;

        constexpr iterator       begin() noexcept       { return data; }
        constexpr const_iterator begin() const noexcept { return data; }
        constexpr iterator       end() noexcept         { return data + size; }
        constexpr const_iterator end() const noexcept   { return data + size; }

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

namespace std
{
    template<typename T, size_t N> constexpr 
    size_t size(jot::Static_Array<T, N> const& arr)   noexcept {return (size_t) arr.size;}

    template<typename T, size_t N> constexpr 
    auto data(jot::Static_Array<T, N> const& arr)     noexcept {return arr.data;}
}

#undef nodisc