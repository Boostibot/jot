#pragma once

#include "bits.h"
#include "defines.h"

namespace jot 
{
    using Endian_Base = u8;

    namespace detail
    {
        enum Endian_Type : Endian_Base
        {
            Little = 0,
            Big = 1,
            BigWord = 2,
            Pdp = BigWord,
            LittleWord = 3,
            Honeywell = LittleWord,
            Unknown = 255
        };
    }
    
    using Endian = detail::Endian_Type;

    func get_local_endian() -> Endian
    {
        Bytes<u32> rep = {1, 2, 3, 4};
        static_assert(rep.size == 4, "!!!");

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

    func lower_bytes_offset(size_t size, size_t field_size, Endian endian = LOCAL_ENDIAN) -> size_t
    {
        if(endian == Endian::Little)
            return 0;
        else
            return field_size - size;
    }

    func higher_bytes_offset(size_t size, size_t field_size, Endian endian = LOCAL_ENDIAN) -> size_t
    {
        return lower_bytes_offset(size, field_size, endian) + field_size - size; 
    }

    proc place_endian(size_t size, size_t field_size, Endian endian, mut&& same_lambda, mut&& opposite_lambda, Endian local_endian = LOCAL_ENDIAN) -> auto
    {
        if(endian == Endian::Little)
        {
            if(local_endian == Endian::Little)
                return same_lambda(0);
            else
                return opposite_lambda(0);
        }
        else
        {
            if(local_endian == Endian::Little)
                return opposite_lambda(field_size - size);
            else
                return same_lambda(field_size - size);
        }
    }

    template <typename IntegerT>
    func from_endian(let* input_bytes, size_t size, Endian endian, Endian local_endian = LOCAL_ENDIAN) -> IntegerT
    {
        Bytes<IntegerT> rep = {0};

        return place_endian(size, rep.size, endian, 
            [&](let offset){
                copy_bytes(rep + offset, input_bytes, size);
                return bitcast<IntegerT>(rep);
            },
            [&](let offset){
                copy_bytes(rep + offset, input_bytes, size);
                return byteswap(bitcast<IntegerT>(rep));
            }, local_endian
        );
    }

    template <typename IntegerT>
    proc from_endian_to(IntegerT& integer, let* input_bytes, size_t size, Endian endian, Endian local_endian = LOCAL_ENDIAN) -> void
    {
        integer = from_endian<IntegerT>(input_bytes, size, endian, local_endian);
    }

    template <typename IntegerT>
    proc to_endian(const IntegerT& integer, mut* output_bytes, size_t size, Endian endian, Endian local_endian = LOCAL_ENDIAN) -> void
    {
        using Rep = Bytes<IntegerT>;
        Rep rep;

        return place_endian(size, rep.size, endian, 
            [&](let offset){
                rep = bitcast<Rep>(integer);
                copy_bytes(output_bytes + offset, rep.data, size);
            },
            [&](let offset){
                rep = bitcast<Rep>(byteswap(integer));
                copy_bytes(output_bytes + offset, rep.data, size);
            }, local_endian
        );
    }

    template <typename IntegerT>
    func to_endian(const IntegerT& integer, Endian to_endian, Endian from_endian = LOCAL_ENDIAN) -> IntegerT
    {
        if(to_endian == from_endian)
            return integer;
        else
            return byteswap(integer);
    }
}

#include "undefs.h"