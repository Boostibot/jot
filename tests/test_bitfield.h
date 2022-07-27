#pragma once

#include "tester_utils.h"
#include "../lib/jot/bitfield.h"
#include "../lib/jot/defines.h"

namespace jot::tests 
{
    //div_round_up should return multiple of modulo equal or bigger to the number given
    test test_div_round_up()
    {
        static_assert(div_round_up(3, 8) == 1);
        static_assert(div_round_up(1, 8) == 1);
        static_assert(div_round_up(0, 8) == 0);
        static_assert(div_round_up(7, 8) == 1);
        static_assert(div_round_up(8, 8) == 1);

        static_assert(div_round_up(8, 4) == 2);
        static_assert(div_round_up(35, 8) == 5);
        static_assert(div_round_up(48, 8) == 6);
    }

    test test_get_bytefield()
    {
        {
            constexpr u8 val = 0xAA;

            hybrid_assert(get_bytefield<u32>(val, 0, 0) == 0);
            hybrid_assert(get_bytefield<u32>(val, 0, 1) == val);
        }

        {
            constexpr let rep = to_array<u8>({0xAA, 0xBB});
            constexpr u16 val = from_bytes<u16>(rep);

            hybrid_assert(get_bytefield<u32>(val, 0, 0) == 0);
            hybrid_assert(get_bytefield<u32>(val, 0, 1) == 0xAA);
            hybrid_assert(get_bytefield<u32>(val, 1, 0) == 0);
            hybrid_assert(get_bytefield<u32>(val, 1, 1) == 0xBB);
            hybrid_assert(get_bytefield<u32>(val, 0, 2) == val);
        }

        {
            constexpr let rep = to_array<u8>({0xAA, 0xBB, 0xCC, 0xDD});
            constexpr u32 val = from_bytes<u32>(rep);

            constexpr u32 subval1 = from_bytes<u16>(ByteArray<2>{0xAA, 0xBB});
            constexpr u32 subval2 = from_bytes<u16>(ByteArray<2>{0xBB, 0xCC});

            hybrid_assert(get_bytefield<u32>(val, 3, 0) == 0);
            hybrid_assert(get_bytefield<u32>(val, 0, 2) == subval1);
            hybrid_assert(get_bytefield<u32>(val, 1, 2) == subval2);
            hybrid_assert(get_bytefield<u32>(val, 0, 4) == val);

            constexpr u32 after_set1 = set_bytefield<u32>(val, 1, 3, 0x112233);
            hybrid_assert(get_bytefield<u32>(after_set1, 1, 3) == 0x112233);
            hybrid_assert(get_bytefield<u32>(after_set1, 1, 2) != subval2);
        }

        {
            constexpr let rep = to_array<u8>({0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x88});
            constexpr u64 val = from_bytes<u64>(rep);

            constexpr u64 subval1 = from_bytes<u16>(ByteArray<2>{0x00, 0x88});
            constexpr u64 subval2 = from_bytes<u16>(ByteArray<2>{0xEE, 0xFF});
            constexpr u64 subval3 = from_bytes<u32>(ByteArray<4>{0xEE, 0xFF, 0x00, 0x88});
            constexpr u64 subval4 = from_bytes<u32>(ByteArray<4>{0xAA, 0xBB, 0xCC, 0xDD});

            hybrid_assert(get_bytefield<u64>(val, 7, 1) == 0x88);
            hybrid_assert(get_bytefield<u64>(val, 6, 1) == 0x00);
            hybrid_assert(get_bytefield<u64>(val, 6, 2) == subval1);
            hybrid_assert(get_bytefield<u64>(val, 4, 2) == subval2);
            hybrid_assert(get_bytefield<u64>(val, 4, 2, subval3) == subval3);
            hybrid_assert(get_bytefield<u64>(val, 4, 4) == subval3);
            hybrid_assert(get_bytefield<u64>(val, 0, 4) == subval4);
            hybrid_assert(get_bytefield<u64>(val, 0, 2, subval4) == subval4);
            hybrid_assert(get_bytefield<u64>(val, 0, 8) == val);


            constexpr u64 after_set1 = set_bytefield<u64>(val, 1, 5, 0x1122334455);
            hybrid_assert(get_bytefield<u64>(after_set1, 1, 2) != subval2);
            hybrid_assert(get_bytefield<u64>(after_set1, 1, 5) == 0x1122334455);
        }
    }

