#pragma once
#include "tester_utils.h"
#include "../lib/jot/experimental/esoteric_fraction.h"
#include "../lib/jot/defines.h"

namespace jot::tests 
{
    static_assert(fraction_data<FractionData<u8, u8>>);
    static_assert(fraction_data<FractionData<i8, i8>>);
    static_assert(fraction_data<FractionData<u32, u32>>);
    static_assert(fraction_data<FractionData<i32, i32>>);
    static_assert(fraction_data<FractionData<i32, u32>>);
    static_assert(fraction_data<FractionData<i32, u8>>);
    static_assert(fraction_data<FractionData<i8, u64>>);
    static_assert(fraction_data<FractionData<u64, u64>>);
    static_assert(fraction_data<FractionData<i64, i64>>);
    static_assert(fraction_data<FractionData<i64, i32>>);

    template <class iFrac, class uFrac>
    test test_fraction_creation()
    {   
        {
            iFrac frac0;
            iFrac frac1 = {3195, 1192};
            
            runtime_assert(numerator(frac1) == 3195);
            runtime_assert(denominator(frac1) == 1192);

            frac0 = frac1;
            runtime_assert(numerator(frac0) == 3195);
            runtime_assert(denominator(frac0) == 1192);

            frac0 = std::move(frac1);
            runtime_assert(numerator(frac0) == 3195);
            runtime_assert(denominator(frac0) == 1192);

            runtime_assert(numerator(frac0, 1291) == 1291);
            runtime_assert(numerator(frac0) == 1291);
            runtime_assert(denominator(frac0) == 1192);

            
            runtime_assert(cast(i32) frac0 == 1);
            runtime_assert(cast(u8) frac0 == 1);
            runtime_assert(float_equal(cast(f32) frac0, 1.08305369128));
            runtime_assert(float_equal(cast(f64) frac0, 1.08305369128));

            frac0 = 1941;
            runtime_assert(numerator(frac0) == 1941);
            runtime_assert(denominator(frac0) == 1);

            frac0 = 5.151;
            runtime_assert(numerator(frac0) == 5151);
            runtime_assert(denominator(frac0) == 1000);

            frac0 = {1, 3};
            runtime_assert(numerator(frac0) == 1);
            runtime_assert(denominator(frac0) == 3);
        }
        
        {
            constexpr iFrac frac1 = 0;
            constexpr iFrac frac2 = {3, 8};
            constexpr iFrac frac3 = 5.71;
            constexpr iFrac frac4 = frac1;
            constexpr uFrac frac5 = -64.5f;

            hybrid_assert(numerator(frac1) == 0);
            hybrid_assert(denominator(frac1) == 1);

            hybrid_assert(numerator(frac2) == 3);
            hybrid_assert(denominator(frac2) == 8);

            hybrid_assert(numerator(frac3) == 571);
            hybrid_assert(denominator(frac3) == 100);

            hybrid_assert(numerator(frac4) == 0);
            hybrid_assert(denominator(frac4) == 1);

            hybrid_assert(numerator(frac5) == 0);
            hybrid_assert(denominator(frac5) == 0);
        }
    }

    template <class iFrac>
    test test_purely_signed_fraction_creation()
    {
        //@TODO
    }

    template <class iFrac, class uFrac>
    test test_fraction_specials()
    {   
        {
            constexpr uFrac frac1 = {18, 761};
            constexpr uFrac frac2 = {18, 0};
            constexpr uFrac frac3 = {0, 176};
            constexpr uFrac frac4 = {0, 0};
            constexpr uFrac frac5 = {1734, 0};
            constexpr uFrac frac6 = {1973, 176};
            constexpr iFrac frac7 = {-1973, 0};

            hybrid_assert(is_normal(frac1) == true);
            hybrid_assert(is_normal(frac2) == false);
            hybrid_assert(is_normal(frac3) == true);
            hybrid_assert(is_normal(frac4) == false);
            hybrid_assert(is_normal(frac5) == false);
            hybrid_assert(is_normal(frac6) == true);
            hybrid_assert(is_normal(frac7) == false);

            hybrid_assert(is_nan(frac1) == false);
            hybrid_assert(is_nan(frac2) == false);
            hybrid_assert(is_nan(frac3) == false);
            hybrid_assert(is_nan(frac4) == true);
            hybrid_assert(is_nan(frac5) == false);
            hybrid_assert(is_nan(frac6) == false);
            hybrid_assert(is_nan(frac7) == false);

            hybrid_assert(is_infinite(frac1) == false);
            hybrid_assert(is_infinite(frac2) == true);
            hybrid_assert(is_infinite(frac3) == false);
            hybrid_assert(is_infinite(frac4) == false);
            hybrid_assert(is_infinite(frac5) == true);
            hybrid_assert(is_infinite(frac6) == false);
            hybrid_assert(is_infinite(frac7) == true);

            hybrid_assert(is_infinity(frac1) == false);
            hybrid_assert(is_infinity(frac2) == true);
            hybrid_assert(is_infinity(frac3) == false);
            hybrid_assert(is_infinity(frac4) == false);
            hybrid_assert(is_infinity(frac5) == true);
            hybrid_assert(is_infinity(frac6) == false);
            hybrid_assert(is_infinity(frac7) == false);

            hybrid_assert(is_negative_infinity(frac1) == false);
            hybrid_assert(is_negative_infinity(frac2) == false);
            hybrid_assert(is_negative_infinity(frac3) == false);
            hybrid_assert(is_negative_infinity(frac4) == false);
            hybrid_assert(is_negative_infinity(frac5) == false);
            hybrid_assert(is_negative_infinity(frac6) == false);
            hybrid_assert(is_negative_infinity(frac7) == true);
        }

        {
            iFrac ifr1 = {2, 3};
            iFrac ifr2 = {2, 3};
            iFrac ifr3 = {2, 3};

            uFrac ufr1 = {5, 10};
            uFrac ufr2 = {5, 10};

            hybrid_assert(is_nan(nan<iFrac>()));
            hybrid_assert(is_infinity(infinity<iFrac>()));
            hybrid_assert(is_negative_infinity(negative_infinity<iFrac>()));

            nan(ifr1);
            infinity(ifr2);
            negative_infinity(ifr3);

            runtime_assert(is_nan(ifr1));
            runtime_assert(is_infinity(ifr2));
            runtime_assert(is_negative_infinity(ifr3));

            hybrid_assert(is_nan(nan<uFrac>()));
            hybrid_assert(is_infinity(infinity<uFrac>()));

            nan(ufr1);
            infinity(ufr2);

            runtime_assert(is_nan(ufr1));
            runtime_assert(is_infinity(ufr2));
        }
    }


