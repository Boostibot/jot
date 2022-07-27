#pragma once

#include "tester_utils.h"

#include "../lib/jot/experimental/egnostic_endian.h"
#include "../lib/jot/array.h"
#include "../lib/jot/defines.h"

namespace jot::tests
{
    //from_shifted_bytes should shift all bytes by the given offsets and binary or them into a value
    test test_from_shifted_bytes()
    {
        constexpr Array<byte, 8> little_shifts = {0, 8, 16, 24, 32, 40, 48, 56};
        constexpr Array<byte, 8> big_shifts    = {56, 48, 40, 32, 24, 16, 8, 0};
        constexpr Array<byte, 8> swap_two      = {8, 0, 24, 16, 40, 32, 56, 48};
        constexpr Array<byte, 8> all_on_one    = {0};

        //shifting empty should return 0")
        {
            constexpr Array<byte, 0> empty = {0};

            hybrid_assert(egnostic::from_shifted_bytes<u64>(empty, little_shifts, empty.size) == 0);
            hybrid_assert(egnostic::from_shifted_bytes<u64>(empty, big_shifts, empty.size) == 0);
            hybrid_assert(egnostic::from_shifted_bytes<u64>(empty, swap_two, empty.size) == 0);
        }
    
        //shifting should shift accordingly")
        {
            constexpr Array<byte, 8> bytes = {1, 2, 3, 4, 5, 6, 7, 8};
            hybrid_assert(egnostic::from_shifted_bytes<u64>(bytes, little_shifts, bytes.size) == 0x0807060504030201);
            hybrid_assert(egnostic::from_shifted_bytes<u64>(bytes, big_shifts, bytes.size) == 0x0102030405060708);
            hybrid_assert(egnostic::from_shifted_bytes<u64>(bytes, swap_two, bytes.size)   == 0x0708050603040102);
            hybrid_assert(egnostic::from_shifted_bytes<u64>(bytes, all_on_one, bytes.size) == (1 | 2 | 3 | 4 | 5 | 6 | 7 | 8));
        }
    
        //shifting should handle size")
        {
            //shifting less then shift mask should only shift the given bytes")
            {
                constexpr Array<byte, 4> bytes_1 = {1};
                hybrid_assert(egnostic::from_shifted_bytes<u64>(bytes_1, little_shifts, bytes_1.size) == 0x01);
                hybrid_assert(egnostic::from_shifted_bytes<u64>(bytes_1, big_shifts, bytes_1.size) == 0x0100000000000000);
                hybrid_assert(egnostic::from_shifted_bytes<u64>(bytes_1, swap_two, bytes_1.size) == 0x0100);

                constexpr Array<byte, 4> bytes_2 = {1, 2};
                hybrid_assert(egnostic::from_shifted_bytes<u64>(bytes_2, little_shifts, bytes_2.size) == 0x0201);
                hybrid_assert(egnostic::from_shifted_bytes<u64>(bytes_2, big_shifts, bytes_2.size) == 0x0102000000000000);
                hybrid_assert(egnostic::from_shifted_bytes<u64>(bytes_2, swap_two, bytes_2.size) == 0x0102);

                constexpr Array<byte, 4> bytes_4 = {1, 2, 3, 4};
                hybrid_assert(egnostic::from_shifted_bytes<u64>(bytes_4, little_shifts, bytes_4.size) == 0x04030201);
                hybrid_assert(egnostic::from_shifted_bytes<u64>(bytes_4, big_shifts, bytes_4.size) == 0x0102030400000000);
                hybrid_assert(egnostic::from_shifted_bytes<u64>(bytes_4, swap_two, bytes_4.size) == 0x03040102);
            }
        }
    }

    template <typename type, size_t size>
    func to_shifted_bytes_tester(type value, let shifts)
    {
        Array<byte , size> out = {0};
        egnostic::to_shifted_bytes(value, out, shifts, size);
        return out;
    }

