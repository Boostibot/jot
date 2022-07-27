#pragma once

#include "../lib/jot/bits.h"
#include "../lib/jot/endian.h"
#include "../lib/jot/defines.h"
#include "tester_utils.h"


namespace jot::tests 
{
    //get_byte shoudl extract a single byte from the given value"
    test test_get_byte()
    {
        constexpr u8 val8 = 0x11;
        constexpr u32 val32 = 0x33221100;
        constexpr u64 val64 = 0x7766554433221100;
        constexpr let arr = to_array<byte>({150, 100, 130, 120});

        hybrid_assert(get_byte(val8, 0) == 0x11);

        hybrid_assert(get_byte(val32, 0) == 0x00);
        hybrid_assert(get_byte(val32, 1) == 0x11);
        hybrid_assert(get_byte(val32, 2) == 0x22);
        hybrid_assert(get_byte(val32, 3) == 0x33);

        hybrid_assert(get_byte(val64, 7) == 0x77);
        hybrid_assert(get_byte(val64, 6) == 0x66);
        hybrid_assert(get_byte(val64, 5) == 0x55);
        hybrid_assert(get_byte(val64, 4) == 0x44);

        hybrid_assert(get_byte(arr, 0) == 150);
        hybrid_assert(get_byte(arr, 1) == 100);
        hybrid_assert(get_byte(arr, 2) == 130);
        hybrid_assert(get_byte(arr, 3) == 120);
    }

    //set_byte shoudl change single byte in the given value and return a result
    test test_set_byte()
    {
        if constexpr (LOCAL_ENDIAN == Endian::Little)
        {
            constexpr u8 val8 = 0x11;
            constexpr u32 val32 = 0x33221100;
            constexpr u64 val64 = 0x7766554433221100;
            constexpr f32 valf1 = 123.0123f;

            hybrid_assert(set_byte(val8, 0, 0x13) == 0x13);

            hybrid_assert(set_byte(val32, 0, 0x22) == 0x33221122);
            hybrid_assert(set_byte(val32, 0, 0x35) == 0x33221135);
            hybrid_assert(set_byte(val32, 2, 0x22) == 0x33221100);
            hybrid_assert(set_byte(val32, 2, 0x35) == 0x33351100);

            hybrid_assert(set_byte(val64, 2, 0x35) == 0x7766554433351100);
            hybrid_assert(set_byte(val64, 7, 0x99) == 0x9966554433221100);

            test set_4 = [](f32 input){
                f32 valf2 = 123.0123f;

                valf2 = set_byte(valf2, 0, get_byte(input, 0));
                valf2 = set_byte(valf2, 1, get_byte(input, 1));
                valf2 = set_byte(valf2, 2, get_byte(input, 2));
                valf2 = set_byte(valf2, 3, get_byte(input, 3));
                return valf2;
            };

            hybrid_assert(set_4(321.01513f) == 321.01513f);

            //make tester and test this
            //let test_array1 = []{
                //let ar1 = to_array<u16>({0x11FF, 0x22FF, 0x33FFU, 153});

            //};

            constexpr Array ar2 = {0x11FF, 0x22FF, 0x33FFU, -153};
            constexpr Array ar3 = {0xFFFF, 0x22FF, 0x33FFU, -153};

        }
    }
    //bit should retun an integer composed of all zeros except for the single bit and its value in the specified index (from lowest order bits)
    
    test test_bit()
    {
        hybrid_assert(bit(2U, 1) == 0b00000100);
        hybrid_assert(bit(2U, 0) == 0b00000000);
        hybrid_assert(bit(16U, 0) == 0b00000000);
        
        hybrid_assert(bit(0U, 1) == 0b00000001);
        hybrid_assert(bit(0U, 0) == 0b00000000);

        hybrid_assert(bit(2U, 3) == 0b00000100);
        hybrid_assert(bit(5U, 165165) == 0b00100000);
        hybrid_assert(bit(7U, true) == 0b10000000);
        hybrid_assert(bit(27U, false) == 0b00000000);

        hybrid_assert(bit(16, 3) == 0b00000000'00000000'00000001'00000000'00000000);
        hybrid_assert(bit(16, 0) == 0b00000000'00000000'00000000'00000000'00000000);
        hybrid_assert(bit(32)    == 0b00000001'00000000'00000000'00000000'00000000);
    }