    template <class iFrac, class uFrac>
    test test_fraction_comparison()
    {   
        hybrid_assert(uFrac{3, 5} == uFrac{3, 5});
        hybrid_assert(uFrac{3, 5} != uFrac{4, 5});
        hybrid_assert(uFrac{144, 5} != uFrac{144, 6});
        hybrid_assert(uFrac{0, 1} != uFrac{0, 0});
        hybrid_assert(uFrac{123, 321} != uFrac{0, 0});
        hybrid_assert(uFrac{123, 321} != uFrac{1, 0});
        hybrid_assert(iFrac{123, 321} != iFrac{-1, 0});
        hybrid_assert(iFrac{0, 5} == iFrac{0, 1});
        hybrid_assert(iFrac{0, 5} > iFrac{0, 1} == false);
        hybrid_assert(iFrac{0, 5} < iFrac{0, 1} == false);
        hybrid_assert(iFrac{0, 5} <= iFrac{0, 1});
        hybrid_assert(iFrac{0, 0} != iFrac{0, 1});
        hybrid_assert(iFrac{0, 0} > iFrac{0, 1} == false);
        hybrid_assert(iFrac{0, 0} < iFrac{0, 1} == false);
        hybrid_assert(iFrac{0, 0} != iFrac{89, 1});
        hybrid_assert(iFrac{0, 1} > iFrac{-1, 3});
        
        hybrid_assert(iFrac{0, 0} > iFrac{89, 1} == false);
        hybrid_assert(iFrac{0, 0} < iFrac{89, 1} == false);
        hybrid_assert(iFrac{0, 0} < iFrac{-89, 1} == false);
        hybrid_assert(iFrac{0, 0} > iFrac{-89, 1} == false);

        hybrid_assert(iFrac{0, 1} != iFrac{1, 0});
        hybrid_assert(iFrac{0, 1} < iFrac{1, 0});
        hybrid_assert(iFrac{0, 1} != iFrac{-1, 0});
        hybrid_assert(iFrac{0, 1} > iFrac{-1, 0});
        hybrid_assert(iFrac{1, 0} != iFrac{-1, 0});
        hybrid_assert(iFrac{1, 0} > iFrac{-1, 0});
        hybrid_assert(iFrac{3, 0} != iFrac{5, 0});
        hybrid_assert(iFrac{3, 0} < iFrac{5, 0});

        hybrid_assert(uFrac{3, 5} > uFrac{0, 5});
        hybrid_assert(uFrac{3, 5} > uFrac{0, 1});
        hybrid_assert(uFrac{3, 5} > uFrac{3, 3000});
        hybrid_assert(iFrac{3, 5} > iFrac{-3, 1});
        hybrid_assert(iFrac{-3, 5} > iFrac{-3, 0});
        hybrid_assert(iFrac{1741, 1} > iFrac{-1, 0});
        hybrid_assert(uFrac{1741, 1} > uFrac{0, 0} == false);

        hybrid_assert(uFrac{3, 5} < uFrac{14, 5});
        hybrid_assert(uFrac{3, 5} < uFrac{1, 1});
        hybrid_assert(uFrac{3, 5} < uFrac{1874, 1});
        hybrid_assert(uFrac{3, 5} < uFrac{1874, 0});
        hybrid_assert(uFrac{1741, 1} < uFrac{0, 0} == false);

        hybrid_assert(iFrac{0, 5} > iFrac{-3, 1});
        hybrid_assert(iFrac{0, 187} > iFrac{-198, 123});
        hybrid_assert(iFrac{187, 0} > iFrac{-187, 0});
        hybrid_assert(iFrac{187, 0} > iFrac{-187, 1});
        hybrid_assert(iFrac{-187, 0} >= iFrac{187, 0} == false);
    }
        
