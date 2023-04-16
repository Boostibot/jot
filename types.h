#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
namespace jot 
{
#else
#include <stdbool.h>
#endif
    typedef uint8_t u8;
    typedef uint16_t u16;
    typedef uint32_t u32;
    typedef uint64_t u64;

    typedef int8_t i8;
    typedef int16_t i16;
    typedef int32_t i32;
    typedef int64_t i64;

    typedef bool b8;
    typedef uint16_t b16;
    typedef uint32_t b32;
    typedef uint64_t b64;

    typedef float f32;
    typedef double f64;

    typedef u8 byte;
    typedef const char* cstring;

    //#ifndef __cpp_char8_t
    //typedef char char8_t;
    //#endif
    
#ifdef __cplusplus
}
#endif

typedef ptrdiff_t isize;
typedef size_t usize;
