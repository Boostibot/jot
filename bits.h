#pragma once

#include "utils.h"

#include <bit>
#include <cstring>
#include <climits>

#ifdef _MSC_VER

#include <cstdlib>
#include <intrin.h>
#include <stdlib.h>
#define bswap_32(x) _byteswap_ulong(x)
#define bswap_64(x) _byteswap_uint64(x)

#elif defined(__APPLE__) && defined(__MACH__)

// Mac OS X / Darwin features
#include <libkern/OSByteOrder.h>
#define bswap_32(x) OSSwapInt32(x)
#define bswap_64(x) OSSwapInt64(x)

#elif defined(__sun) || defined(sun)

#include <sys/byteorder.h>
#define bswap_32(x) BSWAP_32(x)
#define bswap_64(x) BSWAP_64(x)

#elif defined(__FreeBSD__)

#include <sys/endian.h>
#define bswap_32(x) bswap32(x)
#define bswap_64(x) bswap64(x)

#elif defined(__OpenBSD__)

#include <sys/types.h>
#define bswap_32(x) swap32(x)
#define bswap_64(x) swap64(x)

#elif defined(__NetBSD__)

#include <sys/types.h>
#include <machine/bswap.h>

#if defined(__BSWAP_RENAME) && !defined(__bswap_32)
#define bswap_32(x) bswap32(x)
#define bswap_64(x) bswap64(x)
#endif

#elif defined(unix) || defined(__unix) || defined(__unix__) || defined(__linux__)

#include <byteswap.h>

#else

#define BITS_NO_INTRIN
#define bswap_32(x)
#define bswap_64(x)

#endif

#include "defines.h"

namespace jot 
{
    using Max_Field = u64;

    template<size_t size_>
    using Byte_Array = Array_<byte, size_>;

    template<typename T>
    using Bytes = Byte_Array<sizeof(T)>;

    constexpr static size_t BYTE_BITS = CHAR_BIT;

    template <typename T>
    constexpr static size_t BIT_COUNT = sizeof(T) * BYTE_BITS;

    #define bitsof(...) (sizeof(__VA_ARGS__) * CHAR_BIT)

    template <typename T>
    func bitcast(let& val) -> T
    {
        return std::bit_cast<T>(val);
    }

    template<typename From>
    func to_bytes(From val) -> Bytes<From>
    {
        using Rep = Bytes<From>;
        assert(sizeof(Rep) >= sizeof(From));
        return bitcast<Rep>(val);
    }

    template<typename To>
    func from_bytes(const Bytes<To>& bytes) -> To
    {
        return bitcast<To>(bytes);
    }

    template <typename T>
    func get_byte(T const& value, size_t index) -> byte
    {
        mut rep = to_bytes(value);

        assert(index < rep.size && "index needs to be in bounds");
        return rep[index];
    }

    template <typename T>
    func set_byte(T const& value, size_t index, byte to_val) -> T
    {
        mut rep = to_bytes(value);

        assert(index < rep.size && "index needs to be in bounds");
        rep[index] = to_val;
        return bitcast<T>(rep);
    }

    
    template <typename T>
    func get_byte(const T array[], size_t index) -> byte
    {
        return get_byte(array[index / sizeof(T)], index % sizeof(T));
    }
    template <typename T>
    func set_byte(const T array[], size_t index, byte to_val) -> T
    {
        return set_byte(array[index / sizeof(T)], index % sizeof(T), to_val);
    }
    
    template <typename My_Max = Max_Field>
    func dirty_bit(size_t bit_offset, My_Max value = 1) -> My_Max
    {
        assert(value == 0 || value == 1);
        return cast(My_Max)(value) << bit_offset;
    }

    template <typename My_Max = Max_Field, typename Val = u64>
    func bit(size_t bit_offset, Val value = 1) -> My_Max
    {
        return dirty_bit<My_Max>(bit_offset, cast(My_Max)(!!value));
    }