    //dirty_bit should retun an integer composed of all zeros except for the single bit and its value in the specified index (does not normalize given input)
    test test_dirty_bit()
    {
        hybrid_assert(dirty_bit(1U,  1) == 0b00000010);
        hybrid_assert(dirty_bit(2U,  1) == 0b00000100);
        hybrid_assert(dirty_bit(2U,  0) == 0b00000000);
        hybrid_assert(dirty_bit(16U, 0) == 0b00000000);
        hybrid_assert(dirty_bit(13U, 1) == 0b0010000000000000);
        hybrid_assert(dirty_bit(0U,  1) == 0b00000001);
        hybrid_assert(dirty_bit(0U,  0) == 0b00000000);

        hybrid_assert(dirty_bit(16)    == 0b00000000'00000000'00000001'00000000'00000000);
        hybrid_assert(dirty_bit(16, 0) == 0b00000000'00000000'00000000'00000000'00000000);
        hybrid_assert(dirty_bit(32)    == 0b00000001'00000000'00000000'00000000'00000000);
    }

    //get_bit should get a single bit at the specified index (from lowest order bits)
    test test_get_bit() 
    {
        hybrid_assert(get_bit(0b00000001, 0) == 1);
        hybrid_assert(get_bit(0b11111101, 1) == 0);

        hybrid_assert(get_bit(0b10101010, 0) == 0);
        hybrid_assert(get_bit(0b10101010, 2) == 0);
        hybrid_assert(get_bit(0b10101010, 4) == 0);
        hybrid_assert(get_bit(0b10101010, 6) == 0);

        hybrid_assert(get_bit(0b10101010, 1) == 1);
        hybrid_assert(get_bit(0b10101010, 3) == 1);
        hybrid_assert(get_bit(0b10101010, 5) == 1);
        hybrid_assert(get_bit(0b10101010, 7) == 1);

        constexpr u64 val = 0b10101111'11111111'11111111'11111111'11111111ULL;

        hybrid_assert(get_bit(val, 32) == 1);
        hybrid_assert(get_bit(val, 33) == 1);
        hybrid_assert(get_bit(val, 36) == 0);
        hybrid_assert(get_bit(val, 37) == 1);
    }

    //has_bit should check if single bit at the specified index is set (does not normalize output)
    test test_has_bit() 
    {
        hybrid_assert(has_bit(0b00000001, 0));
        hybrid_assert(!has_bit(0b11111101, 1));

        hybrid_assert(has_bit(0b1010101000000000, 9));
        hybrid_assert(has_bit(0b1010101000000000, 11));
        hybrid_assert(has_bit(0b1010101000000000, 13));
        hybrid_assert(has_bit(0b1010101000000000, 15));

        hybrid_assert(!has_bit(0b1010101000000000, 8));
        hybrid_assert(!has_bit(0b1010101000000000, 10));
        hybrid_assert(!has_bit(0b1010101000000000, 12));
        hybrid_assert(!has_bit(0b1010101000000000, 14));

        constexpr u64 val = 0b10101111'11111111'11111111'11111111'11111111ULL;
        hybrid_assert( has_bit(val, 32));
        hybrid_assert( has_bit(val, 33));
        hybrid_assert(!has_bit(val, 36));
        hybrid_assert( has_bit(val, 37));
    }

