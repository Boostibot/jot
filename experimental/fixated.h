#pragma once
#include "defines.h"

namespace jot 
{
    template <typename Fix>
    struct Fixated 
    {
        Fix fixated;

        constexpr Fixated(Fix fix) : fixated(fix) {}
        constexpr Fixated() = default;
        constexpr Fixated(const Fixated&) = default;
        constexpr Fixated(Fixated&&) = default;

        constexpr Fixated& operator=(const Fixated&) = default;
        constexpr Fixated& operator=(Fixated&&) = default;

        constexpr operator Fix() const {return fixated;}

        template <typename T> requires requires() {cast(T) fixated;}
        constexpr explicit operator T() const {return cast(T)fixated;}

        #define fixated_bin_op(op)                          \
            constexpr Fixated operator op(let& value) const \
            requires requires () { value op fixated; }      \
            {                                               \
                return cast(Fix)(fixated op value);         \
            }                                               \

        #define fixated_pre_op(op)                      \
            constexpr Fixated operator op() const       \
                requires requires () { op fixated; }    \
            {                                           \
                return cast(Fix)(op fixated);           \
            }                                           \

        fixated_bin_op(+);
        fixated_bin_op(-);
        fixated_bin_op(*);
        fixated_bin_op(/);
        fixated_bin_op(%);
        fixated_bin_op(&);
        fixated_bin_op(|);
        fixated_bin_op(^);
        fixated_bin_op(>>);
        fixated_bin_op(<<);

        fixated_pre_op(+);
        fixated_pre_op(-);
        fixated_pre_op(~);

        #undef fixated_bin_op
        #undef fixated_pre_op
    };

    template <class Fix>
    Fixated(Fix) -> Fixated<Fix>;

    template <typename Field>
    func fixate(Field field)
    {
        return Fixated<Field>(field);
    }
}

#include "undefs.h"