    template <typename T>
    func has_bit(T integer, size_t bit_pos) -> bool
    {
        #if defined(_MSC_VER) && !defined(BITS_NO_INTRIN)
            if(!std::is_constant_evaluated())
            {
                if constexpr (sizeof(integer) <= sizeof(long))
                {
                    long copy = cast(long) integer;
                    return cast(bool) _bittest(&copy, cast(long) bit_pos);
                }
                else if (sizeof(integer) <= sizeof(u64))
                {
                    const long long copy = cast(long long) integer;
                    return cast(bool) _bittest64(&copy, cast(long long) bit_pos);
                }
            }
        #endif
        return cast(bool)(integer & bit<Max_Field>(bit_pos));
    }

    template <typename T>
    func get_bit(T integer, size_t bit_pos) -> Max_Field
    {
        return cast(Max_Field) has_bit<T>(integer, bit_pos);
    }

    template <typename T>
    func set_bit(T integer, size_t bit_offset, size_t value) -> T
    {
        return cast(T)((integer | bit(bit_offset)) ^ bit(bit_offset, !value));
    }
    
    template <typename T>
    func toggle_bit(T integer, size_t bit_offset) -> T
    {
        return cast(T)(integer ^ bit(bit_offset));
    }
    
    template <typename Field>
    func bitmask_higher(size_t from_bit) -> Field
    {
        constexpr Max_Field zero = 0;
        return cast(Field) (~zero << from_bit);
    }

    template <typename Field>
    func bitmask_lower(size_t to_bit) -> Field
    {
        constexpr Max_Field zero = 0;
        return cast(Field) ~(~zero << to_bit);
    }

    template <typename Field>
    func bitmask_range(size_t from_bit, size_t to_bit) -> Field
    {
        return bitmask_higher<Field>(from_bit) & bitmask_lower<Field>(to_bit);
    }

    template <typename Field>
    func bitmask(size_t from_bit, size_t num_bits) -> Field
    {
        return bitmask_range<Field>(from_bit, from_bit + num_bits);
    }

    namespace detail 
    {
        template<class T1, class T2>
        func get_iter_info(size_t i, size_t to_offset, size_t from_offset) -> Array_<size_t, 4>
        {
            let ti = i + to_offset;
            let fi = i + from_offset;

            let to_index = ti / sizeof(T1);
            let from_index = fi / sizeof(T2);

            let to_byte_index = ti % sizeof(T1);
            let from_byte_index = fi % sizeof(T2);

            return Array_{to_index, from_index, to_byte_index, from_byte_index};
        }

        template <typename T1, typename T2, typename Size>
        proc copy_bytes_forward(T1 to[], size_t to_offset, const T2 from[], size_t from_offset, Size size)
        {
            //Little dumb algorithm that goes byte by byte for each copying the whole 
            // item at index
            for(Size i = 0; i < size; i++)
            {
                let info = get_iter_info<T1, T2>(cast(size_t) i, cast(size_t) to_offset, cast(size_t) from_offset);
                let [to_index, from_index, to_byte_index, from_byte_index] = info.data;

                //even thought we copy bytes we need to be able to access the whole element as we 
                // are using normal assignment -> this can be very unsafe but since this a consteval
                // proc at worst we will get a compiler error
                let single = get_byte(from[from_index], from_byte_index);
                to[to_index] = set_byte(to[to_index], to_byte_index, single);
            }
        }

        template <typename T1, typename T2, typename Size>
        proc copy_bytes_backward(T1 to[], size_t to_offset, const T2 from[], size_t from_offset, Size size)
        {
            for(Size i = size; i-- > 0; )
            {
                let info = get_iter_info<T1, T2>(cast(size_t) i, cast(size_t) to_offset, cast(size_t) from_offset);
                let [to_index, from_index, to_byte_index, from_byte_index] = info.data;

                let single = get_byte(from[from_index], from_byte_index);
                to[to_index] = set_byte(to[to_index], to_byte_index, single);
            }
        }

