#pragma once

#include <cstddef>
#include <cstdint>

namespace jot 
{
    using u8 = uint8_t;
    using u16 = uint16_t;
    using u32 = uint32_t;
    using u64 = uint64_t;

    using i8 = int8_t;
    using i16 = int16_t;
    using i32 = int32_t;
    using i64 = int64_t;

    using b8 = bool;
    using b16 = uint16_t;
    using b32 = uint32_t;
    using b64 = uint64_t;

    using f32 = float;
    using f64 = double;

    using byte = u8;
    using cstring = const char*;

    #ifdef DEFAULT_SIZE_TYPE
    using tsize = DEFAULT_SIZE_TYPE;
    #else
    using tsize = i64;
    #endif // DEFAULT_SIZE_TYPE

    using isize = i64;
    using usize = u64;
}