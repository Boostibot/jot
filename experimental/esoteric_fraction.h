#pragma once
#include <numeric>
#include <limits>
#include <cmath>
#include <cassert>

#include "../types.h"
#include "../meta.h"
#include "../fraction.h"

#include "../defines.h"

namespace jot 
{

    template <fraction_data FracData>
    struct Manual_Fraction : TrivialFraction<FracData> 
    {
        static_assert(!(std::is_unsigned_v<FracData::Numerator> && std::is_signed_v<FracData::Denominator>), 
            "must not be (unsigned)/(signed)");

        using Fraction_Data = FracData;
        using Numerator = typename FracData::Numerator;
        using Denominator = typename FracData::Denominator;
        static constexpr bool is_signed = is_signed_frac_v<FracData>;

        private:
        using Frac = Manual_Fraction;
        using Num = Numerator;
        using Den = Denominator;
        using Common = common_int_t<Num, Den>;

        public:
        constexpr Manual_Fraction() = default;
        constexpr Manual_Fraction(const Frac&) = default;
        constexpr Manual_Fraction(Frac&&) = default;

        constexpr Manual_Fraction(const FracData& data)           : TrivialFraction<FracData>(data) {}
        constexpr Manual_Fraction(FracData&& data)                : TrivialFraction<FracData>(std::move(data)) {}
        constexpr Manual_Fraction(Num num, Den den = cast(Den)(1)) : TrivialFraction<FracData>(num, den) {} 

        template <std::floating_point Float, std::integral Int = i64>
        constexpr Manual_Fraction(Float number, Int cycles = cast(Int)(DEF_TO_FRAC_CYCLES), Float precision = cast(Float)(DEF_TO_FRAC_PRECISION))
            : TrivialFraction<FracData>(number, cycles, precision) {}

        constexpr Frac& operator=(const Frac&) = default;
        constexpr Frac& operator=(Frac&&) = default;

        template<std::floating_point T>
        constexpr explicit operator T() const { return to_float<T>(*this); }
        template<std::integral T>
        constexpr explicit operator T() const { return cast(T) to_integer(*this); }
        constexpr explicit operator FracData() const { return cast(FracData) cast(TrivialFraction<FracData>) *this; }

        func operator+() const                    { return *this;}
        func operator-() const requires is_signed { return detail::construct(-numerator(*this), denominator(*this)); }

            //If you belive in math close your eyes because this is gonna hurt
            proc& operator +=(const Frac& other) 
        {
            using namespace detail;
            mut this_num = detail::num(*this);
            mut this_den = detail::den(*this);
            mut other_num = detail::num(other);
            mut other_den = detail::den(other);

            //Inversion just for the case of
            // 7/0 + 1/-3 = 7/0 (would be -7/0 without this)
            if constexpr(is_purely_signed_frac_v<FracData>)
                if ((other_den < 0) != (this_den < 0)) 
                {
                    other_num = -other_num;
                    other_den = -other_den;
                }

            //7/0 + 1/0 => 8/0
            //7/0 - 1/0 => 6/0            
            //7/0 - 0/0 => 0/0

            //If both denominators equal its just a common occurance 
            // or it can be the case of operation on two sepcial values
            if(this_den == other_den)
            {
                //Even if its an operation on infinities we allow it:
                // ie: 7/0 + 1/0 = 8/0
                //But we have to assure NaN is invariant:
                // 7/0 + 0/0 = 0/0 
                let tn_zero = cast(u8)(this_num != 0);
                let on_zero = cast(u8)(other_num != 0);

                return *assign(this, (this_num + other_num) * (tn_zero & on_zero), this_den);
            }

            let new_num = this_num * other_den + other_num * this_den;
            let new_den = this_den * other_den;

            return *assign(this, new_num, new_den);
        }

        proc& operator -=(const Frac& other) 
        {
            using namespace detail;
            mut this_num = detail::num(*this);
            mut this_den = detail::den(*this);
            mut other_num = detail::num(other);
            mut other_den = detail::den(other);

            //Inversion just for the case of
            // 7/0 + 1/-3 = 7/0 (would be -7/0 without this)
            if constexpr(is_purely_signed_frac_v<FracData>)
                if ((other_den < 0) != (this_den < 0)) 
                {
                    other_num = -other_num;
                    other_den = -other_den;
                }

            if(this_den == other_den)
            {
                let tn_zero = cast(u8)(this_num != 0);
                let on_zero = cast(u8)(other_num != 0);

                return *assign(this, (this_num - other_num) * (tn_zero & on_zero), this_den);
            }

            let new_num = this_num * other_den - other_num * this_den;
            let new_den = this_den * other_den;

            return *assign(this, new_num, new_den);
        }

