#pragma once
#include <cstddef>
#include <cassert>
#include <utility>
#include "span.h"
#include "meta.h"
#include "defines.h"

namespace jot
{

    template<typename T>
    concept per_elem_like = requires(T t)
    {
        t.begin;
        t.end;
    };

    template<typename T>
    struct PerElem
    {
        T begin;
        T end;

        using Val = std::remove_cvref_t<decltype(*begin)>;

        #define single_for() \
            for(mut it = begin; it != end; ++it) \

        #define paralel_for(other) \
            mut it2 = other.begin; \
            for(mut it1 = begin; it1 != end; ++it1, ++it2) \

        #define single_op_(qual, op) \
            proc& operator##op(qual Val& other) requires requires() {*begin op other;} \
            { \
                single_for() \
                    *it op other; \
                return *this; \
            }

        #define paralel_op_(qual, op) \
            proc& operator##op(qual per_elem_like auto& other) requires requires() {*begin op *other.begin;} \
            { \
                paralel_for(other) \
                    *it1 op *it2; \
                return *this; \
            }

        #define all_of_single_op_(qual, op) \
            proc operator##op(qual Val& other) requires requires() {*begin op other;} \
            { \
                single_for() \
                { \
                    if(!(*it op other)) \
                        return false; \
                } \
                return true; \
            }

        #define all_of_paralel_op_(qual, op) \
            proc operator##op(qual per_elem_like auto& other) requires requires() {*begin op *other.begin;} \
            { \
                paralel_for(other) \
                { \
                    if(!(*it1 op *it2)) \
                        return false; \
                } \
                return true; \
            }

        #define unary_increment_prefix_op(op) \
            proc& operator##op() requires requires() {op *begin;} \
            { \
                single_for() \
                    op *it; \
                return *this; \
            }

        #define unary_increment_postfix_op(op) \
            proc operator##op(int) requires requires() {*begin op;} \
            { \
                single_for() \
                    *it op; \
                return *this; \
            }

        #define single_op(op) \
            single_op_(const, op); \
            single_op_(, op); 

        #define paralel_op(op) \
            paralel_op_(const, op); \
            paralel_op_(, op); 

        #define all_of_single_op(op) \
            all_of_single_op_(const, op); \
            all_of_single_op_(, op); 

        #define all_of_paralel_op(op) \
            all_of_paralel_op_(const, op); \
            all_of_paralel_op_(, op); 

        single_op(=);
        single_op(+=);
        single_op(-=);
        single_op(*=);
        single_op(/=);
        single_op(%=);
        single_op(&=);
        single_op(|=);
        single_op(^=);
        single_op(<<=);
        single_op(>>=);

        paralel_op(+=);
        paralel_op(-=);
        paralel_op(*=);
        paralel_op(/=);
        paralel_op(%=);
        paralel_op(&=);
        paralel_op(|=);
        paralel_op(^=);
        paralel_op(<<=);
        paralel_op(>>=);

        all_of_single_op(==);
        all_of_single_op(!=);
        all_of_single_op(<);
        all_of_single_op(>);
        all_of_single_op(>=);
        all_of_single_op(<=);

        all_of_paralel_op(==);
        all_of_paralel_op(!=);
        all_of_paralel_op(<);
        all_of_paralel_op(>);
        all_of_paralel_op(>=);
        all_of_paralel_op(<=);

        unary_increment_prefix_op(++);
        unary_increment_prefix_op(--);
        unary_increment_postfix_op(++);
        unary_increment_postfix_op(--);


        proc& operator=(T&& other) requires requires() {*begin = std::move(*other.begin); } 
        { 
            paralel_for(other) 
                *it1 = std::move(*it2);
            return *this; 
        }

        proc operator<=>(T&& other) requires requires() {*begin <=> *other.begin; }
        { 
            using Ret = decltype(*begin <=> *other.begin);
            paralel_for(other) 
            {
                let comp = *it1 <=> *it2;
                if(comp != 0)
                    return comp;
            }
            return cast(Ret)(0); 
        }


        #undef single_for
        #undef paralel_for
        #undef single_op_
        #undef paralel_op_
        #undef all_of_op_
        #undef unary_increment_prefix_op
        #undef unary_increment_postfix_op
        #undef single_op
        #undef paralel_op
        #undef all_of_op
    };

    //deduction guide
    template <class It>
    PerElem(It, It) -> PerElem<It>;

    func custom_invoke(mut&& container, PerElementDummy) noexcept 
        requires requires (){ PerElem{std::begin(container), std::end(container)}; }
    { return PerElem{std::begin(container), std::end(container)}; }
}
#include "undefs.h"