    test test_set_bytefield()
    {
        {
            hybrid_assert(set_bytefield<u8>(0U, 0, 1, 0xAAU) == 0xAA);
            hybrid_assert(set_bytefield<u8>(0U, 0, 0, 0xAAU) == 0);
            hybrid_assert(set_bytefield<u8>(0xBBU, 0, 0, 0xAAU) == 0xBB);
            hybrid_assert(set_bytefield<u8>(0xBBU, 1, 0, 0xAAU) == 0xBB);

            constexpr let val1 = set_bytefield<u16>(0U, 0, 1, 0xAAU);
            constexpr let val2 = set_bytefield<u16>(val1, 1, 1, 0xBBU);
            constexpr let composed = from_bytes<u16>(ByteArray<2>{0xAA, 0xBB});

            hybrid_assert(get_bytefield<u16>(val1, 0, 0) == 0x00);
            hybrid_assert(get_bytefield<u16>(val1, 0, 1) == 0xAA);
            hybrid_assert(get_bytefield<u16>(val2, 1, 1) == 0xBB);
            hybrid_assert(get_bytefield<u16>(val2, 0, 2) == composed);
        }

        {
            constexpr let val1 = set_bytefield<u32>(0U, 0, 1, 0xAAU);
            constexpr let val2 = set_bytefield<u32>(val1, 1, 1, 0xBBU);
            constexpr let val3 = set_bytefield<u32>(val2, 2, 1, 0xCCU);
            constexpr let val4 = set_bytefield<u32>(val3, 3, 1, 0xDDU);

            constexpr let composed = from_bytes<u32>(ByteArray<4>{0xAA, 0xBB, 0xCC, 0xDD});
            constexpr let sub_coposed = from_bytes<u32>(ByteArray<4>{0xBB, 0xCC});

            hybrid_assert(get_bytefield<u32>(val4, 0, 0) == 0x00);
            hybrid_assert(get_bytefield<u32>(val4, 1, 1) == 0xBB);
            hybrid_assert(get_bytefield<u32>(val4, 3, 1) == 0xDD);
            hybrid_assert(get_bytefield<u32>(val4, 0, 4) == composed);
            hybrid_assert(get_bytefield<u32>(val4, 1, 2) == sub_coposed);

            constexpr let val5 = set_bytefield<u32>(val4, 1, 3, 0x112233);
            hybrid_assert(get_bytefield<u32>(val5, 0, 1) == 0xAA);
            hybrid_assert(get_bytefield<u32>(val5, 1, 3) == 0x112233);
        }

        {
            constexpr let val1 = set_bytefield<u64, u64>(0U, 0, 1, 0xAAU);
            constexpr let val2 = set_bytefield<u64>(val1, 1, 2, 0xBBCCU);
            constexpr let val3 = set_bytefield<u64>(val2, 3, 4, 0x11223344);
            constexpr let val4 = set_bytefield<u64>(val3, 7, 1, 0xDDU);

            hybrid_assert(get_bytefield<u64>(val4, 0, 1) == 0xAAU);
            hybrid_assert(get_bytefield<u64>(val4, 1, 2) == 0xBBCCU);
            hybrid_assert(get_bytefield<u64>(val4, 3, 4) == 0x11223344);
            hybrid_assert(get_bytefield<u64>(val4, 7, 1) == 0xDDU);

            constexpr let val5 = set_bytefield<u64>(val4, 1, 7, 0x10203040506070);
            hybrid_assert(get_bytefield<u64>(val5, 0, 1) == 0xAA);
            hybrid_assert(get_bytefield<u64>(val5, 1, 2) != 0xBBCCU);
            hybrid_assert(get_bytefield<u64>(val5, 3, 4) != 0x11223344);
            hybrid_assert(get_bytefield<u64>(val5, 1, 7) == 0x10203040506070);
        }
    }


