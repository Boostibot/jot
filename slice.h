#pragma once 

#include <cassert>
#include <cstdint>

#if __cplusplus >= 202002L
//I dont know how to get rid of the dependency on this (sorry):
#include <iterator>
#endif

#define JOT_SLICE_INCLUDED

#define cast(...) (__VA_ARGS__)
#define nodisc [[nodiscard]]

namespace jot
{
    using isize = ptrdiff_t;

    //Open POD struct representing a contiguous array in memory. 
    //Does not own the data and is literally just a pointer and size analoguous to std::span except much simpler. 
    //All types which wish to be compatible must declare a slice(T) -> Slice<...> function which acts as a constructor
    // (this lets us define the struct in such a simple manner without losing on usability)
    //Is also used to represent strings (see string.h)
    template<typename T>
    struct Slice
    {
        T* data = nullptr;
        isize size = 0;

        constexpr operator Slice<const T>() const noexcept 
        { 
            return Slice<const T>{this->data, this->size}; 
        }

        #include "slice_operator_text.h"
    };

    template<typename T> constexpr 
    bool operator ==(Slice<T> const& a, Slice<T> const& b) noexcept 
    {
        return a.data == b.data && a.size == b.size;
    }
    
    template<typename T> constexpr 
    bool operator !=(Slice<T> const& a, Slice<T> const& b) noexcept 
    {
        return a.data != b.data || a.size != b.size;
    }
    
    //
    template<typename T, isize N> nodisc constexpr
    Slice<const T> slice(const T (&a)[N])
    {
        return Slice<const T>{a, N};
    }
    
    template<typename T, isize N> nodisc constexpr
    Slice<T> slice(T (*a)[N])
    {
        return Slice<T>{*a, N};
    }

    //surprisingly useful
    template<typename T> nodisc constexpr
    Slice<T> slice(Slice<T> s)
    {
        return s;
    }

    template<typename T> nodisc constexpr 
    Slice<T> tail(Slice<T> slice, isize from = 1) 
    {
        assert((0 <= from && from <= slice.size) && "index out of bounds");
        return Slice<T>{slice.data + from, slice.size - from};
    }

    template<typename T> nodisc constexpr 
    Slice<T> head(Slice<T> slice, isize to_index = 1) 
    {   
        assert((0 <= to_index && to_index <= slice.size) && "index out of bounds");
        return Slice<T>{slice.data, to_index};
    }

    template<typename T> nodisc constexpr 
    Slice<T> slice_portion(Slice<T> base_slice, isize from, isize size) 
    {
        return head(tail(base_slice, from), size);
    }

    template<typename T> nodisc constexpr 
    Slice<T> slice_range(Slice<T> base_slice, isize from, isize to) 
    {
        return tail(head(base_slice, to), from);
    }

    template<typename T> nodisc constexpr 
    isize byte_size(Slice<T> slice) 
    {
        return slice.size * cast(isize) sizeof(T);
    }

    template<typename To, typename From> nodisc
    Slice<To> cast_slice(Slice<From> slice)
    {
        return {cast(To*) cast(void*) slice.data, (byte_size(slice) / cast(isize) sizeof(To))};
    }

    #ifdef JOT_ARRAY_INCLUDED
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
    #endif // JOT_ARRAY_INCLUDED
}

namespace std
{
    template<typename T> nodisc constexpr 
    size_t size(jot::Slice<T> const& slice)   noexcept {return cast(size_t) slice.size;}

    template<typename T> nodisc constexpr 
    auto data(jot::Slice<T> const& slice)   noexcept {return slice.data;}
}

#undef nodisc
#undef cast
