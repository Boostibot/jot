#pragma once 

#include <cassert>
#include <cstdint>

#define nodisc [[nodiscard]]

namespace jot
{
    using isize = ptrdiff_t;

    //Open POD struct representing a contiguous array in memory or a string (see string.h). 
    //All types which wish to be compatible must declare a 
    // slice(T) -> Slice<...> function which acts as a constructor
    // (this lets us define the struct in such a simple manner without losing on usability)
    template<typename T>
    struct Slice
    {
        T* data = nullptr;
        isize size = 0;
        
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
        
        nodisc constexpr operator Slice<const T>() const noexcept 
        { 
            return Slice<const T>{data, size}; 
        }
    };
    
    template<typename T, isize N> nodisc constexpr
    Slice<const T> slice(const T (&a)[N]) noexcept
    {
        return Slice<const T>{a, N};
    }
    
    template<typename T, isize N> nodisc constexpr
    Slice<T> slice(T (*a)[N]) noexcept
    {
        return Slice<T>{*a, N};
    }
    
    template<typename T> nodisc constexpr
    Slice<T> slice(Slice<T> s) noexcept
    {
        //surprisingly useful
        return s;
    }

    template<typename T> nodisc constexpr 
    Slice<T> tail(Slice<T> slice, isize from = 1) noexcept
    {
        assert((0 <= from && from <= slice.size) && "index out of bounds");
        return Slice<T>{slice.data + from, slice.size - from};
    }

    template<typename T> nodisc constexpr 
    Slice<T> head(Slice<T> slice, isize to_index = 1) noexcept
    {   
        assert((0 <= to_index && to_index <= slice.size) && "index out of bounds");
        return Slice<T>{slice.data, to_index};
    }

    template<typename T> nodisc constexpr 
    Slice<T> slice_portion(Slice<T> base_slice, isize from, isize size) noexcept
    {
        return head(tail(base_slice, from), size);
    }

    template<typename T> nodisc constexpr 
    Slice<T> slice_range(Slice<T> base_slice, isize from, isize to) noexcept
    {
        return tail(head(base_slice, to), from);
    }

    template<typename To, typename From> nodisc
    Slice<To> cast_slice(Slice<From> slice) noexcept
    {
        isize new_size = slice.size * (isize) sizeof(From) / (isize) sizeof(To);
        return {(To*) (void*) slice.data, new_size};
    }
  
    //this is technically unrelated to slice but since slice is a 
    // centerpoint of this entire library it makes sense to put it here
    template <typename T> nodisc constexpr 
    T && move(T* val) noexcept 
    { 
        return (T &&) *val; 
    };

    template <typename T> nodisc constexpr 
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
    template<typename T> nodisc constexpr 
    size_t size(jot::Slice<T> const& slice)   noexcept {return (size_t) slice.size;}

    template<typename T> nodisc constexpr 
    auto data(jot::Slice<T> const& slice)   noexcept {return slice.data;}
}

#undef nodisc