        proc& operator *=(const Frac& other) 
        {
            using namespace detail;
            let new_num = num_(*this) * num_(other);
            let new_den = den_(*this) * den_(other);

            return *assign(this, new_num, new_den);
        }

        proc& operator /=(const Frac& other) 
        {
            using namespace detail;
            let this_num = detail::num(*this);
            let this_den = detail::den(*this);
            mut other_num = detail::num(other);
            mut other_den = detail::den(other);

            //If can have negative denominator we need to assure the sign gets carried over
            // for cases like the foloowing
            // (1/0) / (-1/3) --> (-3/0) doesnt work without special handling
            // (1/0) / (1/-3) --> (-3/0) works

            // (-1/3) / (1/0) --> (-3/0) doesnt work without special handling
            // (1/0) / (1/-3) --> (-3/0) works
            if constexpr(is_purely_signed_frac_v<FracData>)
                if ((other_den < 0) != (this_den < 0)) 
                {
                    other_num = -other_num;
                    other_den = -other_den;
                }

            let new_num = this_num * other_den;
            let new_den = this_den * other_num;

            return *assign(this, new_num, new_den);
        }

        proc operator +(const Frac& other) const {mut copy = *this; copy += other; return copy;}
        proc operator -(const Frac& other) const {mut copy = *this; copy -= other; return copy;}
        proc operator *(const Frac& other) const {mut copy = *this; copy *= other; return copy;}
        proc operator /(const Frac& other) const {mut copy = *this; copy /= other; return copy;}


        func operator ==(const Frac& other) const 
        {
            using namespace detail;
            let this_num = detail::num(*this);
            let this_den = detail::den(*this);
            mut other_num = detail::num(other);
            mut other_den = detail::den(other);

            let norm1 = this_num * other_den;
            let norm2 = other_num * this_den;
            if(norm1 != norm2) 
                return false;

            //If both numerators arent zero returns true
            //if((this_den != 0 && other_den != 0)
            //return true

            let ret = (bool) (((u8)(this_den != 0) & (u8)(other_den != 0)) //If both numerators arent zero returns true
                | ((u8)(this_num == other_num) & (u8)(this_den == other_den))); //Else returns same only if teh two equal

                                                                                //let ret = (bool)( (u8)(this_den == 0) & (u8)(other_den == 0) & (u8)(this_num == 0) & (u8)(this_den == 0) );
            return ret;
        }

        func operator <=>(const Frac& other) const 
        {
            using namespace detail;
            let this_num = detail::num(*this);
            let this_den = detail::den(*this);
            mut other_num = detail::num(other);
            mut other_den = detail::den(other);

            //If the signs in denominators dont match flips the sign to match
            //  Is important for cases like:
            //  0/1 > 1/-3 => norm1 = 0, norm2 = 1
            //  0/1 > 1/3  => norm1 = 0, norm2 = 1
            if constexpr(is_purely_signed_frac_v<FracData>)
                if ((other_den < 0) != (this_den < 0)) 
                {
                    other_num = -other_num;
                    other_den = -other_den;
                }

            let norm1 = this_num * other_den;
            let norm2 = other_num * this_den;
            if(norm1 == norm2) 
            {
                //If comparing nans alwasy return false 
                //  (is merged with line on bottom to avoid misspredictions - measured 30% speedup)

                //if((this_num == 0 && this_den == 0)
                //|| (other_num == 0 && other_num == 0))
                //return cast(Common) 0;

                let tn_zero = cast(u8)(this_num != 0);
                let td_zero = cast(u8)(this_den != 0);
                let on_zero = cast(u8)(other_num != 0);
                let od_zero = cast(u8)(other_num != 0);

                return (this_num - other_num) 
                    * sign(this_den) //If is negative flips the result 
                    * ((tn_zero | td_zero) & (on_zero | od_zero));  //If comparing nans alwasy return false
            }

            return (norm1 - norm2);
        }
    };
}

#include "../undefs.h"