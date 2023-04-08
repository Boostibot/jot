#pragma once

#include "slice.h"

namespace jot
{
    #define nodisc [[nodiscard]]
    template<typename T, isize size_>
    struct Array
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

        nodisc constexpr iterator       begin() noexcept       { return data; }
        nodisc constexpr const_iterator begin() const noexcept { return data; }
        nodisc constexpr iterator       end() noexcept         { return data + size; }
        nodisc constexpr const_iterator end() const noexcept   { return data + size; }

        nodisc constexpr T const& operator[](isize index) const noexcept  
        { 
            assert(0 <= index && index < size && "index out of range"); 
            return data[index]; 
        }
        nodisc constexpr T& operator[](isize index) noexcept 
        { 
            assert(0 <= index && index < size && "index out of range"); 
            return data[index]; 
        }
    };

    template<typename T, isize N> nodisc
    Slice<const T> slice(Array<T, N> const& arr) 
    {
        return Slice<const T>{arr.data, N};
    }

    template<typename T, isize N> nodisc
    Slice<T> slice(Array<T, N>* arr) 
    {
        return Slice<T>{arr->data, N};
    }
}

namespace std
{
    template<typename T, size_t N> nodisc constexpr 
    size_t size(jot::Array<T, N> const& arr)   noexcept {return (size_t) arr.size;}

    template<typename T, size_t N> nodisc constexpr 
    auto data(jot::Array<T, N> const& arr)     noexcept {return arr.data;}
}

#undef nodisc