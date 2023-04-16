#pragma once

#include "string.h"
#include "hash.h"
#include "hash_table.h"

namespace jot
{
    template <typename T> 
    uint64_t int_hash(T const& val, uint64_t seed) noexcept
    {
        return uint64_hash((u64) val ^ (seed*8251656)); 
    }

    template <typename T>  
    uint64_t int_slice_hash(Slice<T> const& val, uint64_t seed) noexcept
    {
        return (uint64_t) murmur_hash64(val.data, val.size * (isize) sizeof(T), seed);
    }

    template <typename T>  
    uint64_t int_array_hash(Array<T> const& val, uint64_t seed) noexcept
    {
        return int_slice_hash<const T>(slice(val), seed);
    }

    template <typename T>
    bool slice_key_equals(Slice<T> const& a, Slice<T> const& b) noexcept
    {
        return are_items_equal(a, b);
    }

    template <typename T>
    bool array_key_equals(Array<T> const& a, Array<T> const& b) noexcept
    {
        return are_items_equal(slice(a), slice(b));
    }

    template<typename T>
    using String_Hash = Hash_Table<String_Builder, T, int_array_hash<char>, array_key_equals<char>>;
}