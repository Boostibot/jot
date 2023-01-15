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
    #define func constexpr [[nodiscard]] auto
    #define proc constexpr [[nodiscard]] auto

    using Max_Field = MAX_INTEGER_FIELD_TYPE;

    constexpr static isize BYTE_BITS = CHAR_BIT;

    template <typename T>
    constexpr static isize BIT_COUNT = sizeof(T) * BYTE_BITS;

    template <typename T>
    constexpr static isize HALF_BIT_COUNT = sizeof(T) * BYTE_BITS / 2;

    #define templ_func template <typename T> func

    template <typename T = Max_Field>
    func dirty_bit(isize bit_offset, T value = 1) noexcept -> T {
        assert(value == 0 || value == 1);
        return cast(T)(value) << bit_offset;
    }

    template <typename T = Max_Field, typename Val = Max_Field>
    func bit(isize bit_offset, Val value = 1) noexcept -> T {
        return dirty_bit<T>(bit_offset, cast(T)(!!value));
    }

    templ_func has_bit(T integer, isize bit_pos) noexcept -> bool {
        return cast(bool)(integer & bit<Max_Field>(bit_pos));
    }

    templ_func get_bit(T integer, isize bit_pos) noexcept -> Max_Field {
        return cast(Max_Field) has_bit<T>(integer, bit_pos);
    }

    templ_func set_bit(T integer, isize bit_offset, isize value) noexcept -> T {
        return cast(T)((integer | bit(bit_offset)) ^ bit(bit_offset, !value));
    }

    templ_func high_mask(size_t index = HALF_BIT_COUNT<T>) noexcept -> T {
        assert(index < BIT_SIZE<T>);
        return cast(T) (cast(T)(-1) << index);
    }

    templ_func low_mask(size_t index = HALF_BIT_COUNT<T>) noexcept -> T {
        return cast(T) ~high_mask<T>(index);
    }

    templ_func range_mask(isize from_bit, isize to_bit) noexcept -> T {
        return cast(T) (high_mask<T>(from_bit) & low_mask<T>(to_bit));
    }

    templ_func high_bits(T value, size_t index = HALF_BIT_COUNT<T>) noexcept -> T {
        assert(index < BIT_SIZE<T>);
        return cast(T) (value >> index);
    }

    templ_func low_bits(T value, size_t index = HALF_BIT_COUNT<T>) noexcept -> T {
        auto mask = low_mask<T>(index);
        return cast(T) (value & mask);
    };  

    templ_func range_bits(T value, isize from_bit, isize to_bit) noexcept -> T {
        return cast(T) (value & range_mask<T>(from_bit, to_bit));
    }

    templ_func combine_bits(T low, T high, size_t index = HALF_BIT_COUNT<T>) noexcept -> T {
        assert(index < BIT_SIZE<T>);
        return low_bits(low, index) | (high << index);
    };

    templ_func dirty_combine_bits(T low, T high, size_t index = HALF_BIT_COUNT<T>) noexcept -> T {
        assert(index < BIT_SIZE<T>);
        assert(high_bits(low, index) == 0 && "low must not have high bits use combine_bits instead");
        return low | (high << index);
    };
}

#undef func
#undef proc
#undef templ_func
#undef cast

#include "undefs.h"