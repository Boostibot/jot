#pragma once

#include "bits.h"
#include "defines.h"

namespace jot 
{
    using EndianBase = u8;

    namespace detail
    {
        enum EndianType : EndianBase
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
    
    using Endian = detail::EndianType;

    func get_local_endian()
    {
        Bytes<u32> rep = {1, 2, 3, 4};
        static_assert(rep.size == 4, "!!!");

        u32 val = bitcast<u32>(rep);
        switch (val)
        {
            break; case 0x01020304: return Endian::Big;
            break; case 0x04030201: return Endian::Little;
            break; case 0x03040102: return Endian::BigWord;
            break; case 0x02010403: return Endian::LittleWord;
        }

        return Endian::Unknown;
    }

    constexpr let LOCAL_ENDIAN = get_local_endian();

    template <typename EndianT = EndianBase>
    func lower_bytes_offset(let size, let field_size, EndianT endian = LOCAL_ENDIAN)
    {
        using Res = decltype(field_size - size);
        if(endian == Endian::Little)
            return cast(Res) 0;
        else
            return field_size - size;
    }

    template <typename EndianT = EndianBase>
    func higher_bytes_offset(let size, let field_size, EndianT endian = LOCAL_ENDIAN)
    {
        return lower_bytes_offset(size, field_size, endian) + field_size - size; 
    }

    //Optimization that assumes only little and big endian
    // - removes one jump
    template <typename EndianT = EndianBase>
    proc place_endian(let size, let field_size, let endian, mut&& same_lambda, mut&& opposite_lambda, EndianT local_endian = LOCAL_ENDIAN)
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

    template <typename IntegerT, typename EndianT = EndianBase>
    proc from_endian(let* input_bytes, let size, let endian, EndianT local_endian = LOCAL_ENDIAN)
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

    template <typename IntegerT, typename EndianT = EndianBase>
    proc from_endian_to(IntegerT& integer, let* input_bytes, let size, let endian, EndianT local_endian = LOCAL_ENDIAN)
    {
        integer = from_endian<IntegerT>(input_bytes, size, endian, local_endian);
    }

    template <typename IntegerT, typename EndianT = EndianBase>
    proc to_endian(const IntegerT& integer, mut* output_bytes, let size, let endian, EndianT local_endian = LOCAL_ENDIAN)
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

    template <typename IntegerT, typename EndianT = EndianBase>
    proc to_endian(const IntegerT& integer, let to_endian, EndianT from_endian = LOCAL_ENDIAN)
    {
        if(to_endian == from_endian)
            return integer;
        else
            return byteswap(integer);
    }
}

#include "undefs.h"