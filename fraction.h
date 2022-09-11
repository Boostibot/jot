#pragma once

#include <numeric>
#include <limits>
#include <cmath>
#include <cassert>

#include "types.h"
#include "meta.h"
#include "defines.h"

namespace jot
{
    func numerator(let& frac)                   { return frac.numerator; }
    func denominator(let& frac)                 { return frac.denominator; }
    proc numerator(mut& frac, let& set_to_val)  { return (frac.numerator = set_to_val); }
    proc denominator(mut& frac, let& set_to_val){ return (frac.denominator = set_to_val); }

    template <class FracData>
    concept fraction_data = requires (FracData data)
    {
        //Constructible
        FracData(0, 0U);

        //Has members
        typename FracData::Numerator;
        typename FracData::Denominator;

        //Has setters and getters
        numerator(data);
        denominator(data);

        numerator(data, 0);
        denominator(data, 0U);
    };


    template <fraction_data Frac>
    static constexpr bool is_signed_frac_v = std::is_signed_v<Frac::Numerator>;

    template <fraction_data Frac>
    static constexpr bool is_purely_signed_frac_v = std::is_signed_v<Frac::Numerator> && std::is_signed_v<Frac::Denominator>;

    template <typename T1, typename T2>
    struct bigger_type : std::conditional<(sizeof(T1) > sizeof(T2)), T1, T2> {};

    template <typename T1, typename T2>
    using bigger_type_t = typename bigger_type<T1, T2>::type;

    template <typename T1>
    struct sufficient_int : std::conditional<std::is_signed_v<T1>,
        T1,
        std::conditional_t<(sizeof(i16) > sizeof(T1)), 
        i16,
        std::conditional_t<(sizeof(i32) > sizeof(T1)), 
        i32, 
        i64>>> {};

    template <typename T1, typename T2>
    struct common_int : sufficient_int<bigger_type_t<T1, T2>> {};

    template <typename T1, typename T2>
    using common_int_t = typename common_int<T1, T2>::type;

    //A trivial implementation of fraction_data
    template <typename Num, typename Den>
    struct Fraction_Data
    {
        static_assert(!(std::is_unsigned_v<Num> && std::is_signed_v<Den>), 
            "must not be (unsigned)/(signed)");
        using Numerator = Num;
        using Denominator = Den;

        Num numerator;
        Den denominator;
    };

    func spread(const fraction_data auto& frac) 
    { return Fraction_Data{numerator(frac), denominator(frac)}; }

    //specials
    namespace detail 
    {
        template <fraction_data Frac>
        func to_num(let num)
        { return cast(Frac::Numerator)(num); }

        template <fraction_data Frac>
        func to_den(let den)
        { return cast(Frac::Denominator)(den); }

        template <fraction_data FracData>
        func num_(const FracData& frac) 
        { return numerator(frac); }

        template <fraction_data FracData>
        func den_(const FracData& frac) 
        {  return denominator(frac); }

        template <fraction_data FracData>
        func num(const FracData& frac) 
        { 
            using Common = common_int_t<FracData::Numerator, FracData::Denominator>;
            return cast(Common) num_(frac); 
        }

        template <fraction_data FracData>
        func den(const FracData& frac) 
        { 
            using Common = common_int_t<FracData::Numerator, FracData::Denominator>;
            return cast(Common) den_(frac); 
        }

        template <fraction_data FracData>
        proc num(FracData& frac, let& val) 
        { 
            using Common = common_int_t<FracData::Numerator, FracData::Denominator>;
            return cast(Common)(numerator(frac, cast(FracData::Numerator)(val))); 
        }

        template <fraction_data FracData>
        proc den(FracData& frac, let& val) 
        { 
            using Common = common_int_t<FracData::Numerator, FracData::Denominator>;
            return cast(Common) denominator(frac, cast(FracData::Denominator)(val)); 
        }

        template <fraction_data Frac>
        func construct(let num, let den)
        { return Frac(to_num<Frac>(num), to_den<Frac>(den)); }

        template <fraction_data Frac>
        proc& assign(Frac& frac, let num, let den)
        {
            numerator(frac, to_num<Frac>(num));
            denominator(frac, to_den<Frac>(den));
            return frac;
        }
    }

    template <fraction_data Frac>
    func nan()                     { return detail::construct<Frac>(0, 0); }
    proc& nan(fraction_data auto& frac) { return detail::assign(frac, 0, 0); }

    template <fraction_data Frac>
    func infinity()                     { return detail::construct<Frac>(1, 0); }
    proc& infinity(fraction_data auto& frac) { return detail::assign(frac, 1, 0); }

    template <fraction_data Frac>
    func negative_infinity()
    {
        static_assert(is_signed_frac_v<Frac>);
        return detail::construct<Frac>(-1, 0);
    }