    //set_bit shoudl set a single bit at the specified index (from lowest order bits)
    test test_set_bit() 
    {
        hybrid_assert(set_bit(0b00000001, 0, 0) == 0b00000000);
        hybrid_assert(set_bit(0b00000001, 3, 0) == 0b00000001);
        hybrid_assert(set_bit(0b00000001, 3, 1) == 0b00001001);
        hybrid_assert(set_bit(0b1010101000000000, 9, 1) == 0b1010101000000000);
        hybrid_assert(set_bit(0b1010101000000000, 11, 1) == 0b1010101000000000);
        hybrid_assert(set_bit(0b1010101000000000, 9, 0) == 0b1010100000000000);
        hybrid_assert(set_bit(0b1010101000000000, 11, 0) == 0b1010001000000000);
        hybrid_assert(set_bit(0b1010101000000000, 5, 11)   == 0b1010101000100000);
        hybrid_assert(set_bit(0b1010101000000000, 5, true) == 0b1010101000100000);
        hybrid_assert(set_bit(0b1010101000000000, 5, 1651) == 0b1010101000100000);
        hybrid_assert(set_bit(0b1010101000000000, 5, 1651) == 0b1010101000100000);

        hybrid_assert(set_bit(0b01ULL, 16, 1)    == 0b00000000'00000000'00000001'00000000'00000001);
        hybrid_assert(set_bit(0b10ULL, 16, 0)    == 0b00000000'00000000'00000000'00000000'00000010);
        hybrid_assert(set_bit(0b11ULL, 32, 1)    == 0b00000001'00000000'00000000'00000000'00000011);
    }

