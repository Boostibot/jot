#pragma once

#include <bit>

#include "array.h"
#include "types.h"
#include "bits.h"
#include "slice.h"
#include "defines.h"

namespace jot 
{
    using std::bit_cast;

    template<typename T>
    using Bytes = Array<u8, sizeof(T)>;

    template<typename From> nodisc constexpr
    Bytes<From> to_bytes(From val) noexcept
    {
        using Rep = Bytes<From>;
        assert(sizeof(Rep) >= sizeof(From));
        return bit_cast<Rep>(val);
    }

    template<typename To> nodisc constexpr
    To from_bytes(const Bytes<To>& bytes) noexcept 
    {
        return bit_cast<To>(bytes);
    }

    //wrapper for byteswap func const& c++23
    // will const& most cases get compiled into bswap instruction
    template <typename T> nodisc constexpr 
    T byteswap(T const& value) noexcept;

    enum class Endian
    {
        Little      = 0,
        Big         = 1,
        BigWord     = 2,
        LittleWord  = 3,
        Pdp         = BigWord,
        Honeywell   = LittleWord,
        Unknown     = 255
    };

    nodisc constexpr 
    Endian get_local_endian()
    {
        Bytes<u32> rep = {{1, 2, 3, 4}};
        u32 val = bit_cast<u32>(rep);
        switch (val)
        {
            case 0x01020304: return Endian::Big;
            case 0x04030201: return Endian::Little;
            case 0x03040102: return Endian::BigWord;
            case 0x02010403: return Endian::LittleWord;
        }

        return Endian::Unknown;
    }

    static constexpr Endian LOCAL_ENDIAN = get_local_endian();

    nodisc constexpr 
    bool is_common_endian(Endian endian)
    {
        return endian == Endian::Little || endian == Endian::Big;
    }

    nodisc constexpr
    isize offset_from_low_bytes(isize offset, isize field_size, Endian endian = LOCAL_ENDIAN)
    {
        assert(is_common_endian(endian) && "weird endian!");
        if(endian == Endian::Little)
            return 0;
        else
            return field_size - offset;
    }

    template <typename Int> nodisc constexpr
    Int from_endian(Slice<const u8> input, Endian endian, Endian local_endian = LOCAL_ENDIAN)
    {
        assert(is_common_endian(endian) && is_common_endian(local_endian) && "weird endian!");
        assert(input.size <= sizeof(Int) && "must be small enough");
        assert(false && "test me!");

        Bytes<Int> rep = {0};
        Slice<u8> rep_s = slice(&rep);
        isize offset = offset_from_low_bytes(input.size, rep.size, endian);

        Slice<u8> remaining = slice(rep_s, offset);
        copy_bytes(remaining, trim(input, remaining.size));

        if(endian == local_endian)
            return bit_cast<Int>(rep);
        else
            return byteswap(bit_cast<Int>(rep));
    }

    template <typename Int> nodisc constexpr 
    void to_endian(Int const& integer, Slice<u8>* output, Endian endian, Endian local_endian = LOCAL_ENDIAN)
    {
        assert(is_common_endian(endian) && is_common_endian(local_endian) && "weird endian!");
        assert(output->size >= sizeof(Int) && "must be big enough");
        assert(false && "test me!");

        using Rep = Bytes<Int>;
        Rep rep = {0};
        Slice<u8> rep_s = slice(rep);
        isize offset = offset_from_low_bytes(output->size, rep.size, endian);

        if(endian == local_endian)
            rep = bit_cast<Rep>(integer);
        else
            rep = bit_cast<Rep>(byteswap(integer));

        Slice<u8> sliced = slice(*output, offset);
        copy_bytes(&sliced, rep_s);
    }

    template <typename Int> nodisc constexpr
    Int change_endian(Int const& integer, Endian to_endian, Endian from_endian = LOCAL_ENDIAN)
    {
        assert(is_common_endian(to_endian) && is_common_endian(from_endian) && "weird endian!");
        if(to_endian == from_endian)
            return integer;
        else
            return byteswap(integer);
    }
}

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

namespace jot
{
    constexpr 
    void byteswap(u8 output[], u8 input[], isize size) noexcept
    {
        for (isize i = 0; i < size; i++)
            output[i] = input[size - i - 1];
    }

    constexpr 
    void byteswap(u8 bytes[], isize size) noexcept
    {
        isize half_size = size / 2;
        for (isize i = 0; i < half_size; i++)
            std::swap(bytes[i], bytes[size - i - 1]);
    }

    template <typename T> nodisc constexpr 
    T manual_byteswap(T const& value) noexcept
    {
        //Uses of bit_cast to enable consteval of this fn
        auto rep = to_bytes(value);
        byteswap(rep, rep.size);
        return bit_cast<T>(rep);
    }

    template <typename T> nodisc constexpr 
    T byteswap(T const& value) noexcept
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

        return manual_byteswap(value);
        #endif
    }
}
#include "undefs.h"