        template <typename T1, typename Size>
        proc set_bytes(T1 to[], size_t to_offset, byte val, Size size)
        {
            for(Size i = 0; i < size; i++)
            {
                let info = get_iter_info<T1, T1>(cast(size_t) i, cast(size_t) to_offset, cast(size_t) to_offset);
                let [to_index, from_index, to_byte_index, from_byte_index] = info.data;

                to[to_index] = set_byte(to[to_index], to_byte_index, val);
            }
        }

        template <typename T1, typename T2, typename Size>
        func compare_bytes(const T1 to[], size_t to_offset, const T2 from[], size_t from_offset, Size size)
        {
            for(Size i = 0; i < size; i++)
            {
                let info = get_iter_info<T1, T2>(cast(size_t) i, cast(size_t) to_offset, cast(size_t) from_offset);
                let [to_index, from_index, to_byte_index, from_byte_index] = info.data;

                let byte1 = get_byte(to[to_index], to_byte_index);
                let byte2 = get_byte(from[from_index], from_byte_index);

                if(byte1 != byte2)
                    return cast(int)(byte1) - cast(int)(byte2);
            }

            return (int)0;
        }

        template <typename T>
        proc move_bytes(T to[], size_t to_index, size_t to_offset, size_t from_index, size_t from_offset, size_t size)
        {
            let from_byte_index = from_index * sizeof(T) + from_offset;
            let to_byte_index = to_index * sizeof(T) + to_offset;

            if(from_byte_index == to_byte_index)
                return;

            if(from_byte_index < to_byte_index)
                return copy_bytes_backward(to + to_index, to_offset, to + from_index, from_offset, size);
            else
                return copy_bytes_forward(to + to_index, to_offset, to + from_index, from_offset, size);
        }

        void memcpy(void* to, size_t to_offset, const void* from, size_t from_offset, size_t size)
        {
            std::memcpy(cast(byte*)(to) + to_offset, cast(const byte*)from + from_offset, cast(size_t) size);
        }

        void memset(void* to, size_t to_offset, byte val, size_t size)
        {
            std::memset(cast(byte*)(to) + to_offset, cast(int)val, cast(size_t) size);
        }

        void memmove(void* to, size_t to_offset, const void* from, size_t from_offset, size_t size)
        {
            std::memmove(cast(byte*)(to) + to_offset, cast(const byte*)from + from_offset, cast(size_t) size);
        }

        auto memcmp(const void* to, size_t to_offset, const void* from, size_t from_offset, size_t size)
        {
            return std::memcmp(cast(const byte*)(to) + to_offset, cast(const byte*)from + from_offset, cast(size_t) size);
        }
    }

    //In many cases copy_n doesnt produce optimal assembly (extra checks)
    // this performs the exact same work during compile time and 
    // during run time becomes a single memcpy
    template <typename T1, typename T2>
    proc copy_bytes(T1* to, size_t to_offset, const T2* from, size_t from_offset, size_t size) -> void
    {
        //needs to be constexpr if becasue copy bytes cannot take void ptr
        // (so yes this cant be condensed into a single if and else)
        if constexpr (std::is_void_v<T1> || std::is_void_v<T2>)
        {
            detail::memcpy(to, to_offset, from, from_offset, size);
        }
        else
        {
            if (std::is_constant_evaluated())
                detail::copy_bytes_forward(to, to_offset, from, from_offset, size);
            else
                detail::memcpy(to, to_offset, from, from_offset, size);
        }
    }