    //to_shifted_bytes should set value of each of given byte to the given value shifted by the according shift
    test test_to_shifted_bytes()
    {
        constexpr Array<u8, 8> little_shifts = {0, 8, 16, 24, 32, 40, 48, 56};
        constexpr Array<u8, 8> big_shifts    = {56, 48, 40, 32, 24, 16, 8, 0};
        constexpr Array<u8, 8> swap_two      = {8, 0, 24, 16, 40, 32, 56, 48};
        constexpr Array<u8, 8> all_on_one    = {0};

        //shifting to empty shoudl do nothing")
        {
            constexpr Array<u8, 0> empty = {0};
            
            hybrid_assert(to_shifted_bytes_tester<u64, 0>(1246443, little_shifts) == empty);
            hybrid_assert(to_shifted_bytes_tester<u64, 0>(0,       big_shifts) == empty);
            hybrid_assert(to_shifted_bytes_tester<u64, 0>(3,       swap_two) == empty);
            hybrid_assert(to_shifted_bytes_tester<u64, 0>(9534,    all_on_one) == empty);
        }

        //shifting to bytes shoudl produce the expected output")
        {
            hybrid_assert(to_shifted_bytes_tester<u64, 8>(0x0807060504030201, little_shifts) == to_array<u8>({1, 2, 3, 4, 5, 6, 7, 8}));
            hybrid_assert(to_shifted_bytes_tester<u64, 8>(0x0807060504030201, big_shifts) == to_array<u8>({8, 7, 6, 5, 4, 3, 2, 1}));
            hybrid_assert(to_shifted_bytes_tester<u64, 8>(0x0807060504030201, swap_two) == to_array<u8>({2, 1, 4, 3, 6, 5, 8, 7}));
            hybrid_assert(to_shifted_bytes_tester<u64, 8>(0x0807060504030201, all_on_one) == to_array<u8>({1, 1, 1, 1, 1, 1, 1, 1}));

            /*{
                std::array<u8, 8> expected = {1, 2, 3, 4, 5, 6, 7, 8};
                egnostic::to_shifted_bytes<u64>(0x0807060504030201, bytes, little_shifts, bytes.size());
                runtime_assert(bytes == expected);
            }
            {
                std::array<u8, 8> expected = {8, 7, 6, 5, 4, 3, 2, 1};
                egnostic::to_shifted_bytes<u64>(0x0807060504030201, bytes, big_shifts, bytes.size());
                runtime_assert(bytes == expected);
            }
            {
                std::array<u8, 8> expected = {2, 1, 4, 3, 6, 5, 8, 7};
                egnostic::to_shifted_bytes<u64>(0x0807060504030201, bytes, swap_two, bytes.size());
                runtime_assert(bytes == expected);
            }
            {
                std::array<u8, 8> expected = {1, 1, 1, 1, 1, 1, 1, 1};
                egnostic::to_shifted_bytes<u64>(0x0807060504030201, bytes, all_on_one, bytes.size());
            
                runtime_assert(bytes == expected);
            }*/
        }
    }

    //from_endian should convert bytes in given endian to value
    test test_egnostic_from_endian()
    {

        constexpr let bytes_8 = to_array<u8>({1, 2, 3, 4, 5, 6, 7, 8});
        constexpr let bytes_7 = to_array<u8>({1, 2, 3, 4, 5, 6, 7});
        constexpr let bytes_6 = to_array<u8>({1, 2, 3, 4, 5, 6});
        constexpr let bytes_5 = to_array<u8>({1, 2, 3, 4, 5});
        constexpr let bytes_4 = to_array<u8>({1, 2, 3, 4});
        constexpr let bytes_3 = to_array<u8>({1, 2, 3});
        constexpr let bytes_2 = to_array<u8>({1, 2});
        constexpr let bytes_1 = to_array<u8>({1});

        hybrid_assert(egnostic::from_endian(bytes_1, Endian::Little) == 0x01);
        hybrid_assert(egnostic::from_endian(bytes_1, Endian::Big) == 0x01);
        hybrid_assert(egnostic::from_endian(bytes_1, Endian::LittleWord) == 0x01);
        hybrid_assert(egnostic::from_endian(bytes_1, Endian::BigWord) == 0x01);

        hybrid_assert(egnostic::from_endian(bytes_2, Endian::Little) == 0x0201);
        hybrid_assert(egnostic::from_endian(bytes_2, Endian::Big) == 0x0102);
        hybrid_assert(egnostic::from_endian(bytes_2, Endian::LittleWord) == 0x0201);
        hybrid_assert(egnostic::from_endian(bytes_2, Endian::BigWord) == 0x0102);
    
        hybrid_assert(egnostic::from_endian(bytes_4, Endian::Little) == 0x04030201);
        hybrid_assert(egnostic::from_endian(bytes_4, Endian::Big) == 0x01020304);
        hybrid_assert(egnostic::from_endian(bytes_4, Endian::LittleWord) == 0x02010403);
        hybrid_assert(egnostic::from_endian(bytes_4, Endian::BigWord) == 0x03040102);
    
        hybrid_assert(egnostic::from_endian(bytes_8, Endian::Little) == 0x0807060504030201);
        hybrid_assert(egnostic::from_endian(bytes_8, Endian::Big) == 0x0102030405060708); 
        hybrid_assert(egnostic::from_endian(bytes_8, Endian::LittleWord) == 0x0201040306050807);
        hybrid_assert(egnostic::from_endian(bytes_8, Endian::BigWord) == 0x0708050603040102);

        //conversions should work on all sizes (not powers of 2)")
        {
            hybrid_assert(egnostic::from_endian(bytes_3, Endian::Little) == 0x030201);
            hybrid_assert(egnostic::from_endian(bytes_3, Endian::Big) == 0x010203);

            hybrid_assert(egnostic::from_endian(bytes_5, Endian::Little) == 0x0504030201);
            hybrid_assert(egnostic::from_endian(bytes_5, Endian::Big) == 0x0102030405); 

            hybrid_assert(egnostic::from_endian(bytes_6, Endian::Little) == 0x060504030201);
            hybrid_assert(egnostic::from_endian(bytes_6, Endian::Big) == 0x010203040506);

            hybrid_assert(egnostic::from_endian(bytes_7, Endian::Little) == 0x07060504030201);
            hybrid_assert(egnostic::from_endian(bytes_7, Endian::Big) == 0x01020304050607); 
        }

        //conversions from exotic endians should work on all sizes (not powers of 2)")
        {
            hybrid_assert(egnostic::from_endian(bytes_3, Endian::LittleWord) == 0x10302);
            hybrid_assert(egnostic::from_endian(bytes_3, Endian::BigWord) == 0x030102);
            hybrid_assert(egnostic::from_endian(bytes_6, Endian::LittleWord) == 0x020104030605);
            hybrid_assert(egnostic::from_endian(bytes_6, Endian::BigWord) == 0x050603040102);
        }
    }

