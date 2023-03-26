#pragma once

#include <cstring>
#include <type_traits>
#include "slice.h"

#define cast(...) (__VA_ARGS__)
#define nodisc [[nodiscard]]

namespace jot
{
    template<typename T> nodisc constexpr 
    bool are_aliasing(Slice<T> left, Slice<T> right)
    { 
        uintptr_t left_pos = cast(uintptr_t) left.data;
        uintptr_t right_pos = cast(uintptr_t) right.data;
        if(right_pos < left_pos)
        {
            uintptr_t diff = left_pos - right_pos;
            return diff < cast(uintptr_t) right.size;
        }
        else
        {
            uintptr_t diff = right_pos - left_pos;
            return diff < cast(uintptr_t) left.size;
        }
    }

    template<typename T> nodisc constexpr 
    bool are_one_way_aliasing(Slice<T> before, Slice<T> after)
    { 
        return (before.data + before.size > after.data) && (after.data > before.data);
    }
    
    template <typename T>
    static constexpr bool is_byte_copyable = std::is_scalar_v<T> || std::is_trivially_copy_constructible_v<T>;
    
    template <typename T>
    static constexpr bool is_byte_comparable = std::is_scalar_v<T> && sizeof(T) == 1;

    template <typename T>
    static constexpr bool is_byte_nullable = std::is_scalar_v<T>;
    
    constexpr 
    bool is_const_eval(bool if_not_present = false) noexcept 
    {
        #ifdef __cpp_lib_is_constant_evaluated
            return std::is_constant_evaluated();
        #else
            return if_not_present;
        #endif
    }

    template<typename T> constexpr 
    void fill(Slice<T> to , T const& with) noexcept
    {
        for(isize i = 0; i < to.size; i++)
            to.data[i] = with;
    }

    template<typename T> constexpr 
    void null_items(Slice<T> to) noexcept
    {
        if(is_const_eval() == false && is_byte_nullable<T>)
        {
            memset(to.data, 0, to.size * sizeof(T));
            return;
        }

        fill(to, cast(T) 0);
    }

    template<typename T> nodisc constexpr 
    int compare(Slice<T> a, Slice<T> b) noexcept
    {
        if(a.size < b.size)
            return -1;

        if(a.size > b.size)
            return 1;

        if(is_const_eval() == false && std::is_scalar_v<T> && sizeof(T) == 1)
            return memcmp(a.data, b.data, a.size * sizeof(T));

        for(isize i = 0; i < a.size; i++)
        {
            if(a[i] < b[i])
                return -1;

            if(a[i] > b[i])
                return 1;
        }

        return 0;
    }

    template<typename T> constexpr 
    void copy_items(Slice<T> to, Slice<const T> from) noexcept
    {
        //we by default use memmove since its safer
        // (memcpy is still availible though under longer, uglier name)
        assert(to.size >= from.size && "size must be big enough");
        if(is_const_eval() == false && is_byte_copyable<T>)
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
    void copy_items_no_alias(Slice<T> to, Slice<const T> from) noexcept
    {
        assert(are_aliasing(*to, from) == false && "must not alias");
        assert(to.size >= from.size && "size must be big enough");
        
        if(is_const_eval() == false && is_byte_copyable<T>)
        {
            memcpy(to.data, from.data, from.size * sizeof(T));
        }
        else
        {
            for(isize i = 0; i < from.size; i++)
                to.data[i] = from.data[i];
        }
    }
    
    template<typename T> constexpr 
    void move_items(Slice<T> to, Slice<T> from) noexcept
    {
        assert(to.size >= from.size && "size must be big enough");
        if(is_const_eval() == false && is_byte_copyable<T>)
        {
            memmove(to.data, from.data, from.size * sizeof(T));
            return;
        }

        if(to.data < from.data)
        {
            for(isize i = 0; i < from.size; i++)
                to.data[i] = cast(T&&) from.data[i];
        }
        else
        {
            for(isize i = from.size; i-- > 0;)
                to.data[i] = cast(T&&) from.data[i];
        }
    }
}

#undef nodisc
#undef cast