    template <class iFrac>
    test test_purely_signed_fraction_comparison()
    {   
        hybrid_assert(iFrac{123, 321} == iFrac{-123, -321});
        hybrid_assert(iFrac{-123, 1} == iFrac{123, -1});
        hybrid_assert(iFrac{-1, -321} != iFrac{1, -321});
        hybrid_assert(iFrac{0, -5} == iFrac{0, -1});
        hybrid_assert(iFrac{0, -5} < iFrac{0, -1} == false);
        hybrid_assert(iFrac{0, 5} == iFrac{0, -1});
        hybrid_assert(iFrac{0, 5} <= iFrac{0, -1});
        hybrid_assert(iFrac{0, 1} > iFrac{-1, 3});
        hybrid_assert(iFrac{0, 1} > iFrac{1, -3});
        hybrid_assert(iFrac{0, 1} < iFrac{1, -3} == false);
        hybrid_assert(iFrac{0, 5} > iFrac{0, -1} == false);
        hybrid_assert(iFrac{0, 0} != iFrac{0, -1});
        hybrid_assert(iFrac{0, 0} < iFrac{0, -1} == false);
        hybrid_assert(iFrac{0, 0} > iFrac{0, -1} == false);
        hybrid_assert(iFrac{0, -1} != iFrac{0, 0});
        hybrid_assert(iFrac{0, -1} > iFrac{0, 0} == false);
        hybrid_assert(iFrac{0, -1} < iFrac{0, 0} == false);
        hybrid_assert(iFrac{0, 0} != iFrac{89, -1});
        hybrid_assert(iFrac{0, 0} > iFrac{89, -1} == false);
        hybrid_assert(iFrac{0, 0} < iFrac{89, -1} == false);
        hybrid_assert(iFrac{0, -1} != iFrac{-1, 0});
        hybrid_assert(iFrac{0, -1} != iFrac{1, 0});
        hybrid_assert(iFrac{1, 0} != iFrac{-1, 0});
        hybrid_assert(iFrac{3, 0} != iFrac{5, 0});
        hybrid_assert(iFrac{3, 0} < iFrac{5, 0});

        hybrid_assert(iFrac{0, 321} == iFrac{0, -321});
        hybrid_assert(iFrac{1, 0} != iFrac{-1, 0});
        hybrid_assert(iFrac{-123, -321} != iFrac{123, -321});

        hybrid_assert(iFrac{-5, -4} > iFrac{5, -4});
        hybrid_assert(iFrac{-5, -4} < iFrac{5, -4} == false);
        hybrid_assert(iFrac{-51, 131} > iFrac{51, -131} == false);
        hybrid_assert(iFrac{-51, 131} < iFrac{51, -131} == false);
        hybrid_assert(iFrac{-51, 131} >= iFrac{51, -131});
        hybrid_assert(iFrac{-51, 131} <= iFrac{51, -131});

        hybrid_assert(iFrac{-5, -2} > iFrac{5, -81});
        hybrid_assert(iFrac{-5, -2} >= iFrac{5, -81});
        hybrid_assert(iFrac{-5, -2} < iFrac{5, -81} == false);
        hybrid_assert(iFrac{0, 2} > iFrac{5, -81});
        hybrid_assert(iFrac{0, -2} > iFrac{5, -81});
        hybrid_assert(iFrac{0, -2} > iFrac{-5, 81});
        hybrid_assert(iFrac{0, -2} < iFrac{-5, -81});

    }

