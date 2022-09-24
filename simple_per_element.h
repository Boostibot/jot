#pragma once

//#include <ranges>
//#include <tuple>

#include "utils.h"
#include "defines.h"

namespace jot
{
    //Per element is a generic class used to perform simple operations on each
    //  element of a collection
    //  it can be hooked into by specializing the operator().
    //This is so that containers can specialize all operators but can still be used
    //  conveniniently
    //Used like such
    //    Array arr1 = {1, 2, 3};
    //    Array arr2 = {4, 5, 8};
    // 
    //    arr1() += arr2();       //Add each element of arr2 to arr2
    //    arr1() -= 5;            //Remove 5 from every element
    //    arr1() ++;              //Increments every element
    //    if(arr1() >= arr2() && arr1() < 5) {} //Per element comparisons

    template<typename T>
    concept per_elem_like = requires(T t)
    {
        *t.begin_it;
        *t.end_it;
    };

    template<typename T>
    struct PerElem : std::ranges::view_interface<PerElem<T>>
    {
        T begin_it;
        T end_it;

        auto begin() const { return begin_it; }
        auto end() const { return begin_it; }

        using Val = std::remove_cvref_t<decltype(*begin_it)>;

        #define single_for() \
            for(mut it = begin_it; it != end_it; ++it) \

        #define paralel_for(other) \
            mut it2 = other.begin_it; \
            for(mut it1 = begin_it; it1 != end_it; ++it1, ++it2) \

        #define mono_op_(qual, op) \
            proc& operator##op(qual Val& other) requires requires() {*begin_it op other;} \
            { \
                single_for() \
                    *it op other; \
                return *this; \
            }

        #define dual_op_(qual, op) \
            proc& operator##op(qual per_elem_like auto& other) requires requires() {*begin_it op *other.begin_it;} \
            { \
                paralel_for(other) \
                    *it1 op *it2; \
                return *this; \
            }

        #define all_of_mono_op_(qual, op) \
            proc operator##op(qual Val& other) requires requires() {*begin_it op other;} \
            { \
                single_for() \
                { \
                    if(!(*it op other)) \
                        return false; \
                } \
                return true; \
            }

        #define all_of_dual_op_(qual, op) \
            proc operator##op(qual per_elem_like auto& other) requires requires() {*begin_it op *other.begin_it;} \
            { \
                paralel_for(other) \
                { \
                    if(!(*it1 op *it2)) \
                        return false; \
                } \
                return true; \
            }

        #define unary_increment_prefix_op(op) \
            proc& operator##op() requires requires() {op *begin_it;} \
            { \
                single_for() \
                    op *it; \
                return *this; \
            }

        #define unary_increment_postfix_op(op) \
            proc operator##op(int) requires requires() {*begin_it op;} \
            { \
                single_for() \
                    *it op; \
                return *this; \
            }

        #define mono_op(op) \
            mono_op_(const, op); \
            mono_op_(, op); 

        #define dual_op(op) \
            dual_op_(const, op); \
            dual_op_(, op); 

        #define all_of_mono_op(op) \
            all_of_mono_op_(const, op); \
            all_of_mono_op_(, op); 

        #define all_of_dual_op(op) \
            all_of_dual_op_(const, op); \
            all_of_dual_op_(, op); 

        mono_op(=);
        mono_op(+=);
        mono_op(-=);
        mono_op(*=);
        mono_op(/=);
        mono_op(%=);
        mono_op(&=);
        mono_op(|=);
        mono_op(^=);
        mono_op(<<=);
        mono_op(>>=);

        dual_op(+=);
        dual_op(-=);
        dual_op(*=);
        dual_op(/=);
        dual_op(%=);
        dual_op(&=);
        dual_op(|=);
        dual_op(^=);
        dual_op(<<=);
        dual_op(>>=);

        all_of_mono_op(==);
        all_of_mono_op(!=);
        all_of_mono_op(<);
        all_of_mono_op(>);
        all_of_mono_op(>=);
        all_of_mono_op(<=);

        all_of_dual_op(==);
        all_of_dual_op(!=);
        all_of_dual_op(<);
        all_of_dual_op(>);
        all_of_dual_op(>=);
        all_of_dual_op(<=);

        unary_increment_prefix_op(++);
        unary_increment_prefix_op(--);
        unary_increment_postfix_op(++);
        unary_increment_postfix_op(--);


        proc& operator=(T&& other) requires requires() {*begin_it = std::move(*other.begin_it); } 
        { 
            paralel_for(other) 
                *it1 = std::move(*it2);
            return *this; 
        }

        proc operator<=>(T&& other) requires requires() {*begin_it <=> *other.begin_it; }
        { 
            using Ret = decltype(*begin_it <=> *other.begin_it);
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
        #undef mono_op_
        #undef dual_op_
        #undef all_of_op_
        #undef unary_increment_prefix_op
        #undef unary_increment_postfix_op
        #undef mono_op
        #undef dual_op
        #undef all_of_op
    };

    //deduction guide
    template <class It>
    PerElem(It, It) -> PerElem<It>;

    //Per default hooks into every single container supporting begin_it end_it
    func custom_invoke(mut&& container, PerElementDummy) noexcept 
        requires requires (){ PerElem{std::begin(container), std::end(container)}; }
    { return PerElem{std::begin_it(container), std::end_it(container)}; }
}
#include "undefs.h"