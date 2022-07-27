#pragma once
#include "tester_utils.h"
#include "../lib/jot/defines.h"

namespace jot::tests 
{

    template <typename Integer, typename From>
    concept convertible_integer = 
        (std::is_unsigned_v<Integer> && std::is_unsigned_v<From> && (sizeof(Integer) >= sizeof(From))) ||
        (std::is_signed_v<Integer>   && std::is_signed_v<From> && (sizeof(Integer) >= sizeof(From))) ||
        (std::is_signed_v<Integer>   && std::is_unsigned_v<From> && (sizeof(Integer) > sizeof(From)));

    test test_concepts()
    {
        //convertible_integer shoudl return true only if the types are convertible without explicit cast
        // as if in the form of Left = Right; (with convertible_integer<Left, Right>)
        static_assert(convertible_integer<u64, u32>);
        static_assert(convertible_integer<u32, u32>);
        static_assert(convertible_integer<u32, u16>);
        static_assert(convertible_integer<u32, u8>);
        static_assert(convertible_integer<u8, u8>);

        static_assert(convertible_integer<u32, u64> == false);
        static_assert(convertible_integer<u16, u32> == false);
        static_assert(convertible_integer<u8, u32> == false);

        static_assert(convertible_integer<i64, i32>);
        static_assert(convertible_integer<i32, i32>);
        static_assert(convertible_integer<i32, i16>);
        static_assert(convertible_integer<i32, i8>);
        static_assert(convertible_integer<i8, i8>);

        static_assert(convertible_integer<i32, i64> == false);
        static_assert(convertible_integer<i16, i32> == false);
        static_assert(convertible_integer<i8, i32> == false);

        static_assert(convertible_integer<u32, i32> == false);
        static_assert(convertible_integer<u64, i32> == false);
        static_assert(convertible_integer<u64, i64> == false);
        static_assert(convertible_integer<u16, i16> == false);
        static_assert(convertible_integer<u16, i8> == false);

        static_assert(convertible_integer<i32, u32> == false);
        static_assert(convertible_integer<i32, u64> == false);
        static_assert(convertible_integer<i64, u64> == false);
        static_assert(convertible_integer<i16, u16> == false);
        static_assert(convertible_integer<i8, u16> == false);
        static_assert(convertible_integer<i8, u8> == false);

        static_assert(convertible_integer<i64, u32>);
        static_assert(convertible_integer<i64, u16>);
        static_assert(convertible_integer<i64, u8>);
        static_assert(convertible_integer<i32, u16>);
        static_assert(convertible_integer<i32, u8>);
        static_assert(convertible_integer<i16, u8>);
    }
}

#include "../lib/jot/undefs.h"