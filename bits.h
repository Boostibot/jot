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

#ifndef MAX_INTEGER_FIELD_TYPE
    #define MAX_INTEGER_FIELD_TYPE jot::u64
#endif

#include "defines.h"

namespace jot 
{

    #undef func
    #undef proc
    #define func constexpr nodisc auto
    #define proc constexpr nodisc auto

    using Max_Field = MAX_INTEGER_FIELD_TYPE;

    template<isize size_>
    using Byte_Array = Array<u8, size_>;

    template<typename T>
    using Bytes = Byte_Array<sizeof(T)>;

    constexpr static isize BYTE_BITS = CHAR_BIT;

    template <typename T>
    constexpr static isize BIT_COUNT = sizeof(T) * BYTE_BITS;

    template <typename T>
    constexpr static isize HALF_BIT_COUNT = sizeof(T) * BYTE_BITS / 2;

    using std::bit_cast;

    template<typename From>
    func to_bytes(From val) noexcept -> Bytes<From>
    {
        using Rep = Bytes<From>;
        assert(sizeof(Rep) >= sizeof(From));
        return bit_cast<Rep>(val);
    }

    template<typename To>
    func from_bytes(const Bytes<To>& bytes) noexcept -> To
    {
        return bit_cast<To>(bytes);
    }

    #define templ_func template <typename T> func
    

    templ_func get_byte(T in value, isize index) noexcept -> u8 {
        mut rep = to_bytes(value);

        assert(index < rep.size && "index needs to be in bounds");
        return rep[index];
    }

    templ_func set_byte(T in value, isize index, u8 to_val) noexcept -> T {
        mut rep = to_bytes(value);

        assert(index < rep.size && "index needs to be in bounds");
        rep[index] = to_val;
        return bit_cast<T>(rep);
    }

    template <typename T = Max_Field>
    func dirty_bit(isize bit_offset, T value = 1) noexcept -> T {
        assert(value == 0 || value == 1);
        return cast(T)(value) << bit_offset;
    }

    template <typename T = Max_Field, typename Val = u64>
    func bit(isize bit_offset, Val value = 1) noexcept -> T {
        return dirty_bit<T>(bit_offset, cast(T)(!!value));
    }

    templ_func has_bit(T integer, isize bit_pos) noexcept -> bool
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
        let mask = low_mask<T>(index);
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

    namespace detail 
    {
        struct Iter_Info
        {
            isize to_index;
            isize from_index;

            isize to_byte_index;
            isize from_byte_index;

            constexpr Iter_Info(isize i, isize to_offset, isize from_offset, isize size1, isize size2) noexcept 
            {
                const isize ti = i + to_offset;
                const isize fi = i + from_offset;

                to_index = ti / size1;
                from_index = fi / size2;

                to_byte_index = ti % size1;
                from_byte_index = fi % size2;
            }
        };

        template <typename T1, typename T2, typename Size>
        proc copy_bytes_forward(T1 to[], isize to_offset, const T2 from[], isize from_offset, Size size) noexcept
        {
            //Little dumb algorithm that goes u8 by u8 for each copying the whole 
            // item at index
            for(Size i = 0; i < size; i++)
            {
                Iter_Info info(i,  to_offset, from_offset, sizeof(T1), sizeof(T2));

                //even thought we copy bytes we need to be able to access the whole element as we 
                // are using normal assignment -> this can be very unsafe but since this a consteval
                // proc at worst we will get a compiler error
                let single = get_byte(from[info.from_index], info.from_byte_index);
                to[info.to_index] = set_byte(to[info.to_index], info.to_byte_index, single);
            }
        }

        template <typename T1, typename T2, typename Size>
        proc copy_bytes_backward(T1 to[], isize to_offset, const T2 from[], isize from_offset, Size size) noexcept
        {
            for(Size i = size; i-- > 0; )
            {
                Iter_Info info(i,  to_offset, from_offset, sizeof(T1), sizeof(T2));

                let single = get_byte(from[info.from_index], info.from_byte_index);
                to[info.to_index] = set_byte(to[info.to_index], info.to_byte_index, single);
            }
        }

        template <typename T1, typename Size>
        proc set_bytes(T1 to[], isize to_offset, u8 val, Size size) noexcept
        {
            for(Size i = 0; i < size; i++)
            {
                Iter_Info info(i, to_offset, 0, sizeof(T1), 1);
                to[info.to_index] = set_byte(to[info.to_index], info.to_byte_index, val);
            }
        }

