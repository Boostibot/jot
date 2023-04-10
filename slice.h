#pragma once 

#include <cassert>
#include <cstddef>

using isize_t = ptrdiff_t;
using usize_t = size_t;

using isize = ptrdiff_t;
using usize = size_t;

namespace jot
{
    //Open POD struct representing a contiguous array in memory or a string (see string.h). 
    //All types which wish to be compatible must declare a 
    // slice(T) -> Slice<...> function which acts as a constructor
    // (this lets us define the struct in such a simple manner without losing on usability)
    template<typename T>
    struct Slice
    {
        T* data = nullptr;
        isize_t size = 0;
        
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

        constexpr T const& operator[](isize_t index) const noexcept  
        { 
            assert(0 <= index && index < size && "index out of range"); 
            return data[index]; 
        }
        constexpr T& operator[](isize_t index) noexcept 
        { 
            assert(0 <= index && index < size && "index out of range"); 
            return data[index]; 
        }
        
        constexpr operator Slice<const T>() const noexcept 
        { 
            return Slice<const T>{data, size}; 
        }
    };
    
    template<typename T, isize_t N> constexpr
    Slice<const T> slice(const T (&a)[N]) noexcept
    {
        return Slice<const T>{a, N};
    }
    
    template<typename T, isize_t N> constexpr
    Slice<T> slice(T (*a)[N]) noexcept
    {
        return Slice<T>{*a, N};
    }
    
    template<typename T> constexpr
    Slice<T> slice(Slice<T> s) noexcept
    {
        //surprisingly useful
        return s;
    }

    template<typename T> constexpr 
    Slice<T> tail(Slice<T> slice, isize_t from = 1) noexcept
    {
        assert((0 <= from && from <= slice.size) && "index out of bounds");
        return Slice<T>{slice.data + from, slice.size - from};
    }

    template<typename T> constexpr 
    Slice<T> head(Slice<T> slice, isize_t to_index = 1) noexcept
    {   
        assert((0 <= to_index && to_index <= slice.size) && "index out of bounds");
        return Slice<T>{slice.data, to_index};
    }

    template<typename T> constexpr 
    Slice<T> slice_portion(Slice<T> base_slice, isize_t from, isize_t size) noexcept
    {
        return head(tail(base_slice, from), size);
    }

    template<typename T> constexpr 
    Slice<T> slice_range(Slice<T> base_slice, isize_t from, isize_t to) noexcept
    {
        return tail(head(base_slice, to), from);
    }

    template<typename To, typename From>
    Slice<To> cast_slice(Slice<From> slice) noexcept
    {
        isize_t new_size = slice.size * (isize_t) sizeof(From) / (isize_t) sizeof(To);
        return {(To*) (void*) slice.data, new_size};
    }
  
    //this is technically unrelated to slice but since slice is a 
    // centerpoint of this entire library it makes sense to put it here
    constexpr isize_t max(isize_t a, isize_t b)
    {
        return a > b ? a : b;
    }

    constexpr isize_t min(isize_t a, isize_t b)
    {
        return a < b ? a : b;
    }

    constexpr isize_t clamp(isize_t val, isize_t lo, isize_t hi)
    {
        return max(lo, min(val, hi));
    }

    constexpr isize_t div_round_up(isize_t value, isize_t to_multiple_of)
    {
        return (value + to_multiple_of - 1) / to_multiple_of;
    }

    template <typename T> constexpr 
    T && move(T* val) noexcept 
    { 
        return (T &&) *val; 
    };

    template <typename T> constexpr 
    void swap(T* a, T* b) noexcept 
    { 
        //we dont use the move function here because we want to insteatiate
        // as little templates as possible
        T copy = (T&&) *a;
        *a = (T&&) *b;
        *b = (T&&) copy;
    };
}

namespace std
{
    template<typename T> constexpr 
    size_t size(jot::Slice<T> const& slice)   noexcept {return (size_t) slice.size;}

    template<typename T> constexpr 
    auto data(jot::Slice<T> const& slice)   noexcept {return slice.data;}
}