    template <class iFrac, class uFrac>
    test test_fraction_aritmetic()
    {
        //On normal values
        {
            hybrid_assert(uFrac{0, 1} + uFrac{0, 1} == uFrac{0, 1});
            hybrid_assert(uFrac{1, 1} + uFrac{1, 1} == uFrac{2, 1});
            hybrid_assert(uFrac{3, 5} + uFrac{3, 5} == uFrac{30, 25});
            hybrid_assert(uFrac{3, 3} + uFrac{3, 7} == uFrac{30, 21});
            hybrid_assert(uFrac{4, 3} + uFrac{0, 7} == uFrac{28, 21});
            hybrid_assert(uFrac{0, 3} + uFrac{0, 7} == uFrac{0, 21});

            hybrid_assert(uFrac{0, 1} - uFrac{0, 1} == uFrac{0, 1});
            hybrid_assert(uFrac{1, 1} - uFrac{1, 1} == uFrac{0, 1});
            hybrid_assert(uFrac{4, 5} - uFrac{3, 5} == uFrac{5, 25});
            hybrid_assert(uFrac{3, 3} - uFrac{3, 7} == uFrac{12, 21});
            hybrid_assert(uFrac{4, 3} - uFrac{0, 7} == uFrac{28, 21});
            hybrid_assert(uFrac{0, 3} - uFrac{0, 7} == uFrac{0, 21});
            
            hybrid_assert(uFrac{0, 1} * uFrac{0, 1} == uFrac{0, 1});
            hybrid_assert(uFrac{1, 1} * uFrac{1, 1} == uFrac{1, 1});
            hybrid_assert(uFrac{4, 5} * uFrac{3, 5} == uFrac{12, 25});
            hybrid_assert(uFrac{3, 3} * uFrac{3, 7} == uFrac{9, 21});
            hybrid_assert(uFrac{4, 3} * uFrac{0, 7} == uFrac{0, 21});
            hybrid_assert(uFrac{0, 3} * uFrac{0, 7} == uFrac{0, 21});
            hybrid_assert(uFrac{0, 7} * uFrac{0, 3} == uFrac{0, 21});

            hybrid_assert(uFrac{0, 1} / uFrac{1, 1} == uFrac{0, 1});
            hybrid_assert(uFrac{1, 1} / uFrac{1, 1} == uFrac{1, 1});
            hybrid_assert(uFrac{4, 5} / uFrac{3, 5} == uFrac{20, 15});
            hybrid_assert(uFrac{3, 3} / uFrac{3, 7} == uFrac{21, 9});

            //signed
            hybrid_assert(iFrac{0, 1} + iFrac{0, 1} == iFrac{0, 1});
            hybrid_assert(iFrac{1, 1} + iFrac{-1, 1} == iFrac{0, 1});
            hybrid_assert(iFrac{3, 5} + iFrac{-3, 5} == iFrac{0, 25});
            hybrid_assert(iFrac{-3, 3} + iFrac{3, 7} == iFrac{-12, 21});

            hybrid_assert(iFrac{0, 1} - iFrac{0, 1} == iFrac{0, 1});
            hybrid_assert(iFrac{1, 1} - iFrac{-1, 1} == iFrac{2, 1});
            hybrid_assert(iFrac{-4, 5} - iFrac{3, 5} == iFrac{-35, 25});
            hybrid_assert(iFrac{3, 3} - iFrac{-3, 7} == iFrac{30, 21});
            hybrid_assert(iFrac{-4, 3} - iFrac{0, 7} == iFrac{-28, 21});
            hybrid_assert(iFrac{0, 3} - iFrac{0, 7} == iFrac{0, 21});

            hybrid_assert(iFrac{0, 1} * iFrac{0, 1} == iFrac{0, 1});
            hybrid_assert(iFrac{1, 1} * iFrac{-1, 1} == iFrac{-1, 1});
            hybrid_assert(iFrac{-4, 5} * iFrac{3, 5} == iFrac{-12, 25});
            hybrid_assert(iFrac{-3, 3} * iFrac{3, 7} == iFrac{-9, 21});
            hybrid_assert(iFrac{-4, 3} * iFrac{0, 7} == iFrac{0, 21});
            hybrid_assert(iFrac{0, 3} * iFrac{0, 7} == iFrac{0, 21});
            hybrid_assert(iFrac{0, 7} * iFrac{0, 3} == iFrac{0, 21});

            hybrid_assert(iFrac{0, 1} / iFrac{-1, 1} == iFrac{0, 1});
            hybrid_assert(iFrac{-1, 1} / iFrac{1, 1} == iFrac{-1, 1});
            hybrid_assert(iFrac{-3, 3} / iFrac{3, 7} == iFrac{-21, 9});
        }

        //any operation on nan should return nan
        {
            hybrid_assert(uFrac{4, 3} + uFrac{0, 0} == uFrac{0, 0});
            hybrid_assert(uFrac{0, 0} + uFrac{4684, 4} == uFrac{0, 0});
            hybrid_assert(uFrac{0, 0} + uFrac{0, 7} == uFrac{0, 0});
            hybrid_assert(uFrac{0, 0} + uFrac{7, 0} == uFrac{0, 0});

            hybrid_assert(uFrac{4, 3} - uFrac{0, 0} == uFrac{0, 0});
            hybrid_assert(uFrac{0, 0} - uFrac{4684, 4} == uFrac{0, 0});
            hybrid_assert(uFrac{0, 0} - uFrac{0, 7} == uFrac{0, 0});
            hybrid_assert(uFrac{0, 0} - uFrac{7, 0} == uFrac{0, 0});

            hybrid_assert(uFrac{4, 3} * uFrac{0, 0} == uFrac{0, 0});
            hybrid_assert(uFrac{0, 0} * uFrac{4684, 4} == uFrac{0, 0});
            hybrid_assert(uFrac{0, 0} * uFrac{0, 7} == uFrac{0, 0});
            hybrid_assert(uFrac{0, 0} * uFrac{7, 0} == uFrac{0, 0});

            hybrid_assert(uFrac{4, 3} / uFrac{0, 0} == uFrac{0, 0});
            hybrid_assert(uFrac{0, 0} / uFrac{4684, 4} == uFrac{0, 0});
            hybrid_assert(uFrac{0, 0} / uFrac{0, 7} == uFrac{0, 0});
            hybrid_assert(uFrac{0, 0} / uFrac{7, 0} == uFrac{0, 0});

            //Signed
            hybrid_assert(iFrac{-4, 3} + iFrac{0, 0} == iFrac{0, 0});
            hybrid_assert(iFrac{0, 0} + iFrac{-4684, 4} == iFrac{0, 0});
            hybrid_assert(iFrac{0, 0} + iFrac{0, 7} == iFrac{0, 0});
            hybrid_assert(iFrac{0, 0} + iFrac{-7, 0} == iFrac{0, 0});

            hybrid_assert(iFrac{-4, 3} - iFrac{0, 0} == iFrac{0, 0});
            hybrid_assert(iFrac{0, 0} - iFrac{-4684, 4} == iFrac{0, 0});
            hybrid_assert(iFrac{0, 0} - iFrac{0, 7} == iFrac{0, 0});
            hybrid_assert(iFrac{0, 0} - iFrac{-7, 0} == iFrac{0, 0});

            hybrid_assert(iFrac{-4, 3} * iFrac{0, 0} == iFrac{0, 0});
            hybrid_assert(iFrac{0, 0} * iFrac{-4684, 4} == iFrac{0, 0});
            hybrid_assert(iFrac{0, 0} * iFrac{0, 7} == iFrac{0, 0});
            hybrid_assert(iFrac{0, 0} * iFrac{-7, 0} == iFrac{0, 0});

            hybrid_assert(iFrac{-4, 3} / iFrac{0, 0} == iFrac{0, 0});
            hybrid_assert(iFrac{0, 0} / iFrac{-4684, 4} == iFrac{0, 0});
            hybrid_assert(iFrac{0, 0} / iFrac{0, 7} == iFrac{0, 0});
            hybrid_assert(iFrac{0, 0} / iFrac{-7, 0} == iFrac{0, 0});
        }

        //On infinities
        {
            // +
            hybrid_assert(iFrac{8, 0} + iFrac{-4684, 4} == iFrac{32, 0});
            hybrid_assert(iFrac{0, 0} + iFrac{-7, 0} == iFrac{0, 0});
            hybrid_assert(iFrac{-7, 0} + iFrac{0, 0} == iFrac{0, 0});
            hybrid_assert(iFrac{7, 0} + iFrac{-7, 0} == iFrac{0, 0});
            hybrid_assert(iFrac{87, 0} + iFrac{-7, 0} == iFrac{80, 0});

            hybrid_assert(iFrac{40, 0} + iFrac{1, 3} == iFrac{120, 0});
            hybrid_assert(iFrac{40, 0} + iFrac{-1, 3} == iFrac{120, 0});

            hybrid_assert(iFrac{1, 3} + iFrac{40, 0} == iFrac{120, 0});
            hybrid_assert(iFrac{-1, 3} + iFrac{40, 0} == iFrac{120, 0});

            // -
            hybrid_assert(iFrac{4, 0} - iFrac{0, 1} == iFrac{4, 0});
            hybrid_assert(iFrac{0, 0} - iFrac{-7, 0} == iFrac{0, 0});
            hybrid_assert(iFrac{-7, 0} - iFrac{0, 0} == iFrac{0, 0});
            hybrid_assert(iFrac{7, 0} - iFrac{-7, 0} == iFrac{14, 0});
            hybrid_assert(iFrac{7, 0} - iFrac{0, 0} == iFrac{0, 0});
            hybrid_assert(iFrac{87, 0} - iFrac{-7, 0} == iFrac{94, 0});

            hybrid_assert(iFrac{40, 0} - iFrac{1, 3} == iFrac{120, 0});
            hybrid_assert(iFrac{40, 0} - iFrac{-1, 3} == iFrac{120, 0});

            hybrid_assert(iFrac{1, 3} - iFrac{40, 0} == iFrac{-120, 0});
            hybrid_assert(iFrac{-1, 3} - iFrac{-40, 0} == iFrac{120, 0});

            // *
            hybrid_assert(iFrac{4, 0} * iFrac{0, 1} == iFrac{0, 0});
            hybrid_assert(iFrac{-4, 0} * iFrac{0, 1} == iFrac{0, 0});
            hybrid_assert(iFrac{0, 0} * iFrac{-7, 0} == iFrac{0, 0});
            hybrid_assert(iFrac{-7, 0} * iFrac{0, 0} == iFrac{0, 0});
            hybrid_assert(iFrac{7, 0} * iFrac{-7, 0} == iFrac{-49, 0});
            hybrid_assert(iFrac{7, 0} * iFrac{0, 0} == iFrac{0, 0});
            hybrid_assert(iFrac{2, 0} * iFrac{-7, 0} == iFrac{-14, 0});

            // /
            hybrid_assert(iFrac{4, 4} / iFrac{0, 2} == iFrac{8, 0});
            hybrid_assert(iFrac{-4, 4} / iFrac{0, 2} == iFrac{-8, 0});

            hybrid_assert(iFrac{0, 2} / iFrac{4, 4} == iFrac{0, 8});
            hybrid_assert(iFrac{0, 2} / iFrac{-4, 4} == iFrac{0, 8});

            hybrid_assert(iFrac{4, 0} / iFrac{0, 2} == iFrac{8, 0});
            hybrid_assert(iFrac{-4, 0} / iFrac{0, 2} == iFrac{-8, 0});

            hybrid_assert(iFrac{4, 4} / iFrac{2, 0} == iFrac{0, 8});
            hybrid_assert(iFrac{4, 4} / iFrac{-2, 0} == iFrac{0, 8});
            hybrid_assert(iFrac{4, 4} / iFrac{2, 0} == iFrac{0, 8});
            hybrid_assert(iFrac{-4, 4} / iFrac{-2, 0} == iFrac{0, 8});
        
        }

        iFrac ifr1 = {2, 3};
        iFrac ifr2 = {2, 3};

        uFrac ufr1 = {5, 10};
        uFrac ufr2 = {5, 10};
    }

