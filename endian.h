#pragma once

#include "types.h"
#include "bits.h"
#include "slice.h"
#include "defines.h"

namespace jot 
{
    enum class Endian
    {
        Little = 0,
        Big = 1,
        BigWord = 2,
        Pdp = BigWord,
        LittleWord = 3,
        Honeywell = LittleWord,
        Unknown = 255
    };

    constexpr func get_local_endian() -> Endian
    {
        Bytes<u32> rep = {1, 2, 3, 4};
        u32 val = bit_cast<u32>(rep);
        switch (val)
        {
            break; case 0x01020304: return Endian::Big;
            break; case 0x04030201: return Endian::Little;
            break; case 0x03040102: return Endian::BigWord;
            break; case 0x02010403: return Endian::LittleWord;
        }

        return Endian::Unknown;
    }

    constexpr Endian LOCAL_ENDIAN = get_local_endian();

    constexpr func offset_from_low_bytes(isize offset, isize field_size, Endian endian = LOCAL_ENDIAN) -> isize
    {
        if(endian == Endian::Little)
            return 0;
        else
            return field_size - offset;
    }

    template <typename Int>
    constexpr func from_endian(Slice<const u8> input, Endian endian, Endian local_endian = LOCAL_ENDIAN) -> Int
    {
        assert(false && "test me!");

        Bytes<Int> rep = {0};
        isize offset = offset_from_low_bytes(size, rep.size, endian);
        if(endian == local_endian)
        {
            copy_bytes(rep + offset, input.data, input.size);
            return bitcast<Int>(rep);
        }
        else
        {
            copy_bytes(rep + offset, input.data, input.size);
            return byteswap(bitcast<Int>(rep));
        }
    }

    template <typename Int>
    constexpr proc to_endian(Int in integer, Slice<u8> output, Endian endian, Endian local_endian = LOCAL_ENDIAN) -> void
    {
        assert(false && "test me!");

        using Rep = Bytes<Int>;
        Rep rep = {0};
        isize offset = offset_from_low_bytes(size, rep.size, endian);

        if(endian == local_endian)
        {
            rep = bitcast<Rep>(integer);
            copy_bytes(output.data + offset, rep.data, output.size);
        }
        else
        {
            rep = bitcast<Rep>(byteswap(integer));
            copy_bytes(output.data + offset, rep.data, output.size);
        }
    }

    template <typename Int>
    constexpr func change_endian(Int in integer, Endian to_endian, Endian from_endian = LOCAL_ENDIAN) -> Int
    {
        if(to_endian == from_endian)
            return integer;
        else
            return byteswap(integer);
    }
}

#include "undefs.h"