    template <fraction_data Frac>
    proc& negative_infinity(Frac& frac)
    {
        static_assert(is_signed_frac_v<Frac>);
        return detail::assign<Frac>(frac, -1, 0);
    }

    func is_normal(const fraction_data auto& frac)             { return denominator(frac) != 0; }
    func is_nan(const fraction_data auto& frac)                { return !is_normal(frac) && numerator(frac) == 0; }
    func is_infinite(const fraction_data auto& frac)           { return !is_normal(frac) && numerator(frac) != 0; }
    func is_infinity(const fraction_data auto& frac)           { return !is_normal(frac) && numerator(frac) > 0; }
    func is_negative_infinity(const fraction_data auto& frac)  { return !is_normal(frac) && numerator(frac) < 0; }

    func sign(const std::integral auto& scalar)
    {
        return scalar >= 0 
            ? cast(i8) 1 
            : cast(i8) -1;
    }

    func sign(const fraction_data auto& frac)
    { return sign(frac.numerator) * sign(frac.denominator); }

    func abs(const std::integral auto& scalar) 
    { return scalar * sign(scalar); }
    func abs(const fraction_data auto& frac)
    {
        let [num, den] = spread(frac);
        return detail::construct(abs(num), abs(den));
    }


    //Normalization
    namespace detail
    {
        template <fraction_data Frac>
        func norm_ratio(const Frac& frac)
        { return std::gcd(numerator(frac), denominator(frac)); }
    }

    template <fraction_data Frac>
    func is_invarinat(const Frac& frac)
    {
        using namespace detail;
        let [num, den] = spread(frac);

        //If special value
        if(den == 0)
        {
            if constexpr (is_signed_frac_v<Frac>)
                if(num == -1) //-INF (if exists)
                    return true;

            return num == 0  //NAN
                || num == 1; //INF
        }
        if(den < 0)
            return false;

        return norm_ratio(frac) == 1;
    }


    template <fraction_data Frac>
    func normalize_sign(const Frac& frac)
    {
        if constexpr (is_purely_signed_frac_v<Frac>)
        {
            let [num, den] = spread(frac);
            if (den < 0) 
                return detail::construct(-num, -den);
        }

        return frac;
    }

    template <fraction_data Frac>
    proc& normalize_assign(Frac& frac)
    {
        using namespace detail;
        mut [num, den] = spread(frac);

        if (den == 0)
        {
            //Normalize +/- inf
            if(num > 0)
                num = to_num<Frac>(1);
            else if(num < 0)
                num = to_num<Frac>(-1);

            assign(frac, num, den);
            return frac;
        }

        let ratio = abs(norm_ratio(frac));
        num /= cast(Frac::Numerator)(ratio);
        den /= cast(Frac::Denominator)(ratio);
        
        if constexpr (is_purely_signed_frac_v<Frac>)
            if (den < 0) 
            {
                num = -num;
                den = -den;
            }

        assign(frac, num, den);

        assert(is_invarinat(frac));
        return frac;
    }

    proc normalize(fraction_data auto frac) 
    { normalize_assign(frac); return frac; }

    static constexpr f64 DEF_TO_FRAC_PRECISION = 5e-8;
    static constexpr i64 DEF_TO_FRAC_CYCLES = 12;

    template <fraction_data Frac, std::integral Int>
    proc& to_fraction(Int number, Frac& out)
    {
        using namespace detail;
        return assign(out, number, 1);
    }

    template <fraction_data Frac, std::floating_point Float, std::integral Int = i64, bool do_precision = true>
    proc& to_fraction(Float number, Frac& out, Int cycles = cast(Int) DEF_TO_FRAC_CYCLES, Float precision = cast(Float) DEF_TO_FRAC_PRECISION)
    {
        using Num = Frac::Numerator;
        using Den = Frac::Denominator;

        if(!std::is_constant_evaluated())
        {
            if(std::isinf(number))
                return infinity<Frac>(out);

            if(std::isnan(number))
                return nan<Frac>(out);
        }

        i16 sign;
        if(number < 0.0)
        {
            //Invalid representation - if the user wants to have the abs value they should
            // abs the number
            if constexpr (!is_signed_frac_v<Frac>)
                return nan<Frac>(out);

            sign = -1;
            number *= -1; //abs
        }
        else
            sign = 1;

        Int counter = 0;
        Float decimal_part = number - cast(Int) number;

        Float result[2] = {cast(Float) cast(Int) number, 1};
        Float prev[2] = {1.0, .0};
        Float temp[2] = {.0, .0};

        while(counter < cycles)
        {
            //setting do precision to off and counter to some small number can
            // lead to loop unrolling and further optimization
            if(do_precision && decimal_part < precision)
                break;

            Float new_number = 1 / decimal_part;
            Float whole_part = cast(Float) cast(Int) new_number;

            temp[0] = result[0];
            temp[1] = result[1];

            result[0] = whole_part * result[0] + prev[0];
            result[1] = whole_part * result[1] + prev[1];

            prev[0] = temp[0];
            prev[1] = temp[1];

            decimal_part = new_number - whole_part;
            counter ++;
        }

        return detail::assign(out, result[0] * sign, result[1]);
        //return normalize_assign(out);
    }