    template <class iFrac>
    test test_purely_signed_fraction_aritmetic()
    {
        {
            hybrid_assert(iFrac{0, 1} + iFrac{0, 1} == iFrac{0, 1});
            hybrid_assert(iFrac{0, 1} + iFrac{0, -1} == iFrac{0, -1});
            hybrid_assert(iFrac{1, 1} + iFrac{-1, 1} == iFrac{0, 1});
            hybrid_assert(iFrac{3, 5} + iFrac{3, -5} == iFrac{0, -25});
            hybrid_assert(iFrac{3, 3} + iFrac{3, 7} == iFrac{30, 21});
            hybrid_assert(iFrac{4, -3} + iFrac{0, -7} == iFrac{-28, 21});
            hybrid_assert(iFrac{0, -3} + iFrac{0, -7} == iFrac{0, 21});

            hybrid_assert(iFrac{0, 1} - iFrac{0, -1} == iFrac{0, -1});
            hybrid_assert(iFrac{1, 1} - iFrac{-1, -1} == iFrac{0, -1});
            hybrid_assert(iFrac{-4, 5} - iFrac{3, -5} == iFrac{5, -25});
            hybrid_assert(iFrac{3, 3} - iFrac{3, -7} == iFrac{-30, -21});
            hybrid_assert(iFrac{-4, 3} - iFrac{0, -7} == iFrac{28, -21});
            hybrid_assert(iFrac{0, -3} - iFrac{0, 7} == iFrac{0, -21});

            hybrid_assert(iFrac{0, 1} * iFrac{0, -1} == iFrac{0, -1});
            hybrid_assert(iFrac{1, 1} * iFrac{-1, 1} == iFrac{-1, 1});
            hybrid_assert(iFrac{-4, 5} * iFrac{3, 5} == iFrac{-12, 25});
            hybrid_assert(iFrac{-3, 3} * iFrac{3, 7} == iFrac{-9, 21});
            hybrid_assert(iFrac{-4, 3} * iFrac{0, 7} == iFrac{0, 21});
            hybrid_assert(iFrac{0, 3} * iFrac{0, 7} == iFrac{0, 21});
            hybrid_assert(iFrac{0, 7} * iFrac{0, 3} == iFrac{0, 21});

            hybrid_assert(iFrac{0, 1} / iFrac{-1, 1} == iFrac{0, 1});
            hybrid_assert(iFrac{-1, 1} / iFrac{1, 1} == iFrac{-1, 1});
            hybrid_assert(iFrac{-3, 3} / iFrac{3, 7} == iFrac{-21, 9});
            hybrid_assert(iFrac{-3, 3} / iFrac{-3, 7} == iFrac{-21, -9});
        }

        //any operation on nan should return nan
        {
            hybrid_assert(iFrac{-4, -3} + iFrac{0, 0} == iFrac{0, 0});
            hybrid_assert(iFrac{0, 0} + iFrac{-4684, 4} == iFrac{0, 0});
            hybrid_assert(iFrac{0, 0} + iFrac{0, -7} == iFrac{0, 0});
            hybrid_assert(iFrac{0, 0} + iFrac{-7, 0} == iFrac{0, 0});

            hybrid_assert(iFrac{-4, -3} - iFrac{0, 0} == iFrac{0, 0});
            hybrid_assert(iFrac{0, 0} - iFrac{-4684, 4} == iFrac{0, 0});
            hybrid_assert(iFrac{0, 0} - iFrac{0, -7} == iFrac{0, 0});
            hybrid_assert(iFrac{0, 0} - iFrac{-7, 0} == iFrac{0, 0});

            hybrid_assert(iFrac{-4, -3} * iFrac{0, 0} == iFrac{0, 0});
            hybrid_assert(iFrac{0, 0} * iFrac{-4684, 4} == iFrac{0, 0});
            hybrid_assert(iFrac{0, 0} * iFrac{0, -7} == iFrac{0, 0});
            hybrid_assert(iFrac{0, 0} * iFrac{-7, 0} == iFrac{0, 0});

            hybrid_assert(iFrac{-4, -3} / iFrac{0, 0} == iFrac{0, 0});
            hybrid_assert(iFrac{0, 0} / iFrac{-4684, 4} == iFrac{0, 0});
            hybrid_assert(iFrac{0, 0} / iFrac{0, -7} == iFrac{0, 0});
            hybrid_assert(iFrac{0, 0} / iFrac{-7, 0} == iFrac{0, 0});

            hybrid_assert(iFrac{-4, -3} / iFrac{0, 0} == iFrac{0, 0});
            hybrid_assert(iFrac{4, 0} / iFrac{0, 0} == iFrac{0, 0});
            hybrid_assert(iFrac{0, 4} / iFrac{0, 0} == iFrac{0, 0});
            hybrid_assert(iFrac{0, -4} / iFrac{0, 0} == iFrac{0, 0});
        }


        {
            // +
            hybrid_assert(iFrac{4, 0} + iFrac{0, -1} == iFrac{4, 0});
            hybrid_assert(iFrac{8, 0} + iFrac{-4684, -4} == iFrac{32, 0});
            hybrid_assert(iFrac{0, 0} + iFrac{-7, 0} == iFrac{0, 0});
            hybrid_assert(iFrac{-7, 0} + iFrac{0, 0} == iFrac{0, 0});
            hybrid_assert(iFrac{7, 0} + iFrac{-7, 0} == iFrac{0, 0});
            hybrid_assert(iFrac{87, 0} + iFrac{-7, 0} == iFrac{80, 0});

            hybrid_assert(iFrac{40, 0} + iFrac{1, -3} == iFrac{120, 0});
            hybrid_assert(iFrac{40, 0} + iFrac{1, 3} == iFrac{120, 0});
            hybrid_assert(iFrac{40, 0} + iFrac{-1, 3} == iFrac{120, 0});

            hybrid_assert(iFrac{1, -3} + iFrac{40, 0} == iFrac{120, 0});
            hybrid_assert(iFrac{1, 3} + iFrac{40, 0} == iFrac{120, 0});
            hybrid_assert(iFrac{-1, 3} + iFrac{40, 0} == iFrac{120, 0});

            // -
            hybrid_assert(iFrac{4, 0} - iFrac{0, -1} == iFrac{4, 0});
            hybrid_assert(iFrac{8, 0} - iFrac{-4684, -4} == iFrac{32, 0});
            hybrid_assert(iFrac{0, 0} - iFrac{-7, 0} == iFrac{0, 0});
            hybrid_assert(iFrac{-7, 0} - iFrac{0, 0} == iFrac{0, 0});
            hybrid_assert(iFrac{7, 0} - iFrac{-7, 0} == iFrac{14, 0});
            hybrid_assert(iFrac{7, 0} - iFrac{0, 0} == iFrac{0, 0});
            hybrid_assert(iFrac{87, 0} - iFrac{-7, 0} == iFrac{94, 0});

            hybrid_assert(iFrac{40, 0} - iFrac{1, -3} == iFrac{120, 0});
            hybrid_assert(iFrac{40, 0} - iFrac{1, 3} == iFrac{120, 0});
            hybrid_assert(iFrac{40, 0} - iFrac{-1, 3} == iFrac{120, 0});

            hybrid_assert(iFrac{1, -3} - iFrac{-40, 0} == iFrac{120, 0});
            hybrid_assert(iFrac{1, 3} - iFrac{40, 0} == iFrac{-120, 0});
            hybrid_assert(iFrac{-1, 3} - iFrac{-40, 0} == iFrac{120, 0});

            // *
            hybrid_assert(iFrac{4, 0} * iFrac{0, -1} == iFrac{0, 0});
            hybrid_assert(iFrac{4, 0} * iFrac{0, 1} == iFrac{0, 0});
            hybrid_assert(iFrac{-4, 0} * iFrac{0, 1} == iFrac{0, 0});
            hybrid_assert(iFrac{8, 0} * iFrac{-1, -4} == iFrac{-8, 0});
            hybrid_assert(iFrac{0, 0} * iFrac{-7, 0} == iFrac{0, 0});
            hybrid_assert(iFrac{-7, 0} * iFrac{0, 0} == iFrac{0, 0});
            hybrid_assert(iFrac{7, 0} * iFrac{-7, 0} == iFrac{-49, 0});
            hybrid_assert(iFrac{7, 0} * iFrac{0, 0} == iFrac{0, 0});
            hybrid_assert(iFrac{2, 0} * iFrac{-7, 0} == iFrac{-14, 0});

            // /
            //When dividing by zero only the sign of the number being divided mattenrs 
            hybrid_assert(iFrac{4, 4} / iFrac{0, 2} == iFrac{8, 0});
            hybrid_assert(iFrac{4, 4} / iFrac{0, -2} == iFrac{8, 0});
            hybrid_assert(iFrac{-4, 4} / iFrac{0, -2} == iFrac{-8, 0});
            hybrid_assert(iFrac{4, -4} / iFrac{0, -2} == iFrac{-8, 0});
            hybrid_assert(iFrac{-4, -4} / iFrac{0, -2} == iFrac{8, 0});
            hybrid_assert(iFrac{4, -4} / iFrac{0, 2} == iFrac{-8, 0});

            hybrid_assert(iFrac{0, 2} / iFrac{4, 4} == iFrac{0, 8});
            hybrid_assert(iFrac{0, 2} / iFrac{4, -4} == iFrac{0, 8});
            hybrid_assert(iFrac{0, -2} / iFrac{4, -4} == iFrac{0, 8});
            hybrid_assert(iFrac{0, -2} / iFrac{4, 4} == iFrac{0, 8});

            hybrid_assert(iFrac{4, 0} / iFrac{0, 2} == iFrac{8, 0});
            hybrid_assert(iFrac{4, 0} / iFrac{0, -2} == iFrac{8, 0});
            hybrid_assert(iFrac{-4, 0} / iFrac{0, 2} == iFrac{-8, 0});
            hybrid_assert(iFrac{-4, 0} / iFrac{0, -2} == iFrac{-8, 0});

            //Sign of zero does not matter therefore we can handle having "bad" results
            hybrid_assert(iFrac{4, 4} / iFrac{2, 0} == iFrac{0, 8});
            hybrid_assert(iFrac{4, 4} / iFrac{-2, 0} == iFrac{0, -8});
            hybrid_assert(iFrac{-4, 4} / iFrac{-2, 0} == iFrac{0, -8});
            hybrid_assert(iFrac{4, -4} / iFrac{-2, 0} == iFrac{0, 8});
            hybrid_assert(iFrac{4, -4} / iFrac{2, 0} == iFrac{0, -8});
        }
    }


