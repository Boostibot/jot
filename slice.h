#pragma once 

#include <cstring>
#include <cassert>
#include <cstdint>

//I dont know how to get rid of the dependency on these two:
#include <type_traits>
#include <iterator>

#define cast(...) (__VA_ARGS__)
#define nodisc [[nodiscard]]

namespace jot
{
    #ifndef JOT_SIZE_T
        using isize = ptrdiff_t;
    #endif

    nodisc constexpr 
    isize strlen(const char* str)
    {
        isize size = 0;
        while(str[size] != '\0')
        {
            size++;
        }
        return size;
    };

    //Open POD struct representing a contiguous array 
    // in memory. Does not own the data and is literally just a pointer.
    // Is analoguous to std::span except much simpler. Is also used for strings (see string.h)
    template<typename T>
    struct Slice
    {
        T* data = nullptr;
        isize size = 0;

        constexpr Slice() = default;
        constexpr Slice(T* data, isize size) 
            : data(data), size(size) {}

        template<typename T, std::enable_if_t<std::is_same_v<T, const char>, bool> = true>
        constexpr Slice(T* str)
            : data(str), size(strlen(str)) {}

        template<typename T>
        constexpr operator Slice<const T>() const noexcept 
        { 
            return Slice<const T>{this->data, this->size}; 
        }
        
        constexpr bool operator ==(Slice const& other) const noexcept 
        {
            return other.data == this->data && other.size == this->size;
        }
        constexpr bool operator !=(Slice const& other) const noexcept
        {
            return !(other == *this);
        }

        #define DATA data
        #define SIZE size
        #include "slice_op_text.h"
        #undef DATA
        #undef SIZE
    };


    Slice(const char*) -> Slice<const char>;

    nodisc constexpr 
    Slice<const char> slice(const char* str) 
    {
        return {str, strlen(str)};
    }

    template<typename Cont> nodisc constexpr 
    auto slice(Cont const& sliced) 
    {
        using T_ref = decltype(*sliced.data);
        using T = std::remove_reference_t<T_ref>;
        return Slice<T>{sliced.data, sliced.size};
    }

    template<typename Cont> nodisc constexpr 
    auto slice(Cont* sliced) 
    {
        using T_ref = decltype(*sliced->data);
        using T = std::remove_reference_t<T_ref>;
        return Slice<T>{sliced->data, sliced->size};
    }
    
    constexpr bool is_const_eval(bool if_not_present = false) noexcept {
        #ifdef __cpp_lib_is_constant_evaluated
            return std::is_constant_evaluated();
        #else
            return if_not_present;
        #endif
    }

    #define constexpr_assert(a) (is_const_eval(true) ? (void)0 : assert(a))

    template<typename T> nodisc constexpr  
    bool is_invarinat(Slice<T> slice) {
        return slice.size >= 0;
    }

    template<typename T> nodisc constexpr 
    Slice<T> slice(Slice<T> slice, isize from) {
        constexpr_assert((0 <= from && from <= slice.size) && "index out of bounds");
        return Slice<T>{slice.data + from, slice.size - from};
    }

    template<typename T> nodisc constexpr 
    Slice<T> trim(Slice<T> slice, isize to_index) {   
        constexpr_assert((0 <= to_index && to_index <= slice.size) && "index out of bounds");
        return Slice<T>{slice.data, to_index};
    }

    #undef constexpr_assert

    template<typename T> nodisc constexpr 
    Slice<T> slice_size(Slice<T> base_slice, isize from, isize size) {
        return trim(slice(base_slice, from), size);
    }

    template<typename T> nodisc constexpr 
    Slice<T> slice_range(Slice<T> base_slice, isize from, isize to) {
        return slice(trim(base_slice, to), from);
    }

    template<typename T> nodisc constexpr 
    isize byte_size(Slice<T> slice) {
        return slice.size * cast(isize) sizeof(T);
    }

    template<typename To, typename From = int> nodisc constexpr
    Slice<To> cast_slice(Slice<From> slice)
    {
        if constexpr (std::is_convertible_v<From*, To*>)
            return {cast(To*) slice.data, slice.size};
        else
            return {cast(To*) cast(void*) slice.data, (byte_size(slice) / cast(isize) sizeof(To))};
    }

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

    template<typename T> constexpr 
    void fill(Slice<T>* to , T const& with) noexcept
    {
        for(isize i = 0; i < to->size; i++)
            to->data[i] = with;
    }

    template<typename T> constexpr 
    void null_items(Slice<T>* to) noexcept
    {
        if(is_const_eval() == false && is_byte_nullable<T>)
        {
            memset(to->data, 0, to->size * sizeof(T));
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
    void copy_items(Slice<T>* to, Slice<const T> from) noexcept
    {
        //we by default use memmove since its safer
        // (memcpy is still availible though under longer, uglier name)
        assert(to->size >= from.size && "size must be big enough");
        if(is_const_eval() == false && is_byte_copyable<T>)
        {
            memmove(to->data, from.data, from.size * sizeof(T));
            return;
        }

        if(to->data < from.data)
        {
            for(isize i = 0; i < from.size; i++)
                to->data[i] = from.data[i];
        }
        else
        {
            for(isize i = from.size; i-- > 0;)
                to->data[i] = from.data[i];
        }
    }
    
    template<typename T> constexpr 
    void copy_items_no_alias(Slice<T>* to, Slice<const T> from) noexcept
    {
        assert(are_aliasing(*to, from) == false && "must not alias");
        assert(to->size >= from.size && "size must be big enough");
        
        if(is_const_eval() == false && is_byte_copyable<T>)
        {
            memcpy(to->data, from.data, from.size * sizeof(T));
        }
        else
        {
            for(isize i = 0; i < from.size; i++)
                to->data[i] = from.data[i];
        }
    }
    
    template<typename T> constexpr 
    void move_items(Slice<T>* to, Slice<T>* from) noexcept
    {
        assert(to->size >= from->size && "size must be big enough");
        if(is_const_eval() == false && is_byte_copyable<T>)
        {
            memmove(to->data, from->data, from->size * sizeof(T));
            return;
        }

        if(to->data < from->data)
        {
            for(isize i = 0; i < from->size; i++)
                to->data[i] = cast(T&&) from->data[i];
        }
        else
        {
            for(isize i = from->size; i-- > 0;)
                to->data[i] = cast(T&&) from->data[i];
        }
    }
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