    template <fraction_data Frac, std::floating_point Float, std::integral Int = i64, bool do_precision = true>
    func to_fraction(Float number, Int cycles = DEF_TO_FRAC_CYCLES, Float precision = cast(Float) DEF_TO_FRAC_PRECISION)
    {
        Frac out;
        return to_fraction<Frac, Float, Int, do_precision>(number, out, cycles, precision);
    }

    template <fraction_data Frac, std::integral Int>
    proc to_fraction(Int number)
    {
        Frac out;
        return to_fraction(number, out);
    }

    template <std::floating_point Float = double>
    func to_float(const fraction_data auto& frac)   { return cast(Float)(numerator(frac)) / denominator(frac); }
    func to_integer(const fraction_data auto& frac) { return numerator(frac) / denominator(frac); }

    template <fraction_data FracData>
    struct TrivialFraction : FracData 
    {
        static_assert(!(std::is_unsigned_v<FracData::Numerator> && std::is_signed_v<FracData::Denominator>), 
            "must not be (unsigned)/(signed)");

        using Fraction_Data = FracData;
        using Numerator = typename FracData::Numerator;
        using Denominator = typename FracData::Denominator;
        static constexpr bool is_signed = is_signed_frac_v<FracData>;

        private:
        using Frac = TrivialFraction;

        protected:
        using Num = Numerator;
        using Den = Denominator;
        using Common = common_int_t<Num, Den>;

        public:
        constexpr TrivialFraction() = default;
        constexpr TrivialFraction(const Frac&) = default;
        constexpr TrivialFraction(Frac&&) = default;

        constexpr TrivialFraction(const FracData& data)           : FracData(data) {}
        constexpr TrivialFraction(FracData&& data)                : FracData(std::move(data)) {}
        constexpr TrivialFraction(Num num, Den den = cast(Den) 1) : FracData(num, den) {} 

        template <std::floating_point Float, std::integral Int = i64>
        constexpr TrivialFraction(Float number, Int cycles = cast(Int) DEF_TO_FRAC_CYCLES, Float precision = cast(Float) DEF_TO_FRAC_PRECISION)
            : FracData() 
        { to_fraction(number, *this, cycles, precision); }

        constexpr Frac& operator=(const Frac&) = default;
        constexpr Frac& operator=(Frac&&) = default;

        template<std::floating_point T>
        constexpr explicit operator T() const { return to_float<T>(*this); }
        template<std::integral T>
        constexpr explicit operator T() const { return cast(T) to_integer(*this); }

        func operator+() const                    { return *this;}
        func operator-() const requires is_signed { return detail::construct(-numerator(*this), denominator(*this)); }

        proc& operator +=(const Frac& other) 
        {
            using namespace detail;
            let new_num = num(*this) * den(other) + num(other) * den(*this);
            let new_den = den(*this) * den(other);

            return assign(*this, new_num, new_den);
        }

        proc& operator -=(const Frac& other) 
        {
            using namespace detail;
            let new_num = num(*this) * den(other) - num(other) * den(*this);
            let new_den = den(*this) * den(other);

            return assign(*this, new_num, new_den);
        }

        proc& operator *=(const Frac& other) 
        {
            using namespace detail;
            let new_num = num_(*this) * num_(other);
            let new_den = den_(*this) * den_(other);

            return assign(*this, new_num, new_den);
        }

        proc& operator /=(const Frac& other) 
        {
            using namespace detail;
            let new_num = num(*this) * den(other);
            let new_den = den(*this) * num(other);

            return assign(*this, new_num, new_den);
        }

        proc operator +(const Frac& other) const {mut copy = *this; copy += other; return copy;}
        proc operator -(const Frac& other) const {mut copy = *this; copy -= other; return copy;}
        proc operator *(const Frac& other) const {mut copy = *this; copy *= other; return copy;}
        proc operator /(const Frac& other) const {mut copy = *this; copy /= other; return copy;}

        func operator ==(const Frac& other) const 
        {
            return this->operator<=>(other) == 0;
        }

        func operator <=>(const Frac& other) const 
        {
            using namespace detail;
            let norm1 = num(*this) * den(other);
            let norm2 = den(*this) * num(other);
            return norm1 - norm2;
        }
    };
}

#include "undefs.h"