    //toggle_bit shoudl toggle (flip) a single bit at the specified index (from lowest order bits)
    test test_toggle_bit() 
    {
        hybrid_assert(toggle_bit(0b00000001, 0) == 0b00000000);
        hybrid_assert(toggle_bit(0b00000001, 3) == 0b00001001);
        hybrid_assert(toggle_bit(0b00001001, 3) == 0b00000001);
        hybrid_assert(toggle_bit(0b10101010'00000000, 9) == 0b1010100000000000);
        hybrid_assert(toggle_bit(0b10101010'00000000, 11) == 0b1010001000000000);

        hybrid_assert(toggle_bit(0b10101010'00000000, 5)   == 0b1010101000100000);

        hybrid_assert(toggle_bit(0b01ULL, 16)    == 0b00000000'00000000'00000001'00000000'00000001);
        hybrid_assert(toggle_bit(0b10ULL, 16)    == 0b00000000'00000000'00000001'00000000'00000010);
        hybrid_assert(toggle_bit(0b11ULL, 32)    == 0b00000001'00000000'00000000'00000000'00000011);
        hybrid_assert(toggle_bit(0b11ULL, 33)    == 0b00000010'00000000'00000000'00000000'00000011);
    }

    //bitmask_higher shoudl create a binary mask with 1s masking all bits onwards from from_bit
    test test_bitmask_higher() 
    {
        hybrid_assert(bitmask_higher<u8>(1) == 0b11111110);
        hybrid_assert(bitmask_higher<u8>(2) == 0b11111100);
        hybrid_assert(bitmask_higher<u8>(7) == 0b10000000);
        hybrid_assert(bitmask_higher<u8>(8) == 0b00000000);

        hybrid_assert(bitmask_higher<u64>(8) == 0xFFFFFFFFFFFFFF00);
        hybrid_assert(bitmask_higher<u64>(32) == 0xFFFFFFFF00000000);
    }

    //bitmask_lower shoudl create a binary mask with 1s masking all bits untilto to_bit
    test test_bitmask_lower() 
    {
        hybrid_assert(bitmask_lower<u8>(1) == 0b00000001);

        hybrid_assert(bitmask_lower<u8>(2) == 0b00000011);
        hybrid_assert(bitmask_lower<u8>(7) == 0b01111111);
        hybrid_assert(bitmask_lower<u8>(8) == 0b11111111);

        hybrid_assert(bitmask_lower<u64>(8) == 0x00000000000000FF);
        hybrid_assert(bitmask_lower<u64>(32) == 0x00000000FFFFFFFF);
    }

    //bitmask_range shoudl create a binary mask with 1s masking all bits in the given range
    test test_bitmask_range() 
    {
        hybrid_assert(bitmask_range<u8>(1, 2) == 0b00000010);
        hybrid_assert(bitmask_range<u8>(1, 4) == 0b00001110);
        hybrid_assert(bitmask_range<u8>(5, 5) == 0b00000000);
        hybrid_assert(bitmask_range<u8>(2, 2) == 0b00000000);
        hybrid_assert(bitmask_range<u8>(1, 1) == 0b00000000);
        hybrid_assert(bitmask_range<u8>(8, 8) == 0b00000000);
        hybrid_assert(bitmask_range<u8>(0, 8) == 0b11111111);
        hybrid_assert(bitmask_range<u8>(3, 6) == 0b00111000);

        hybrid_assert(bitmask_range<u64>(32, 48) == 0x0000FFFF00000000);
        hybrid_assert(bitmask_range<u64>(32, 49) == 0x0001FFFF00000000);

        //If the ranges is invalid the mask should be empty
        hybrid_assert(bitmask_range<u8>(6, 3) == 0b00000000);
        hybrid_assert(bitmask_range<u8>(5, 4) == 0b00000000);
        hybrid_assert(bitmask_range<u8>(5, 0) == 0b00000000);
        hybrid_assert(bitmask_range<u8>(8, 7) == 0b00000000);
    }
    

    //bitmask shoudl create a binary mask with 1s masking all bits in the given range
    test test_bitmask() 
    {
        hybrid_assert(bitmask<u8>(1, 1) == 0b00000010);
        hybrid_assert(bitmask<u8>(1, 3) == 0b00001110);
        hybrid_assert(bitmask<u8>(5, 0) == 0b00000000);
        hybrid_assert(bitmask<u8>(2, 0) == 0b00000000);
        hybrid_assert(bitmask<u8>(1, 0) == 0b00000000);
        hybrid_assert(bitmask<u8>(8, 0) == 0b00000000);
        hybrid_assert(bitmask<u8>(0, 8) == 0b11111111);
        hybrid_assert(bitmask<u8>(3, 3) == 0b00111000);
        hybrid_assert(bitmask<u64>(32, 16) == 0x0000FFFF00000000);
        hybrid_assert(bitmask<u64>(32, 17) == 0x0001FFFF00000000);
    }
    

    //copy_bytes should copy raw bytes and are_bytes_equal should determine if the bytes are equal
    test test_copy_bytes()
    {
        //Bytes should be copied
        {
            byte input[] = {1, 2, 3, 4, 5, 6};
            byte expected[] = {1, 2, 3, 4, 5, 6};
            byte output[100] = {255, 254, 253};

            copy_bytes(output, input, sizeof(expected));
            runtime_assert(equal_n(output, expected, sizeof(expected)));
            runtime_assert(are_bytes_equal(output, expected, sizeof(expected)));
        }

        //Empty shoudlnt change the data
        {
            byte input[] = {1};
            byte expected[] = {255, 254, 253};
            byte output[100] = {255, 254, 253};

            copy_bytes(output, input, 0);
            runtime_assert(equal_n(expected, output, 3));
            runtime_assert(are_bytes_equal(output, expected, sizeof(expected)));
        }

        //Any data should be copied
        {
            //double arr
            {
                double d_output[100] = {0};
                double input[] = {1, 2, 3, 4, 5, 6, -454.5464, 54444, 4};
                double expected[] = {1, 2, 3, 4, 5, 6, -454.5464, 54444, 4};
                byte output[100] = {255, 254, 253};

                copy_bytes(cast(void*) output, cast(const void*) input, sizeof(expected));
                copy_bytes(d_output, input, sizeof(expected));
                runtime_assert(equal_n(d_output, expected, 9));
                runtime_assert(are_bytes_equal(output, expected, sizeof(expected)));
                runtime_assert(are_bytes_equal(d_output, expected, sizeof(expected)));
            }

            //TestStruct1 data
            {
                TestStruct1 input;
                TestStruct1 output2 = {0};
                TestStruct1 expected;
                byte output[100] = {255, 254, 253};

                runtime_assert(expected != output2);

                copy_bytes(&output2, &input, sizeof(expected));
                copy_bytes(output, &input, sizeof(expected));

                runtime_assert(output2 == expected);
                runtime_assert(are_bytes_equal(&output2, &expected, sizeof(expected)));
                runtime_assert(are_bytes_equal(output, &expected, sizeof(expected)));
            }
        }

        //Should work in consteval context"
        {
            constexpr let context1 = []{
                byte output[100] = {0};
                byte input[100] = {1, 2, 3, 4, 5, 6};
                byte expected[100] = {1, 2, 3, 4, 5, 6};
                copy_bytes(output, input, 6);

                return equal_n(expected, output, 6)
                    && are_bytes_equal(output, expected, sizeof(expected));
            };

            constexpr let context2 = []{
                byte output[100] = {0};
                TestStruct1 input;
                TestStruct1 output2 = {0};
                TestStruct1 expected;

                copy_bytes(&output2, &input, sizeof(expected));
                copy_bytes(output, &input, sizeof(expected));

                return output2 == expected
                    && are_bytes_equal(&output2, &expected, sizeof(expected))
                    && are_bytes_equal(output, &expected, sizeof(expected));
            };

            hybrid_assert(context1());
            hybrid_assert(context2());
        }
    }

    //make tester


    //byteswap shoudl swap bytes in the given type/array of bytes
    test test_byteswap()
    {

        //array overload
        { 
            byte unchanged[8] = {1, 2, 3, 4, 5, 6, 7, 8};
            byte input[8] = {1, 2, 3, 4, 5, 6, 7, 8};
            byte expected[8] = {8, 7, 6, 5, 4, 3, 2, 1};
            byte output[8] = {1, 2, 3, 4, 5, 6, 7, 8};
            
            //1
            {
                byteswap(output, input, 8);
                byteswap(input, 8);

                runtime_assert(equal_n(input, expected, 8));
                runtime_assert(equal_n(output, expected, 8));

                byteswap(output, input, 8);
                byteswap(input, 8);

                runtime_assert(equal_n(input, unchanged, 8));
                runtime_assert(equal_n(output, unchanged, 8));
            }

            //Swapping odd number of bytes should work
            {
                byteswap(input, 7);
                runtime_assert(equal_n(input, expected + 1, 7));
                byteswap(input, 7);
                runtime_assert(equal_n(input, unchanged, 7));

                byteswap(input, 5);
                runtime_assert(equal_n(input, expected + 3, 5));
                byteswap(input, 5);
                runtime_assert(equal_n(input, unchanged, 5));
            }

            //Swapping a single or zero bytes shouldnt change the value
            {
                byteswap(output, input, 1);
                byteswap(input, 1);
                runtime_assert(equal_n(input, unchanged, 8));
                runtime_assert(equal_n(output, unchanged, 8));

                byteswap(output, input, 0);
                byteswap(input, 0);
                runtime_assert(equal_n(input, unchanged, 8));
                runtime_assert(equal_n(output, unchanged, 8));
            }
        }

        //value overload
        {
            hybrid_assert(byteswap<u8>(123) == 123); 
            hybrid_assert(byteswap<u8>(64) == 64); 
            hybrid_assert(byteswap<u8>(0) == 0); 

            hybrid_assert(byteswap<u16>(0xBCFE) == 0xFEBC); 
            hybrid_assert(byteswap<u16>(0x18F0) == 0xF018); 
            hybrid_assert(byteswap<u16>(0x0000) == 0x0000);
            
            hybrid_assert(byteswap<u32>(0x11223344) == 0x44332211); 
            hybrid_assert(byteswap<u64>(0x1122334455667788) == 0x8877665544332211); 
        }
    }

    test test_bits()
    {
        test_get_byte();
        test_set_byte();
        test_bit();
        test_dirty_bit();
        test_get_bit();
        test_has_bit();
        test_set_bit();
        test_toggle_bit();
        test_bitmask_higher();
        test_bitmask_lower();
        test_bitmask_range();
        test_bitmask();
        test_copy_bytes();
        test_byteswap();
    }

    run_test(test_bits);
}


#include "../lib/jot/undefs.h"