#pragma once

#include <cstring>
#include "slice.h"

#define cast(...) (__VA_ARGS__)
#define nodisc [[nodiscard]]

namespace jot
{
    enum class Type_Optims : uint32_t 
    {
        NONE = 0,

        //If type can be safely constructed by and assigned by memcopying bytes into it
        BYTE_COPY = 1,
        BYTE_MOVE = 2,

        //If slice of this type can be safely compared for equality using memcmp
        BYTE_EQUALS = 4,

        //If destructor can be safely ignored (useful when placing asserts in destructors that would otherwise be empty)
        BYTE_DESTRUCT = 8,
        
        //If type can be constructed/assigned to by setting all of its bytes to 0
        BYTE_NULL = 16,
        
        //If it is safe to instead of: move constructing copy and destructing original
        //                    only do: copy its bytes over and not destruct the original
        //Most types behave like this since the only thing that is required for this condition to be met is 
        // that the object does not "know" its own adress. Its however pretty much impossible to prove generally.
        //If this is enabled common slice (and stack) operations become much faster.
        BYTE_TRANSFER = 32,

        ALL_SET = 63,
        POD = 63,
        DEF = BYTE_TRANSFER, //Most types fit this 
    };
    
    constexpr Type_Optims operator | (Type_Optims a, Type_Optims b) { return cast(Type_Optims) (a | b); }
    constexpr Type_Optims operator & (Type_Optims a, Type_Optims b) { return cast(Type_Optims) (a & b); }

    constexpr bool is_flag_set(Type_Optims optims, Type_Optims flag)
    {
        return (cast(uint32_t) optims & cast(uint32_t) flag) > 0;
    }

    template<typename T>
    constexpr static Type_Optims DEF_TYPE_OPTIMS = __is_pod(T) ? Type_Optims::POD : Type_Optims::DEF;
    
    template<typename T>
    constexpr static Type_Optims DEF_TYPE_OPTIMS<Slice<T>> = Type_Optims::POD;

    template<typename T> nodisc constexpr 
    bool is_aliasing(Slice<T> left, Slice<T> right)
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
    bool is_front_aliasing(Slice<T> before, Slice<T> after)
    { 
        return (before.data + before.size > after.data) && (after.data > before.data);
    }
    
    template<typename T> constexpr 
    void fill(Slice<T> to , T const& with) noexcept
    {
        for(isize i = 0; i < to.size; i++)
            to.data[i] = with;
    }

    template<typename T> constexpr 
    void null_items(Slice<T> to, Type_Optims optims = DEF_TYPE_OPTIMS<T>) noexcept
    {
        if(is_flag_set(optims, Type_Optims::BYTE_NULL))
        {
            memset(to.data, 0, to.size * sizeof(T));
            return;
        }

        fill(to, cast(T) 0);
    }

    //@TODO: rename to format is + something
    template<typename T> nodisc constexpr 
    bool are_equal(Slice<T> a, Slice<T> b, Type_Optims optims = DEF_TYPE_OPTIMS<T>) noexcept
    {
        if(a.size != b.size)
            return false;

        if(is_flag_set(optims, Type_Optims::BYTE_EQUALS))
            return memcmp(a.data, b.data, a.size * sizeof(T)) == 0;

        for(isize i = 0; i < a.size; i++)
        {
            if(a[i] != b[i])
                return false;
        }

        return true;
    }

    template<typename T> constexpr 
    void copy_items(Slice<T> to, Slice<const T> from, Type_Optims optims = DEF_TYPE_OPTIMS<T>) noexcept
    {
        //we by default use memmove since its safer and means less
        // worry later on
        assert(to.size >= from.size && "size must be big enough");
        if(is_flag_set(optims, Type_Optims::BYTE_COPY))
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
    void copy_items(Slice<T> to, Slice<T> from, Type_Optims optims = DEF_TYPE_OPTIMS<T>)
    {
        return copy_items<T>(to, cast(Slice<const T>) from, optims);
    }
    
    template<typename T> constexpr 
    void move_items(Slice<T> to, Slice<T> from, Type_Optims optims = DEF_TYPE_OPTIMS<T>) noexcept
    {
        assert(to.size >= from.size && "size must be big enough");
        if(is_flag_set(optims, Type_Optims::BYTE_MOVE))
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

    template<typename T> constexpr 
    void copy_construct_items(Slice<T> to, Slice<const T> from, Type_Optims optims = DEF_TYPE_OPTIMS<T>)
    {
        if(is_flag_set(optims, Type_Optims::BYTE_COPY))
            memmove(to.data, from.data, from.size * sizeof(T));
        else
        {
            assert(to.size >= from.size && "size must be big enough");
            for(isize i = 0; i < from.size; i++)
                new(to.data + i) T(from.data[i]);
        }
    }
    
    template<typename T> constexpr 
    void move_construct_items(Slice<T> to, Slice<T> from, Type_Optims optims = DEF_TYPE_OPTIMS<T>) noexcept
    {
        if(is_flag_set(optims, Type_Optims::BYTE_MOVE))
            memmove(to.data, from.data, from.size * sizeof(T));
        else
        {
            assert(to.size >= from.size && "size must be big enough");
            for(isize i = 0; i < from.size; i++)
                new(to.data + i) T(cast(T&&) from.data[i]);
        }
    }
    template<typename T> constexpr 
    void destruct_items(T* data, isize from, isize to, Type_Optims optims = DEF_TYPE_OPTIMS<T>) noexcept
    {
        if(is_flag_set(optims, Type_Optims::BYTE_DESTRUCT) == false)
            for(isize i = from; i < to; i++)
                data[i].~T();
    }

    template<typename T> constexpr 
    void destruct_items(Slice<T> items, Type_Optims optims = DEF_TYPE_OPTIMS<T>) noexcept
    {
        destruct_items<T>(items.data, 0, items.size, optims);
    }
    
    //fused destructor call with move call. Each element in destiation is first move constructed then destructs 
    // the source both of which happen during the same iteration 
    // (calling move_construct_items and destruct_items results in two iterations over the data)
    template<typename T> constexpr 
    void transfer_items(Slice<T> to, Slice<T> from, Type_Optims optims = DEF_TYPE_OPTIMS<T>) noexcept
    {
        if(is_flag_set(optims, Type_Optims::BYTE_TRANSFER))
            memmove(to.data, from.data, from.size * sizeof(T));
        else
        {
            assert(to.size >= from.size && "size must be big enough");
            for(isize i = 0; i < from.size; i++)
            {
                new(to.data + i) T(cast(T&&) from.data[i]);
                from.data[i].~T();
            }
        }
    }
}

#undef nodisc
#undef cast