    //There is no way to make memmove with its original signature into a constexpr proc without an additional swap buffer
    template <typename T>
    proc move_bytes(T* array, size_t to_index, size_t to_offset, size_t from_index, size_t from_offset, size_t size) -> void
    {
        if constexpr (std::is_void_v<T>)
            detail::memmove(array + to_index, to_offset, array + from_index, from_offset, size);
        else
        {
            if (std::is_constant_evaluated())
                detail::move_bytes(array + to_index, to_offset, array + from_index, from_offset, size);
            else
                detail::memmove(array + to_index, to_offset, array + from_index, from_offset, size);
        }
    }

    template <typename T1>
    proc set_bytes(T1* to, size_t to_offset, byte val, size_t size) -> void
    {
        if constexpr (std::is_void_v<T1>)
            detail::memset(to, to_offset, val, size);
        else
        {
            if (std::is_constant_evaluated())
                detail::set_bytes(to, to_offset, val, size);
            else
                detail::memset(to, to_offset, val, size);
        }
    }

    template <typename T1, typename T2>
    func compare_bytes(const T1* to, size_t to_offset, const T2* from, size_t from_offset, size_t size) -> int
    {
        if constexpr (std::is_void_v<T1> || std::is_void_v<T2>)
            return detail::memcmp(to, to_offset, from, from_offset, size);
        else
        {
            if (std::is_constant_evaluated())
                return detail::compare_bytes(to, to_offset, from, from_offset, size);
            else
                return detail::memcmp(to, to_offset, from, from_offset, size);
        }
    }

    template <typename T1, typename T2>
    func are_bytes_equal(const T1* to, size_t to_offset, const T2* from, size_t from_offset, size_t size) -> bool
    {
        return compare_bytes(to, to_offset, from, from_offset, size) == 0;
    }

    template <typename T1, typename T2>
    proc copy_bytes(T1* to, const T2* from, size_t size) -> void
    {
        return copy_bytes(to, 0, from, 0, size);
    }
    template <typename T>
    proc move_bytes(T* array, size_t to_index, size_t from_index, size_t size) -> void
    {
        return move_bytes(array, to_index, 0, from_index, 0, size);
    }

    template <typename T>
    proc set_bytes(T* to, byte val, size_t size) -> void
    {
        return set_bytes(to, 0, val, size);
    }

    template <typename T1, typename T2>
    func compare_bytes(const T1* to, const T2* from, size_t size) -> int
    {
        return compare_bytes(to, 0, from, 0, size);
    }

    template <typename T1, typename T2>
    func are_bytes_equal(const T1* to, const T2* from, size_t size) -> bool
    {
        return compare_bytes(to, from, size) == 0;
    }

    proc byteswap(byte output[], byte input[], size_t size) -> void
    {
        for (size_t i = 0; i < size; i++)
            output[i] = input[size - i - 1];
    }

    proc byteswap(byte bytes[], size_t size) -> void
    {
        let half_size = size / 2;
        for (size_t i = 0; i < half_size; i++)
            std::swap(bytes[i], bytes[size - i - 1]);
    }

    namespace detail
    {
        template <typename T>
        func manual_byteswap(T& value) -> T
        {
            //Uses of bitcast to enable consteval of this fn
            mut rep = to_bytes(value);
            byteswap(rep, rep.size);
            return bitcast<T>(rep);
        }
    }

    //wrapper for byteswap proc in c++23
    // will in most cases get compiled into bswap instruction
    template <typename T>
    func byteswap(const T& value) -> T
    {
        #ifdef __cpp_lib_byteswap 
            return std::byteswap(value);
        #else
            #ifndef BITS_NO_INTRIN
            if(std::is_unsigned_v<T> && !std::is_constant_evaluated())
            {
                if constexpr (sizeof(value) == sizeof(u64))
                    return cast(T) bswap_64(cast(unsigned long long) value);
                if constexpr (sizeof(value) == sizeof(u32))
                    return cast(T) bswap_32(cast(u32) value);
            }
            #endif // !BITS_NO_INTRIN

        return detail::manual_byteswap(value);
        #endif
    }
}

#undef bswap_32
#undef bswap_64

#include "undefs.h"