#pragma once

#include <algorithm>
#include <compare>
#include <cstdio>
#include <array>

#include "../lib/jot/utils.h"
#include "../lib/jot/defines.h"

#define CONCAT(a, b) IN__CONCAT_(a, b)
#define IN__CONCAT_(a, b) a ## b

namespace jot::tests 
{
    constexpr bool LOG_SUCESS = false;

    #define test proc
    #define runtime_assert(...) \
        if(!(__VA_ARGS__)) { \
            if(!std::is_constant_evaluated()) \
                printf("FAILED: %s() : %d : " #__VA_ARGS__ "\n", __func__, __LINE__);  \
            return; \
        }  \
        else if(LOG_SUCESS && std::is_constant_evaluated() == false) { \
            printf("SUCCESS: %s() : " #__VA_ARGS__ "\n", __func__); \
        }\

    //#define hybrid_assert(...) runtime_assert(__VA_ARGS__)
    #define hybrid_assert(...) static_assert(__VA_ARGS__); runtime_assert(__VA_ARGS__)
    #define run_test(...) static bool CONCAT(CONCAT(test_result_, __LINE__), __VA_ARGS__) = run_test_(__VA_ARGS__, #__VA_ARGS__);

    auto run_test_(mut&& test_fn, const char* name) 
    {
        printf("RUNNING TEST: %s()\n", name);
        test_fn(); return true;
    }

    //The i know what i am doing assignment operator
    template <typename To, typename From>
    proc& operator <<=(To& assign_to, const From& assign_from)
    {
        return (assign_to = cast(To) assign_from);
    }

    proc equal_n(mut&& first, mut&& second, let count)
    {
        return std::equal(first, first + count, second);
    }

    test float_equal(let f1, let f2, f64 epsilon = 0.0001)
    {
        mut diff = f2 - f1;
        if(diff < 0)
            diff *= -1;

        return diff < epsilon;
    }

    struct TestStruct1 
    {
        i32 i1 = -51631;
        i32 i2 = 63464;
        u8 u1 = 144;
        f64 f1 = -5151.54684646;
        u8 u2 = 64;

        proc operator<=>(const TestStruct1&) const = default;
    };

    struct TestStruct2 
    {
        u8 uarr[3] = {1, 2, 123};
        i32 iarr[4] = {-51631, 63464, 0, 9849636};
        u8 u1 = 144;
        f64 farr[5] = {-5151.54684646, 999999, -999999, 0.00003, -0.00003};
        u8 u2 = 64;

        proc operator<=>(const TestStruct2&) const = default;
    };

}

#include "../lib/jot/undefs.h"