    test test_get_array_bytefield()
    {
        {
            constexpr byte vals[] = {0xAA, 0xBB, 0xCC, 0xDD};

            hybrid_assert(get_array_bytefield<u32>(vals, 0, 0) == 0);
            hybrid_assert(get_array_bytefield<u32>(vals, 0, 1) == 0xAA);
            hybrid_assert(get_array_bytefield<u32>(vals, 1, 1) == 0xBB);
            hybrid_assert(get_array_bytefield<u32>(vals, 2, 1) == 0xCC);
            hybrid_assert(get_array_bytefield<u32>(vals, 3, 1) == 0xDD);
            hybrid_assert(get_array_bytefield<u32>(vals, 2, 2) == from_bytes<u16>(Bytes<u16>{0xCC, 0xDD}));
            hybrid_assert(get_array_bytefield<u32>(vals, 1, 2) == from_bytes<u16>(Bytes<u16>{0xBB, 0xCC}));
            hybrid_assert(get_array_bytefield<u32>(vals, 0, 4) == from_bytes<u32>(Bytes<u32>{0xAA, 0xBB, 0xCC, 0xDD}));
        }

        {
            constexpr let byte_vals = to_array<byte>({
                0xAA, 0xBB, 0xCC, 0xDD, 
                0x11, 0x22, 0x33, 0x44,
                0x66, 0x77, 0x88, 0x99});

            using ValsArr = Array<u32, 3>;
            static_assert(sizeof(ValsArr) == sizeof(byte_vals));
            constexpr ValsArr vals = bitcast<ValsArr>(byte_vals);
            
            constexpr let conv = [](u64 val){ return to_endian(val, Endian::Little); };

            hybrid_assert(get_array_bytefield<u32>(vals.data, 9, 0) == 0);
            hybrid_assert(get_array_bytefield<u32>(vals.data, 5, 0) == 0);
            hybrid_assert(get_array_bytefield<u32>(vals.data, 0, 1) == 0xAA);
            hybrid_assert(get_array_bytefield<u32>(vals.data, 1, 1) == 0xBB);

            hybrid_assert(get_array_bytefield<u32>(vals.data, 2, 2) == conv(0xDDCC));
            hybrid_assert(get_array_bytefield<u32>(vals.data, 1, 2) == conv(0xCCBB));
            hybrid_assert(get_array_bytefield<u32>(vals.data, 0, 4) == conv(0xDDCCBBAA));

            hybrid_assert(get_array_bytefield<u32>(vals.data, 5, 2) == conv(0x3322));
            hybrid_assert(get_array_bytefield<u32>(vals.data, 0, 3) == conv(0xCCBBAA));
            hybrid_assert(get_array_bytefield<u32>(vals.data, 8, 4) == conv(0x99887766));

            hybrid_assert(get_array_bytefield<u32>(vals.data, 5, 3) == conv(0x443322));
            hybrid_assert(get_array_bytefield<u64>(vals.data, 3, 5) == conv(0x44332211'DD));
            hybrid_assert(get_array_bytefield<u64>(vals.data, 3, 7) == conv(0x7766'44332211'DD));
        }

        {
            constexpr let byte_vals = to_array<byte>({
                0xAA, 0xBB, 0xCC, 0xDD, 
                0x11, 0x22, 0x33, 0x44,
                0x66, 0x77, 0x88, 0x99,
                0xAA, 0xBB, 0xCC, 0xDD});

            using ValsArr = Array<u64, 2>;
            static_assert(sizeof(ValsArr) == sizeof(byte_vals));
            constexpr ValsArr vals = bitcast<ValsArr>(byte_vals);

            constexpr let conv = [](u64 val){ return to_endian(val, Endian::Little); };

            hybrid_assert(get_array_bytefield<u32>(vals.data, 9, 0) == 0);
            hybrid_assert(get_array_bytefield<u32>(vals.data, 5, 0) == 0);
            hybrid_assert(get_array_bytefield<u32>(vals.data, 0, 1) == 0xAA);
            hybrid_assert(get_array_bytefield<u32>(vals.data, 1, 1) == 0xBB);

            hybrid_assert(get_array_bytefield<u32>(vals.data, 2, 2) == conv(0xDDCC));
            hybrid_assert(get_array_bytefield<u32>(vals.data, 1, 2) == conv(0xCCBB));
            hybrid_assert(get_array_bytefield<u32>(vals.data, 0, 4) == conv(0xDDCCBBAA));

            hybrid_assert(get_array_bytefield<u32>(vals.data, 5, 2) == conv(0x3322));
            hybrid_assert(get_array_bytefield<u32>(vals.data, 0, 3) == conv(0xCCBBAA));
            hybrid_assert(get_array_bytefield<u32>(vals.data, 8, 4) == conv(0x99887766));

            hybrid_assert(get_array_bytefield<u32>(vals.data, 5, 3) == conv(0x443322));
            hybrid_assert(get_array_bytefield<u64>(vals.data, 3, 5) == conv(0x44332211'DD));
            hybrid_assert(get_array_bytefield<u64>(vals.data, 3, 7) == conv(0x7766'44332211'DD));
        }
    }

    template <typename Ret>
    func get_bitfield_tester(Ret val, let from_bit, let bit_count, let expected)
    {
        let rep1 = to_bytes(val);
        Array<u16, 8> rep2 = {0};
        Array<u32, 4> rep3 = {0};
        Array<u64, 2> rep4 = {0};
        copy_bytes(rep2.data, rep1.data, rep1.size);
        copy_bytes(rep3.data, rep1.data, rep1.size);
        copy_bytes(rep4.data, rep1.data, rep1.size);

        return (get_bitfield<Ret, Ret>(val, from_bit, bit_count) == expected)
            && (get_array_bitfield<Ret>(rep1.data, from_bit, bit_count) == expected)
            && (get_array_bitfield<Ret>(rep2.data, from_bit, bit_count) == expected)
            && (get_array_bitfield<Ret>(rep3.data, from_bit, bit_count) == expected)
            && (get_array_bitfield<Ret>(rep4.data, from_bit, bit_count) == expected);
    }

    //get_bitfield should return an unsigned value represented by the bit span in the fields array
    test test_get_bitfield()
    {
        //u8
        {
            constexpr u8 val = 0b10101111;

            hybrid_assert(get_bitfield_tester(val, 0, 0, 0b0));
            hybrid_assert(get_bitfield_tester(val, 8, 0, 0b0));
            hybrid_assert(get_bitfield_tester(val, 0, 8, 0b10101111));
            hybrid_assert(get_bitfield_tester(val, 1, 7, 0b1010111));
            hybrid_assert(get_bitfield_tester(val, 5, 2, 0b01));

            hybrid_assert(get_bitfield_tester(val, 1, 1, 0b1));
            hybrid_assert(get_bitfield_tester(val, 3, 1, 0b1));
            hybrid_assert(get_bitfield_tester(val, 5, 1, 0b1));
            hybrid_assert(get_bitfield_tester(val, 7, 1, 0b1));

            hybrid_assert(get_bitfield_tester(val, 0, 1, 0b1));
            hybrid_assert(get_bitfield_tester(val, 2, 1, 0b1));
            hybrid_assert(get_bitfield_tester(val, 4, 1, 0b0));
            hybrid_assert(get_bitfield_tester(val, 6, 1, 0b0));
        }

        //u16
        {
            constexpr u16 val = 0b10101010'10101010;

            hybrid_assert(get_bitfield_tester(val, 0, 0, 0b0));
            hybrid_assert(get_bitfield_tester(val, 16, 0, 0b0));
            hybrid_assert(get_bitfield_tester(val, 0, 16, 0b10101010'10101010));
            hybrid_assert(get_bitfield_tester(val, 1, 15, 0b10101010'1010101));
            hybrid_assert(get_bitfield_tester(val, 2, 14, 0b10101010'101010));
            hybrid_assert(get_bitfield_tester(val, 2, 13, 0b00101010'101010));
            hybrid_assert(get_bitfield_tester(val, 3, 10, 0b00001010'10101));
            hybrid_assert(get_bitfield_tester(val, 3, 10, 0b00001010'10101));

            hybrid_assert(get_bitfield_tester(val, 1, 1, 0b1));
            hybrid_assert(get_bitfield_tester(val, 3, 1, 0b1));
            hybrid_assert(get_bitfield_tester(val, 5, 1, 0b1));
            hybrid_assert(get_bitfield_tester(val, 7, 1, 0b1));
            hybrid_assert(get_bitfield_tester(val, 9, 1, 0b1));

            hybrid_assert(get_bitfield_tester(val, 0, 1, 0b0));
            hybrid_assert(get_bitfield_tester(val, 2, 1, 0b0));
            hybrid_assert(get_bitfield_tester(val, 4, 1, 0b0));
            hybrid_assert(get_bitfield_tester(val, 6, 1, 0b0));
        }

        //u32
        {
            constexpr u32 val = 0b11110000'00001111'10101010'10101010;

            //Same as for u16
            hybrid_assert(get_bitfield_tester(val, 0, 0, 0b0));
            hybrid_assert(get_bitfield_tester(val, 16, 0, 0b0));
            hybrid_assert(get_bitfield_tester(val, 0, 16, 0b10101010'10101010));
            hybrid_assert(get_bitfield_tester(val, 1, 15, 0b10101010'1010101));
            hybrid_assert(get_bitfield_tester(val, 2, 14, 0b10101010'101010));
            hybrid_assert(get_bitfield_tester(val, 2, 13, 0b00101010'101010));
            hybrid_assert(get_bitfield_tester(val, 3, 10, 0b00001010'10101));
            hybrid_assert(get_bitfield_tester(val, 3, 10, 0b00001010'10101));

            hybrid_assert(get_bitfield_tester(val, 1, 1, 0b1));
            hybrid_assert(get_bitfield_tester(val, 3, 1, 0b1));
            hybrid_assert(get_bitfield_tester(val, 5, 1, 0b1));
            hybrid_assert(get_bitfield_tester(val, 7, 1, 0b1));
            hybrid_assert(get_bitfield_tester(val, 9, 1, 0b1));

            hybrid_assert(get_bitfield_tester(val, 0, 1, 0b0));
            hybrid_assert(get_bitfield_tester(val, 2, 1, 0b0));
            hybrid_assert(get_bitfield_tester(val, 4, 1, 0b0));
            hybrid_assert(get_bitfield_tester(val, 6, 1, 0b0));

            //u32
            hybrid_assert(get_bitfield_tester(val, 32, 0, 0b0));
            hybrid_assert(get_bitfield_tester(val, 0, 32, 0b11110000'00001111'10101010'10101010));

            hybrid_assert(get_bitfield_tester(val, 16, 16, 0b11110000'00001111));
            hybrid_assert(get_bitfield_tester(val, 16, 8, 0b00001111));
            hybrid_assert(get_bitfield_tester(val, 24, 8, 0b11110000));
            hybrid_assert(get_bitfield_tester(val, 28, 4, 0b00001111));

            hybrid_assert(get_bitfield_tester(val, 1, 15, 0b10101010'1010101));
            hybrid_assert(get_bitfield_tester(val, 2, 14, 0b10101010'101010));
            hybrid_assert(get_bitfield_tester(val, 2, 13, 0b00101010'101010));
            hybrid_assert(get_bitfield_tester(val, 3, 10, 0b00001010'10101));
            hybrid_assert(get_bitfield_tester(val, 3, 10, 0b00001010'10101));

            hybrid_assert(get_bitfield_tester(val, 1, 1, 0b1));
            hybrid_assert(get_bitfield_tester(val, 3, 1, 0b1));
            hybrid_assert(get_bitfield_tester(val, 5, 1, 0b1));
            hybrid_assert(get_bitfield_tester(val, 7, 1, 0b1));
            hybrid_assert(get_bitfield_tester(val, 9, 1, 0b1));

            hybrid_assert(get_bitfield_tester(val, 0, 1, 0b0));
            hybrid_assert(get_bitfield_tester(val, 2, 1, 0b0));
            hybrid_assert(get_bitfield_tester(val, 4, 1, 0b0));
            hybrid_assert(get_bitfield_tester(val, 6, 1, 0b0));
        }
    }
    template <typename To>
    func extract_bytes(let& from_arr)
    {
        Bytes<To> rep = {0};
        copy_bytes(rep.data, from_arr.data, rep.size);
        return from_bytes<To>(rep);
    }

    template <typename T>
    func set_bitfield_tester(T val, let from_bit, let bit_count, let value, let expected)
    {
        mut rep1 = to_bytes(val);
        Array<u16, 8> rep2 = {0};
        Array<u32, 4> rep3 = {0};
        Array<u64, 2> rep4 = {0};
        copy_bytes(rep2.data, rep1.data, rep1.size);
        copy_bytes(rep3.data, rep1.data, rep1.size);
        copy_bytes(rep4.data, rep1.data, rep1.size);

        set_array_bitfield(rep1.data, from_bit, bit_count, value);
        set_array_bitfield(rep2.data, from_bit, bit_count, value);
        set_array_bitfield(rep3.data, from_bit, bit_count, value);
        set_array_bitfield(rep4.data, from_bit, bit_count, value);

        return (set_bitfield(val, from_bit, bit_count, value) == expected)
            && (extract_bytes<T>(rep1) == expected) 
            && (extract_bytes<T>(rep2) == expected) 
            && (extract_bytes<T>(rep3) == expected) 
            && (extract_bytes<T>(rep4) == expected);
    }

    
    //set_bitfield should set a bit span in the fields array to an usnigned value
    test test_set_bitfield()
    {
        //u8
        {
            constexpr u8 val = 0b10101111;

            hybrid_assert(set_bitfield_tester(val, 0, 8, 0b0101U, 0b0000101));
            hybrid_assert(set_bitfield_tester(val, 1, 0, 4U, 0b10101111));
            hybrid_assert(set_bitfield_tester(val, 1, 0, 4634141U, 0b10101111));
            hybrid_assert(set_bitfield_tester(val, 8, 0, 6464U, 0b10101111));

            hybrid_assert(set_bitfield_tester(val, 6, 1, 0b0001U, 0b11101111));
            hybrid_assert(set_bitfield_tester(val, 1, 3, 0b0000U, 0b10100001));
            hybrid_assert(set_bitfield_tester(val, 4, 4, 0b1111U, 0b11111111));

            hybrid_assert(set_bitfield_tester(val, 6, 2, 0b111100'0101U, 0b01101111));

        }

        //u16
        {
            constexpr u16 val = 0b11110000'10101111;

            //from u8 with prefixed new byte
            hybrid_assert(set_bitfield_tester(val, 1, 0, 4U, 0b11110000'10101111));
            hybrid_assert(set_bitfield_tester(val, 1, 0, 4634141U, 0b11110000'10101111));
            hybrid_assert(set_bitfield_tester(val, 8, 0, 6464U, 0b11110000'10101111));

            hybrid_assert(set_bitfield_tester(val, 1, 3, 0b0000U, 0b11110000'10100001));
            hybrid_assert(set_bitfield_tester(val, 4, 4, 0b1111U, 0b11110000'11111111));

            //u16
            hybrid_assert(set_bitfield_tester(val, 6, 4, 0b1111U, 0b11110011'11101111) && "set through byte boundary");
            hybrid_assert(set_bitfield_tester(val, 8, 8, 0b00001111U, 0b00001111'10101111) && "set whole byte");
            hybrid_assert(set_bitfield_tester(val, 0, 8, 0b00001111U, 0b11110000'00001111));
            hybrid_assert(set_bitfield_tester(val, 7, 9, 0b0000111100U, 0b000011110'00101111) && "set through byte boundary more than one byte");
            hybrid_assert(set_bitfield_tester(val, 0, 16, 0b10111111'10101010U, 0b10111111'10101010 ) &&"set all");
        }

        //u32
        {
            constexpr u32 val = 0b11110101'00001111'11110000'10101111;

            hybrid_assert(set_bitfield_tester(val, 0, 32, 0b101U, 0b101));
            hybrid_assert(set_bitfield_tester(val, 12, 12, 0b11110000'0000U, 0b11110101'11110000'00000000'10101111));
            hybrid_assert(set_bitfield_tester(val, 13, 1, 0b11110000'0000U, 0b11110101'00001111'11010000'10101111));
            hybrid_assert(set_bitfield_tester(val, 30, 2, 0b10U, 0b10110101'00001111'11110000'10101111));
            hybrid_assert(set_bitfield_tester(val, 16, 8, 0b11110000U, 0b11110101'11110000'11110000'10101111));
            hybrid_assert(set_bitfield_tester(val, 24, 8, 0b11110000U, 0b11110000'00001111'11110000'10101111));
        }
    }

    namespace detail
    {
        template <size_t size>
        using B = Bitfield<bool, size>;
    }
    
    test test_bit_field_info()
    {
        using detail::B;

        using BS1 = BitStorage<B<1>, B<4>, B<8>, B<3>>;
        using BS2 = BitStorage<B<1>, B<4>, B<3>, B<8>>;
        using BS3 = BitStorage<B<2>, B<4>, B<3>, B<8>>;
        using BS4 = BitStorage<B<1>, B<3>>;
        using BS5 = BitStorage<B<1>, B<32>, B<31>, B<34>, B<1>, B<1>, B<1>>;

        //byte_size, bit_count and field_count should match
        static_assert(BS1::byte_size == 2);
        static_assert(BS2::byte_size == 2);
        static_assert(BS3::byte_size == 3);
        static_assert(BS4::byte_size == 1);
        static_assert(BS5::byte_size == 13);

        static_assert(BS1::bit_count == 16);
        static_assert(BS2::bit_count == 16);
        static_assert(BS3::bit_count == 17);
        static_assert(BS4::bit_count == 4);
        static_assert(BS5::bit_count == 101);

        static_assert(BS1::info.field_count == 4);
        static_assert(BS2::info.field_count == 4);
        static_assert(BS3::info.field_count == 4);
        static_assert(BS4::info.field_count == 2);
        static_assert(BS5::info.field_count == 7);

        //bit counts and positions for each field should be correct
        static_assert(BS1::info.bit_count[0] == 1);
        static_assert(BS1::info.bit_count[1] == 4);
        static_assert(BS1::info.bit_count[2] == 8);
        static_assert(BS1::info.bit_count[3] == 3);

        static_assert(BS1::info.from_bit[0] == 0);
        static_assert(BS1::info.from_bit[1] == 1);
        static_assert(BS1::info.from_bit[2] == 5);
        static_assert(BS1::info.from_bit[3] == 13);

        static_assert(BS1::info.to_bit[0] == 1);
        static_assert(BS1::info.to_bit[1] == 5);
        static_assert(BS1::info.to_bit[2] == 13);
        static_assert(BS1::info.to_bit[3] == 16);
    }

    template <typename Container>
    test test_bitfield_outside_storage()
    {
        using BS1 = BitStorage<
            Bitfield<bool, 1>, 
            Bitfield<u16, 4>, 
            Bitfield<u64, 8>, 
            Bitfield<u16, 3>, 
            Bitfield<u32, 32>>;

        static_assert(are_same_v<BS1::FieldType<0>, bool>);
        static_assert(are_same_v<BS1::FieldType<1>, u16>);
        static_assert(are_same_v<BS1::FieldType<2>, u64>);
        static_assert(are_same_v<BS1::FieldType<3>, u16>);
        static_assert(are_same_v<BS1::FieldType<4>, u32>);

        constexpr size_t cont_count = div_round_up(BS1::bit_count, BIT_COUNT<Container>);
        Container conts[cont_count] = {0};
        byte* cont = nullptr;
        static_assert(sizeof(cont) >= BS1::byte_size);

        //Value accessors
        runtime_assert(BS1::get<0>(cont) == false); 
        runtime_assert(BS1::get<1>(cont) == 0); 
        runtime_assert(BS1::get<2>(cont) == 0); 
        runtime_assert(BS1::get<3>(cont) == 0); 
        runtime_assert(BS1::get<4>(cont) == 0); 
        
        cont = BS1::set<0>(cont, true);
        cont = BS1::set<1>(cont, 15);
        cont = BS1::set<2>(cont, 123);
        cont = BS1::set<3>(cont, 7);
        cont = BS1::set<4>(cont, 18301839);
        runtime_assert(BS1::get<0, bool>(cont) == true); 
        runtime_assert(BS1::get<1, u8>(cont) == 15); 
        runtime_assert(BS1::get<2, u8>(cont) == 123); 
        runtime_assert(BS1::get<3, u8>(cont) == 7); 
        runtime_assert(BS1::get<4, u32>(cont) == 18301839); 


        cont = BS1::set<4>(cont, 0);
        cont = BS1::set<1>(cont, 7);
        cont = BS1::set<2>(cont, 255);
        cont = BS1::set<0>(cont, false);
        cont = BS1::set<3>(cont, 2);
        runtime_assert(BS1::get<0, u16>(cont) == 0); 
        runtime_assert(BS1::get<1, u8>(cont) == 7); 
        runtime_assert(BS1::get<2, u8>(cont) == 255); 
        runtime_assert(BS1::get<3, u8>(cont) == 2); 
        runtime_assert(BS1::get<4, u32>(cont) == 0); 

        runtime_assert(BS1::get<0, bool>(cont) == false); 
        runtime_assert(BS1::get<1, u8>(cont) == 7); 
        runtime_assert(BS1::get<2, u64>(cont) == 255); 
        runtime_assert(BS1::get<3, u8>(cont) == 2); 
        runtime_assert(BS1::get<4, u32>(cont) == 0); 


        //Array acessors
        runtime_assert(BS1::array::get<0>(conts) == false); 
        runtime_assert(BS1::array::get<1>(conts) == 0); 
        runtime_assert(BS1::array::get<2>(conts) == 0); 
        runtime_assert(BS1::array::get<3>(conts) == 0); 
        runtime_assert(BS1::array::get<4>(conts) == 0); 

        BS1::array::set<0>(conts, true);
        BS1::array::set<1>(conts, 15);
        BS1::array::set<2>(conts, 123);
        BS1::array::set<3>(conts, 7);
        BS1::array::set<4>(conts, 18301839);
        runtime_assert(BS1::array::get<0, bool>(conts) == true); 
        runtime_assert(BS1::array::get<1, u8>(conts) == 15); 
        runtime_assert(BS1::array::get<2, u8>(conts) == 123); 
        runtime_assert(BS1::array::get<3, u8>(conts) == 7); 
        runtime_assert(BS1::array::get<4, u32>(conts) == 18301839); 


        BS1::array::set<4>(conts, 0);
        BS1::array::set<1>(conts, 7);
        BS1::array::set<2>(conts, 255);
        BS1::array::set<0>(conts, false);
        BS1::array::set<3>(conts, 2);
        runtime_assert(BS1::array::get<0, u16>(conts) == 0); 
        runtime_assert(BS1::array::get<1, u8>(conts) == 7); 
        runtime_assert(BS1::array::get<2, u8>(conts) == 255); 
        runtime_assert(BS1::array::get<3, u8>(conts) == 2); 
        runtime_assert(BS1::array::get<4, u32>(conts) == 0); 

        runtime_assert(BS1::array::get<0, bool>(conts) == false); 
        runtime_assert(BS1::array::get<1, u8>(conts) == 7); 
        runtime_assert(BS1::array::get<2, u64>(conts) == 255); 
        runtime_assert(BS1::array::get<3, u8>(conts) == 2); 
        runtime_assert(BS1::array::get<4, u32>(conts) == 0); 
    }

    test test_bitfield_inside_storage()
    {
        using BS1 = BitStorage<
            Bitfield<bool, 1>, 
            Bitfield<u16, 4>, 
            Bitfield<u16, 8>, 
            Bitfield<u16, 3>, 
            Bitfield<u32, 32>>;

        BS1 bitstorage = {0};

        runtime_assert(bitstorage.get<0>() == false); 
        runtime_assert(bitstorage.get<1>() == 0); 
        runtime_assert(bitstorage.get<2>() == 0); 
        runtime_assert(bitstorage.get<3>() == 0); 
        runtime_assert(bitstorage.get<4>() == 0); 

        bitstorage.set<0>(true);
        bitstorage.set<1>(15);
        bitstorage.set<2>(123);
        bitstorage.set<3>(7);
        bitstorage.set<4>(18301839);
        runtime_assert(bitstorage.get<0>() == true); 
        runtime_assert(bitstorage.get<1>() == 15); 
        runtime_assert(bitstorage.get<2>() == 123); 
        runtime_assert(bitstorage.get<3>() == 7); 
        runtime_assert(bitstorage.get<4>() == 18301839); 


        bitstorage.set<4>(0);
        bitstorage.set<1>(7);
        bitstorage.set<2>(255);
        bitstorage.set<0>(false);
        bitstorage.set<3>(2);
        runtime_assert(bitstorage.get<0>() == 0); 
        runtime_assert(bitstorage.get<1>() == 7); 
        runtime_assert(bitstorage.get<2>() == 255); 
        runtime_assert(bitstorage.get<3>() == 2); 
        runtime_assert(bitstorage.get<4>() == 0); 

        runtime_assert(bitstorage.get<0l>() == false); 
        runtime_assert(bitstorage.get<1>() == 7); 
        runtime_assert(bitstorage.get<2>() == 255); 
        runtime_assert(bitstorage.get<3>() == 2); 
        runtime_assert(bitstorage.get<4>() == 0); 
    }

    //Test get bitspan set bitspan on smaller types
    test test_bitfield_class()
    {
        test_bitfield_outside_storage<u8>();
        test_bitfield_outside_storage<u16>();
        test_bitfield_outside_storage<u32>();
        test_bitfield_outside_storage<u64>();
        test_bitfield_inside_storage();
    }

    test test_bitfield()
    {
        test_div_round_up();
        test_get_bytefield();
        test_set_bytefield();
        test_get_array_bytefield();

        test_get_bitfield();
        test_set_bitfield();
        test_bitfield_class();
    }


    run_test(test_bitfield);
}

#include "../lib/jot/undefs.h"