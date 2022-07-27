#pragma once

#include "tester_utils.h"
#include "../lib/jot/endian.h"
#include "../lib/jot/defines.h"

namespace jot::tests 
{
    //lower_bytes_offset should return an offset into the byte field pointing to the bytes representing the lowest values
    test test_lower_bytes_offset()
    {

    }

    //higher_bytes_offset should return an offset into the byte field pointing to the bytes representing the biggest values
    test test_higher_bytes_offset()
    {

    }

    //get_relative_endian_placement should call the matching lambda with the appropriate lower_bytes_offset
    test test_place_endian()
    {

    }

    template<typename Out, size_t size>
    func from_endian_tester(const byte (&in)[size], let target_endian, let env_endian)
    {
        return from_endian<Out>(in, size, target_endian, env_endian);
    }

    //from_endian should convert given bytes withe the source and target endian and save the resulting number to integer
    test test_from_endian()
    {
        //Single byte should be converted to its value regardless of endians used
        {
            hybrid_assert(from_endian_tester<u8>({0xAB}, Endian::Little, Endian::Big) == 0xAB);
            hybrid_assert(from_endian_tester<u8>({0xAB}, Endian::Little, Endian::Little) == 0xAB);
            hybrid_assert(from_endian_tester<u8>({0xAB}, Endian::Big, Endian::Little) == 0xAB);
            hybrid_assert(from_endian_tester<u8>({0xAB}, Endian::Big, Endian::Big) == 0xAB);
        }

        //Single byte should be converted to its value and shifted depending on the endian
        {
            hybrid_assert(from_endian_tester<u8 >({0xAB}, Endian::Little, Endian::Little) == 0xAB);
            hybrid_assert(from_endian_tester<u16>({0xAB}, Endian::Little, Endian::Little) == 0xAB);
            hybrid_assert(from_endian_tester<u32>({0xAB}, Endian::Little, Endian::Little) == 0xAB);
            hybrid_assert(from_endian_tester<u64>({0xAB}, Endian::Little, Endian::Little) == 0xAB);

            hybrid_assert(from_endian_tester<u8 >({0xAB}, Endian::Big, Endian::Little) == 0xAB);
            hybrid_assert(from_endian_tester<u16>({0xAB}, Endian::Big, Endian::Little) == 0xAB);
            hybrid_assert(from_endian_tester<u32>({0xAB}, Endian::Big, Endian::Little) == 0xAB);
            hybrid_assert(from_endian_tester<u64>({0xAB}, Endian::Big, Endian::Little) == 0xAB);

            hybrid_assert(from_endian_tester<u8 >({0xAB}, Endian::Little, Endian::Big) == 0xAB);
            hybrid_assert(from_endian_tester<u16>({0xAB}, Endian::Little, Endian::Big) == 0xAB00);
            hybrid_assert(from_endian_tester<u32>({0xAB}, Endian::Little, Endian::Big) == 0xAB000000);
            hybrid_assert(from_endian_tester<u64>({0xAB}, Endian::Little, Endian::Big) == 0xAB00000000000000);

            hybrid_assert(from_endian_tester<u8 >({0xAB}, Endian::Big, Endian::Big) == 0xAB);
            hybrid_assert(from_endian_tester<u16>({0xAB}, Endian::Big, Endian::Big) == 0xAB00);
            hybrid_assert(from_endian_tester<u32>({0xAB}, Endian::Big, Endian::Big) == 0xAB000000);
            hybrid_assert(from_endian_tester<u64>({0xAB}, Endian::Big, Endian::Big) == 0xAB00000000000000);
        }

        //Two bytes should get switched if on opposite endians but remain the same on same endians
        {
            constexpr let in = to_bytes<u16>(0xCDAB);

            hybrid_assert(from_endian_tester<u16>(in.data, Endian::Little, Endian::Little) == 0xCDAB);
            hybrid_assert(from_endian_tester<u16>(in.data, Endian::Big, Endian::Big) == 0xCDAB);
            hybrid_assert(from_endian_tester<u16>(in.data, Endian::Little, Endian::Big) == 0xABCD);
            hybrid_assert(from_endian_tester<u16>(in.data, Endian::Big, Endian::Little) == 0xABCD);
        }

        /*
        //Two bytes should get switched and offseted based on the type used
        {
            let in = to_bytes<u16>(0xCDAB);

            from_endian(out16, in.data, in.size, Endian::Little, Endian::Little);
            from_endian(out32, in.data, in.size, Endian::Little, Endian::Little);
            from_endian(out64, in.data, in.size, Endian::Little, Endian::Little);

            REQUIRE(out16 == 0xCDAB);
            REQUIRE(out32 == 0xCDAB);
            REQUIRE(out64 == 0xCDAB);

            from_endian(out16, in.data, in.size, Endian::Big, Endian::Big);
            from_endian(out32, in.data, in.size, Endian::Big, Endian::Big);
            from_endian(out64, in.data, in.size, Endian::Big, Endian::Big);

            REQUIRE(out16 == 0xCDAB);
            REQUIRE(out32 == 0xCDAB);
            REQUIRE(out64 == 0xCDAB);

            from_endian(out16, in.data, in.size, Endian::Little, Endian::Big);
            from_endian(out32, in.data, in.size, Endian::Little, Endian::Big);
            from_endian(out64, in.data, in.size, Endian::Little, Endian::Big);

            REQUIRE(out16 == 0xABCD);
            REQUIRE(out32 == 0xABCD);
            REQUIRE(out64 == 0xABCD);

            from_endian(out16, in.data, in.size, Endian::Big, Endian::Little);
            from_endian(out32, in.data, in.size, Endian::Big, Endian::Little);
            from_endian(out64, in.data, in.size, Endian::Big, Endian::Little);

            REQUIRE(out16 == 0xABCD);
            REQUIRE(out32 == 0xABCD);
            REQUIRE(out64 == 0xABCD);
        }
        */

        /*ByteArray<3> in2 = {111, 121, 131};
        ByteArray<7> in1 = {1, 2, 3, 4, 5, 6, 7};
        ByteArray<4> in1 = {4, 3, 2, 1};*/
        //REQUIRE(true == true);
    }
    //to_endian should convert given integer withe the source and target endian and save resulting bytes to bytes
    test test_to_endian()
    {
    
    }
    
    test test_endian()
    {
        test_from_endian();
        test_to_endian();
    }

    run_test(test_endian);
}

#include "../lib/jot/undefs.h"