#pragma once

#include <cstdint>
#include <cassert>

#ifndef MAX_INTEGER_FIELD_TYPE
    #define MAX_INTEGER_FIELD_TYPE uint64_t
#endif

namespace jot 
{
    #ifndef JOT_SIZE_T
        using isize = ptrdiff_t;
    #endif

    #define cast(...) (__VA_ARGS__)
    #define nodisc [[nodiscard]]

    using Max_Field = MAX_INTEGER_FIELD_TYPE;

    constexpr static isize BYTE_BITS = CHAR_BIT;

    template <typename T>
    constexpr static isize BIT_COUNT = sizeof(T) * BYTE_BITS;

    template <typename T>
    constexpr static isize HALF_BIT_COUNT = sizeof(T) * BYTE_BITS / 2;

    template <typename T = Max_Field> constexpr nodisc
    T dirty_bit(isize bit_offset, T value = 1) noexcept 
    {
        assert(value == 0 || value == 1);
        return cast(T)(value) << bit_offset;
    }

    template <typename T = Max_Field, typename Val = Max_Field> constexpr nodisc
    T bit(isize bit_offset, Val value = 1) noexcept 
    {
        return dirty_bit<T>(bit_offset, cast(T)(!!value));
    }

    template <typename T> constexpr nodisc
    bool has_bit(T integer, isize bit_pos) noexcept 
    {
        return cast(bool)(integer & bit<Max_Field>(bit_pos));
    }

    template <typename T> constexpr nodisc
    Max_Field get_bit(T integer, isize bit_pos) noexcept 
    {
        return cast(Max_Field) has_bit<T>(integer, bit_pos);
    }

    template <typename T> constexpr nodisc
    T set_bit(T integer, isize bit_offset, isize value) noexcept 
    {
        return cast(T)((integer | bit(bit_offset)) ^ bit(bit_offset, !value));
    }

    template <typename T> constexpr nodisc
    T high_mask(size_t index = HALF_BIT_COUNT<T>) noexcept 
    {
        assert(index < BIT_SIZE<T>);
        return cast(T) (cast(T)(-1) << index);
    }

    template <typename T> constexpr nodisc
    T low_mask(size_t index = HALF_BIT_COUNT<T>) noexcept 
    {
        return cast(T) ~high_mask<T>(index);
    }

    template <typename T> constexpr nodisc
    T range_mask(isize from_bit, isize to_bit) noexcept 
    {
        return cast(T) (high_mask<T>(from_bit) & low_mask<T>(to_bit));
    }

    template <typename T> constexpr nodisc
    T high_bits(T value, size_t index = HALF_BIT_COUNT<T>) noexcept 
    {
        assert(index < BIT_SIZE<T>);
        return cast(T) (value >> index);
    }

    template <typename T> constexpr nodisc
    T low_bits(T value, size_t index = HALF_BIT_COUNT<T>) noexcept 
    {
        auto mask = low_mask<T>(index);
        return cast(T) (value & mask);
    };  

    template <typename T> constexpr nodisc
    T range_bits(T value, isize from_bit, isize to_bit) noexcept 
    {
        return cast(T) (value & range_mask<T>(from_bit, to_bit));
    }

    template <typename T> constexpr nodisc
    T combine_bits(T low, T high, size_t index = HALF_BIT_COUNT<T>) noexcept 
    {
        assert(index < BIT_SIZE<T>);
        return low_bits(low, index) | (high << index);
    };

    template <typename T> constexpr nodisc
    T dirty_combine_bits(T low, T high, size_t index = HALF_BIT_COUNT<T>) noexcept 
    {
        assert(index < BIT_SIZE<T>);
        assert(high_bits(low, index) == 0 && "low must not have high bits use combine_bits instead");
        return low | (high << index);
    };
}

#undef nodisc
#undef cast

#include "undefs.h"