    template <class iFrac, class uFrac>
    test test_to_fraction()
    {
        hybrid_assert(to_fraction<iFrac>(5.347) == iFrac{5347, 1000});
        hybrid_assert(to_fraction<iFrac>(97.0) == iFrac{97, 1});
        hybrid_assert(to_fraction<iFrac>(97) == iFrac{97, 1});
        hybrid_assert(to_fraction<iFrac>(97.7) == iFrac{977, 10});
        hybrid_assert(to_fraction<iFrac>(0.0) == iFrac{0, 1});
        hybrid_assert(to_fraction<iFrac>(0) == iFrac{0, 1});

        hybrid_assert(to_fraction<uFrac>(5.347) == uFrac{5347, 1000});
        hybrid_assert(to_fraction<uFrac>(97.0) == uFrac{97, 1});
        hybrid_assert(to_fraction<uFrac>(97) == uFrac{97, 1});
        hybrid_assert(to_fraction<uFrac>(97.7) == uFrac{977, 10});
        hybrid_assert(to_fraction<uFrac>(0.0) == uFrac{0, 1});
        hybrid_assert(to_fraction<uFrac>(0) == uFrac{0, 1});

        hybrid_assert(to_fraction<iFrac>(-48.7) == iFrac{-487, 10});
        hybrid_assert(to_fraction<uFrac>(-48.7) == uFrac{0, 0});
    }