    template<size_t size>
    func to_endian_tester(let value, let endian)
    {
        Array<u8, size> bytes = {0};
        egnostic::to_endian(value, bytes, endian);
        return bytes;
    }

    //to_endian should convert value to bytes in given endian
    test test_egnostic_to_endian()
    {
        //size 1
        {
            constexpr Array<u8, 1> exp1 = {1};
            hybrid_assert(to_endian_tester<1>(0x01, Endian::Little) == exp1);
            hybrid_assert(to_endian_tester<1>(0x01, Endian::Big) == exp1);
            hybrid_assert(to_endian_tester<1>(0x01, Endian::LittleWord) == exp1);
            hybrid_assert(to_endian_tester<1>(0x01, Endian::BigWord) == exp1);
        }
    
        //size 2
        {
            constexpr Array<u8, 2> exp2 = {1, 2};
            hybrid_assert(to_endian_tester<2>(0x0201, Endian::Little) == exp2);
            hybrid_assert(to_endian_tester<2>(0x0102, Endian::Big) == exp2);
            hybrid_assert(to_endian_tester<2>(0x0201, Endian::LittleWord) == exp2);
            hybrid_assert(to_endian_tester<2>(0x0102, Endian::BigWord) == exp2);
        }
    
        //size 4
        {
            constexpr Array<u8, 4> exp4 = {1, 2, 3, 4};
            hybrid_assert(to_endian_tester<4>(0x04030201, Endian::Little) == exp4);
            hybrid_assert(to_endian_tester<4>(0x01020304, Endian::Big) == exp4);
            hybrid_assert(to_endian_tester<4>(0x02010403, Endian::LittleWord) == exp4);
            hybrid_assert(to_endian_tester<4>(0x03040102, Endian::BigWord) == exp4);
        }
    
        //size 8
        {
            constexpr Array<u8, 8> exp8 = {1, 2, 3, 4, 5, 6, 7, 8};
            hybrid_assert(to_endian_tester<8>(0x0807060504030201, Endian::Little) == exp8);
            hybrid_assert(to_endian_tester<8>(0x0102030405060708, Endian::Big) == exp8);
            hybrid_assert(to_endian_tester<8>(0x0201040306050807, Endian::LittleWord) == exp8);
            hybrid_assert(to_endian_tester<8>(0x0708050603040102, Endian::BigWord) == exp8);
        }


        //size 3
        {
            constexpr Array<u8, 3> exp3 = {1, 2, 3};
            hybrid_assert(to_endian_tester<3>(0x030201, Endian::Little) == exp3);
            hybrid_assert(to_endian_tester<3>(0x010203, Endian::Big) == exp3);
            hybrid_assert(to_endian_tester<3>(0x10302, Endian::LittleWord) == exp3);
            hybrid_assert(to_endian_tester<3>(0x030102, Endian::BigWord) == exp3);
        }

        //size 5
        {
            constexpr Array<u8, 5> exp5 = {1, 2, 3, 4, 5};
            hybrid_assert(to_endian_tester<5>(0x0504030201, Endian::Little) == exp5);
            hybrid_assert(to_endian_tester<5>(0x0102030405, Endian::Big) == exp5);
        }

        //size 6
        {

            constexpr Array<u8, 6> exp6 = {1, 2, 3, 4, 5, 6};
            hybrid_assert(to_endian_tester<6>(0x060504030201, Endian::Little) == exp6);
            hybrid_assert(to_endian_tester<6>(0x010203040506, Endian::Big) == exp6);
            hybrid_assert(to_endian_tester<6>(0x020104030605, Endian::LittleWord) == exp6);
            hybrid_assert(to_endian_tester<6>(0x050603040102, Endian::BigWord) == exp6);
        }

        //size 7
        {
            constexpr Array<u8, 7> exp7 = {1, 2, 3, 4, 5, 6, 7};
            hybrid_assert(to_endian_tester<7>(0x07060504030201, Endian::Little) == exp7);
            hybrid_assert(to_endian_tester<7>(0x01020304050607, Endian::Big) == exp7);
        }
    }

    test test_egnostic_endian()
    {
        test_from_shifted_bytes();
        test_to_shifted_bytes();
        test_egnostic_from_endian();
        test_egnostic_to_endian();
    }

    run_test(test_egnostic_endian);
}

#include "../lib/jot/undefs.h"