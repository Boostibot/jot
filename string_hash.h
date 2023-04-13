#pragma once

#include "string.h"
#include "hash.h"
#include "hash_table.h"

namespace jot
{
    template <typename T> 
    hash_t scalar_key_hash(T const& val, hash_t seed) noexcept
    {
        return uint64_hash((u64) val) ^ seed;
    }

    template <typename T>  
    hash_t scalar_slice_hash(Slice<T> const& val, hash_t seed) noexcept
    {
        return (hash_t) murmur_hash64(val.data, val.size * (isize) sizeof(T), seed);
    }

    template <typename T>  
    hash_t scalar_stack_hash(Stack<T> const& val, hash_t seed) noexcept
    {
        return scalar_slice_hash<T>(slice(val), seed);
    }

    template <typename T>
    bool slice_key_equals(Slice<T> const& a, Slice<T> const& b) noexcept
    {
        return are_equal(a, b);
    }

    template <typename T>
    bool stack_key_equals(Stack<T> const& a, Stack<T> const& b) noexcept
    {
        return are_equal(slice(a), slice(b));
    }

    template<typename T>
    using String_Hash = Hash_Table<String_Builder, T, scalar_stack_hash<char>, stack_key_equals<char>>;
}