        template <typename T1, typename T2, typename Size>
        func compare_bytes(const T1 to[], isize to_offset, const T2 from[], isize from_offset, Size size) noexcept
        {
            for(Size i = 0; i < size; i++)
            {
                Iter_Info info(i,  to_offset, from_offset, sizeof(T1), sizeof(T2));

                let byte1 = get_byte(to[info.to_index], info.to_byte_index);
                let byte2 = get_byte(from[info.from_index], info.from_byte_index);

                if(byte1 != byte2)
                    return cast(int)(byte1) - cast(int)(byte2);
            }

            return (int)0;
        }

        template <typename T>
        proc move_bytes(T to[], isize to_index, isize to_offset, isize from_index, isize from_offset, isize size) noexcept
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

        void memcpy(void* to, isize to_offset, const void* from, isize from_offset, isize size) noexcept
        {
            std::memcpy(cast(u8*)(to) + to_offset, cast(const u8*)from + from_offset, cast(isize) size);
        }

        void memset(void* to, isize to_offset, u8 val, isize size) noexcept
        {
            std::memset(cast(u8*)(to) + to_offset, cast(int)val, cast(isize) size);
        }

        void memmove(void* to, isize to_offset, const void* from, isize from_offset, isize size) noexcept
        {
            std::memmove(cast(u8*)(to) + to_offset, cast(const u8*)from + from_offset, cast(isize) size);
        }

        auto memcmp(const void* to, isize to_offset, const void* from, isize from_offset, isize size) noexcept
        {
            return std::memcmp(cast(const u8*)(to) + to_offset, cast(const u8*)from + from_offset, cast(isize) size);
        }
    }

    //In many cases copy_n doesnt produce optimal assembly (extra checks)
    // this performs the exact same work during compile time and 
    // during run time becomes a single memcpy
    template <typename T1, typename T2>
    proc copy_bytes(T1* to, isize to_offset, const T2* from, isize from_offset, isize size) noexcept -> void
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
    proc move_bytes(T* array, isize to_index, isize to_offset, isize from_index, isize from_offset, isize size) noexcept -> void
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
    proc set_bytes(T1* to, isize to_offset, u8 val, isize size) noexcept -> void
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
    func compare_bytes(const T1* to, isize to_offset, const T2* from, isize from_offset, isize size) noexcept -> int
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
    func are_bytes_equal(const T1* to, isize to_offset, const T2* from, isize from_offset, isize size) noexcept -> bool
    {
        return compare_bytes(to, to_offset, from, from_offset, size) == 0;
    }

    template <typename T1, typename T2>
    proc copy_bytes(T1* to, const T2* from, isize size) noexcept -> void
    {
        return copy_bytes(to, 0, from, 0, size);
    }
    template <typename T>
    proc move_bytes(T* array, isize to_index, isize from_index, isize size) noexcept -> void
    {
        return move_bytes(array, to_index, 0, from_index, 0, size);
    }

    template <typename T>
    proc set_bytes(T* to, u8 val, isize size) noexcept -> void
    {
        return set_bytes(to, 0, val, size);
    }

    template <typename T1, typename T2>
    func compare_bytes(const T1* to, const T2* from, isize size) noexcept -> int
    {
        return compare_bytes(to, 0, from, 0, size);
    }

    template <typename T1, typename T2>
    func are_bytes_equal(const T1* to, const T2* from, isize size) noexcept -> bool
    {
        return compare_bytes(to, from, size) == 0;
    }

    inline proc byteswap(u8 output[], u8 input[], isize size) noexcept -> void
    {
        for (isize i = 0; i < size; i++)
            output[i] = input[size - i - 1];
    }

    inline proc byteswap(u8 bytes[], isize size) noexcept -> void
    {
        let half_size = size / 2;
        for (isize i = 0; i < half_size; i++)
            std::swap(bytes[i], bytes[size - i - 1]);
    }

    namespace detail
    {
        templ_func manual_byteswap(T in value) noexcept -> T
        {
            //Uses of bit_cast to enable consteval of this fn
            mut rep = to_bytes(value);
            byteswap(rep, rep.size);
            return bit_cast<T>(rep);
        }
    }

    //wrapper for byteswap proc in c++23
    // will in most cases get compiled into bswap instruction
    templ_func byteswap(T in value) noexcept -> T
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


#undef templ_func

#undef bswap_32
#undef bswap_64

#include "undefs.h"