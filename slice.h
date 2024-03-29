#pragma once 

#include <assert.h>
#include <stddef.h>
#include <string.h>

using isize = ptrdiff_t;
using usize = size_t;

namespace jot
{
    #ifndef SLICE_DEFINED
    #define SLICE_DEFINED
    ///Sized pointer to contiguous array in memory or a string
    template<typename T>
    struct Slice
    {
        T* data = nullptr;
        isize size = 0;

        //All types which wish to be compatible must declare a 
        // slice(T) -> Slice<...> function which acts as a constructor
        // (this lets us define the struct in such a simple manner without losing on usability)
        constexpr T const& operator[](isize index) const noexcept  
        { 
            assert(0 <= index && index < size && "index out of range"); return data[index]; 
        }

        constexpr T& operator[](isize index) noexcept 
        { 
            assert(0 <= index && index < size && "index out of range"); return data[index]; 
        }
        
        constexpr operator Slice<const T>() const noexcept 
        { 
            return Slice<const T>{data, size}; 
        }
    };
    #endif

    template<typename T> constexpr T*       begin(Slice<T>& slice) noexcept         { return slice.data; }
    template<typename T> constexpr const T* begin(Slice<T> const& slice) noexcept   { return slice.data; }
    template<typename T> constexpr T*       end(Slice<T>& slice) noexcept           { return slice.data + slice.size; }
    template<typename T> constexpr const T* end(Slice<T> const& slice) noexcept     { return slice.data + slice.size; }
    
    template<typename T, isize N> constexpr
    Slice<const T> slice(const T (&a)[N]) noexcept
    {
        return Slice<const T>{a, N};
    }
    
    template<typename T, isize N> constexpr
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
    Slice<T> tail(Slice<T> slice, isize from = 1) noexcept
    {
        assert(0 <= from && from <= slice.size && "index out of bounds");
        return Slice<T>{slice.data + from, slice.size - from};
    }

    template<typename T> constexpr 
    Slice<T> head(Slice<T> slice, isize to_index = 1) noexcept
    {   
        assert(0 <= to_index && to_index <= slice.size && "index out of bounds");
        return Slice<T>{slice.data, to_index};
    }
    
    template<typename T> constexpr 
    Slice<T> limit(Slice<T> slice, isize max_size) noexcept
    {   
        assert(0 <= max_size && "index out of bounds");
        if(slice.size > max_size)
            slice.size = max_size;

        return slice;
    }

    template<typename T> constexpr 
    Slice<T> slice_portion(Slice<T> base_slice, isize from, isize size) noexcept
    {
        return head(tail(base_slice, from), size);
    }

    template<typename T> constexpr 
    Slice<T> slice_range(Slice<T> base_slice, isize from, isize to) noexcept
    {
        return tail(head(base_slice, to), from);
    }
    
    template<typename T> constexpr
    inline bool is_in_slice(T* ptr, Slice<T> slice) noexcept
    {
        return ptr >= slice.data && ptr <= slice.data + slice.size;
    }

    template<typename To, typename From>
    Slice<To> cast_slice(Slice<From> slice) noexcept
    {
        isize new_size = slice.size * (isize) sizeof(From) / (isize) sizeof(To);
        return {(To*) (void*) slice.data, new_size};
    }
    
    template<typename T> constexpr 
    bool is_aliasing(Slice<T> left, Slice<T> right)
    { 
        uintptr_t left_pos = (uintptr_t) left.data;
        uintptr_t right_pos = (uintptr_t) right.data;
        if(right_pos < left_pos)
        {
            uintptr_t diff = left_pos - right_pos;
            return diff < (uintptr_t) right.size;
        }
        else
        {
            uintptr_t diff = right_pos - left_pos;
            return diff < (uintptr_t) left.size;
        }
    }

    template<typename T> constexpr 
    bool is_front_aliasing(Slice<T> before, Slice<T> after)
    { 
        return (before.data + before.size > after.data) && (after.data > before.data);
    }

    template<typename T> 
    void null_bytes(Slice<T> to) noexcept
    {
        memset(to.data, 0, to.size * sizeof(T));
    }
    
    template<typename T> 
    bool are_bytes_equal(Slice<T> a, Slice<T> b) noexcept
    {
        if(a.size != b.size)
            return false;

        return memcmp(a.data, b.data, a.size * sizeof(T)) == 0;
    }
    
    template<typename T> 
    void copy_bytes(Slice<T> to, Slice<const T> from) noexcept
    {
        //we by default use memmove since its safer and means less worry later on
        assert(to.size >= from.size && "size must be big enough");
        memmove(to.data, from.data, from.size * sizeof(T));
    }
    
    template<typename T> 
    void copy_bytes(Slice<T> to, Slice<T> from) noexcept
    {
        return copy_bytes<T>(to, (Slice<const T>) from);
    }
    
    template<typename T> 
    void set_items(Slice<T> to, T const& with) noexcept
    {
        for(isize i = 0; i < to.size; i++)
            to.data[i] = with;
    }
    
    template<typename T> 
    bool are_items_equal(Slice<T> a, Slice<T> b) noexcept
    {
        if(a.size != b.size)
            return false;

        for(isize i = 0; i < a.size; i++)
            if(a[i] != b[i])
                return false;

        return true;
    }

    template<typename T> 
    void copy_items(Slice<T> to, Slice<const T> from) noexcept
    {
        //we by default use memmove since its safer and means less worry later on
        assert(to.size >= from.size && "size must be big enough");
        if(__is_trivially_copyable(T))
        {
            memmove(to.data, from.data, from.size * sizeof(T));
            return;
        }

        if(to.data < from.data)
        {
            for(isize i = 0; i < from.size; i++)
                to.data[i] = from.data[i];
        }
        else
        {
            for(isize i = from.size; i-- > 0;)
                to.data[i] = from.data[i];
        }
    }

    template<typename T> constexpr 
    void move_items(Slice<T> to, Slice<T> from) noexcept
    {
        assert(to.size >= from.size && "size must be big enough");
        if(__is_trivially_copyable(T))
        {
            memmove(to.data, from.data, from.size * sizeof(T));
            return;
        }

        if(to.data < from.data)
        {
            for(isize i = 0; i < from.size; i++)
                to.data[i] = (T&&) from.data[i];
        }
        else
        {
            for(isize i = from.size; i-- > 0;)
                to.data[i] = (T&&) from.data[i];
        }
    }

    template<typename T> 
    void copy_items(Slice<T> to, Slice<T> from) noexcept
    {
        return copy_items<T>(to, (Slice<const T>) from);
    }
}