    template <class iFrac, class uFrac>
    test test_normalize()
    {
        hybrid_assert(normalize(iFrac{9, 11}) == iFrac{9, 11});
        hybrid_assert(normalize(iFrac{8, 4}) == iFrac{2, 1});
        hybrid_assert(normalize(iFrac{56, 4}) == iFrac{14, 1});
        hybrid_assert(normalize(iFrac{-9, 27}) == iFrac{-1, 3});

        hybrid_assert(normalize(iFrac{0, 4}) == iFrac{0, 1});
        hybrid_assert(normalize(iFrac{4, 0}) == iFrac{1, 0});
        hybrid_assert(normalize(iFrac{5, 0}) == iFrac{1, 0});
        hybrid_assert(normalize(iFrac{-5, 0}) == iFrac{-1, 0});
        hybrid_assert(normalize(iFrac{0, 0}) == iFrac{0, 0});

        hybrid_assert(normalize(uFrac{9, 11}) == uFrac{9, 11});
        hybrid_assert(normalize(uFrac{8, 4}) == uFrac{2, 1});
        hybrid_assert(normalize(uFrac{56, 4}) == uFrac{14, 1});

        hybrid_assert(normalize(uFrac{0, 4}) == uFrac{0, 1});
        hybrid_assert(normalize(uFrac{4, 0}) == uFrac{1, 0});
        hybrid_assert(normalize(uFrac{5, 0}) == uFrac{1, 0});
        hybrid_assert(normalize(uFrac{0, 0}) == uFrac{0, 0});
    }

    template <class iFrac>
    test test_purely_signed_normalize()
    {
        hybrid_assert(normalize(iFrac{9, -11}) == iFrac{-9, 11});
        hybrid_assert(normalize(iFrac{-8, 4}) == iFrac{-2, 1});
        hybrid_assert(normalize(iFrac{56, -4}) == iFrac{-14, 1});
        hybrid_assert(normalize(iFrac{-9, -27}) == iFrac{1, 3});

        hybrid_assert(normalize(iFrac{0, -4}) == iFrac{0, 1});
        hybrid_assert(normalize(iFrac{-4, 0}) == iFrac{-1, 0});
        hybrid_assert(normalize(iFrac{-5, 0}) == iFrac{-1, 0});
    }

    template<typename Num, typename Den> 
    using ManFrac = ManualFraction<FractionData<Num, Den>>;


    template <class iFrac, class uFrac>
    test test_single_fraction()
    {
        test_fraction_creation<iFrac, uFrac>();
        test_fraction_specials<iFrac, uFrac>();
        test_fraction_comparison<iFrac, uFrac>();
        test_fraction_aritmetic<iFrac, uFrac>();
        test_to_fraction<iFrac, uFrac>();
        test_normalize<iFrac, uFrac>();
    }

    template <class iFrac>
    test test_purely_signed_single_fraction()
    {
        test_purely_signed_fraction_creation<iFrac>();
        test_purely_signed_fraction_comparison<iFrac>();
        test_purely_signed_fraction_aritmetic<iFrac>();
        test_purely_signed_normalize<iFrac>();
    }

    test test_fraction()
    {

        //test_fraction_creation
        test_single_fraction<ManFrac<i32, i32>, ManFrac<u32, u32>>();
        test_single_fraction<ManFrac<i32, i16>, ManFrac<u32, u16>>();
        test_single_fraction<ManFrac<i16, i32>, ManFrac<u16, u32>>();

        test_single_fraction<ManFrac<i32, u32>, ManFrac<u32, u32>>();
        test_single_fraction<ManFrac<i32, u16>, ManFrac<u32, u16>>();
        test_single_fraction<ManFrac<i16, u32>, ManFrac<u16, u32>>();

        test_single_fraction<ManFrac<i32, i64>, ManFrac<u32, u64>>();
        test_single_fraction<ManFrac<i64, i32>, ManFrac<u64, u32>>();
        test_single_fraction<ManFrac<i64, i64>, ManFrac<u64, u64>>();

        test_single_fraction<ManFrac<i32, u64>, ManFrac<u32, u64>>();
        test_single_fraction<ManFrac<i64, u32>, ManFrac<u64, u32>>();
        test_single_fraction<ManFrac<i64, u64>, ManFrac<u64, u64>>();

        //test_purely_signed_fraction_comparison
        test_purely_signed_single_fraction<ManFrac<i16, i16>>();
        test_purely_signed_single_fraction<ManFrac<i32, i32>>();
        test_purely_signed_single_fraction<ManFrac<i32, i64>>();
        test_purely_signed_single_fraction<ManFrac<i64, i32>>();
        test_purely_signed_single_fraction<ManFrac<i64, i64>>();
    }

    run_test(test_fraction);
}

#include "../lib/jot/undefs.h"