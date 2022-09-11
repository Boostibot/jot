#pragma once

#include <ranges>

#include "utils.h"
#include "defines.h"

namespace jot
{
    template <typename T>
    concept expr_like = requires(T t)
    {
        t.fn().val;
        t.fn().has;
        t.apply = false;
    };


    template <typename T>
    struct Per_Elem_Res
    {
        T val;
        bool has;
    };

    template <typename T>
    Per_Elem_Res(T, bool) -> Per_Elem_Res<T>;

    #define Exact_Opt(val, has) Per_Elem_Res<decltype(val)>{val, has}
    #define Exact_Expr(expr) Per_Elem_Expr<decltype(expr)>{expr}


    template <typename Fn>
    struct Per_Elem_Expr
    {
        Fn fn;
        bool top_level = true;

        constexpr void apply()
        {
            while(true)
            {
                auto res = fn();
                if(res.has == false)
                    break;
            }
        }

        constexpr ~Per_Elem_Expr()
        {
            if(top_level)
                apply();
        }

        template <typename Fn2> 
        constexpr auto operator = (Per_Elem_Expr<Fn2>&& right) 
            requires requires() { 
            fn().val = right.fn().val; 
        } 
        { 
            top_level = false; 
            right.top_level = false; 

            auto res = [left = *this, right]() mutable { 
                auto left_res = left.fn(); 
                auto right_res = right.fn(); 
                return Exact_Opt(left_res.val = std::move(right_res.val), left_res.has && right_res.has); 
            };

            return Exact_Expr(res);
        } 


        template <typename Other> 
        constexpr auto operator = (Other&& right) 
            requires (!expr_like<Other>) && requires() { 
            fn().val = right; 
        } 
        { 
            top_level = false; 

            auto res = [left = *this, right]() mutable { 
                auto left_res = left.fn(); 
                return Exact_Opt(left_res.val = std::forward<Other>(right), left_res.has); 
            };

            return Exact_Expr(res);
        } 

        template <typename ... Ts>
        constexpr auto operator()(Ts&& ... ts) 
            requires requires() { 
            fn().val(std::forward<Ts>(ts)...); 
        } 
        {
            top_level = false;
            auto res = [=, left = *this]() mutable { 
                auto left_res = left.fn(); 
                return Exact_Opt(left_res.val(std::forward<Ts>(ts)...), left_res.has);
            };

            return Exact_Expr(res);
        };


        template <typename T>
        constexpr auto operator[](T&& right) 
            requires requires() { 
            fn().val[std::forward<T>(right)]; 
        } 
        {
            top_level = false;
            auto res = [left = *this, right]() mutable { 
                auto left_res = left.fn(); 
                return Exact_Opt(left_res.val[right], left_res.has);
            };

            return Exact_Expr(res);
        };

    };

    template <typename T>
    Per_Elem_Expr(T) -> Per_Elem_Expr<T>;


    #define unary_mixfix_op(prefix, postfix, decor) \
        template <typename Fn1> \
        constexpr auto operator prefix postfix(Per_Elem_Expr<Fn1>&& left) \
            requires requires(decor) { \
                prefix left.fn().val postfix; \
            } \
        { \
            left.top_level = false; \
            \
            return Per_Elem_Expr{[left]() mutable { \
                auto left_res = left.fn(); \
                return Exact_Opt(prefix std::move(left_res.val) postfix, left_res.has); \
            }}; \
        } \

    #define bin_dual_op(op, left_move, right_move) \
        template <typename Fn1, typename Fn2> \
        constexpr auto operator op(Per_Elem_Expr<Fn1>&& left, Per_Elem_Expr<Fn2>&& right) \
            requires requires() { \
                left.fn().val op right.fn().val; \
            } \
        { \
            left.top_level = false; \
            right.top_level = false; \
            \
            return Per_Elem_Expr{[left, right]() mutable { \
                auto left_res = left.fn(); \
                auto right_res = right.fn(); \
                return Exact_Opt(left_move (left_res.val) op right_move (right_res.val), left_res.has && right_res.has); \
            }}; \
        } \

    #define bin_mono_op_left(op, move_fn) \
        template <typename Other, typename Fn2> \
        constexpr auto operator op(Other&& left, Per_Elem_Expr<Fn2>&& right) \
            requires (!expr_like<Other>) && requires() { \
                std::forward<Other>(left) op right.fn().val; \
            } \
        { \
            right.top_level = false; \
            \
            return Per_Elem_Expr{[left, right]() mutable { \
                auto right_res = right.fn(); \
                return Exact_Opt(std::forward<Other>(left) op move_fn (right_res.val), right_res.has); \
            }}; \
        } \

    #define bin_mono_op_right(op, move_fn) \
        template <typename Fn1, typename Other> \
        constexpr auto operator op(Per_Elem_Expr<Fn1>&& left, Other&& right) \
            requires (!expr_like<Other>) && requires() { \
                left.fn().val op std::forward<Other>(right); \
            } \
        { \
            left.top_level = false; \
            \
            return Per_Elem_Expr{[left, right]() mutable { \
                auto left_res = left.fn(); \
                return Exact_Opt(move_fn (left_res.val) op std::forward<Other>(right), left_res.has); \
            }}; \
        } \

    #define bin_mono_op(op, move_fn) bin_mono_op_left(op, move_fn) bin_mono_op_right(op, move_fn)
    #define bin_op(op) bin_dual_op(op, std::move, std::move) bin_mono_op(op, std::move)
    #define bin_assign_op(op) bin_dual_op(op,,std::move) bin_mono_op(op,)

    #define unary_prefix_op(op) unary_mixfix_op(op,,);
    #define unary_postfix_op(op) unary_mixfix_op(,op,int);

    bin_assign_op(+=);
    bin_assign_op(-=);
    bin_assign_op(*=);
    bin_assign_op(/=);
    bin_assign_op(%=);
    bin_assign_op(&=);
    bin_assign_op(|=);
    bin_assign_op(^=);
    bin_assign_op(<<=);
    bin_assign_op(>>=);

    unary_prefix_op(++);
    unary_prefix_op(--);
    unary_postfix_op(++);
    unary_postfix_op(--);

    unary_prefix_op(+);
    unary_prefix_op(-);
    unary_prefix_op(~);
    bin_op(+);
    bin_op(-);
    bin_op(*);
    bin_op(/);
    bin_op(%);
    bin_op(&);
    bin_op(|);
    bin_op(^);
    bin_op(<<);
    bin_op(>>);

    unary_prefix_op(!);
    bin_op(&&);
    bin_op(||);

    bin_op(==);
    bin_op(!=);
    bin_op(<);
    bin_op(>);
    bin_op(<=);
    bin_op(>=);
    bin_op(<=>);

    unary_prefix_op(*);
    unary_prefix_op(&);

    template <stdr::forward_range T>
    constexpr auto make_expr(T&& range)
    {
        return Per_Elem_Expr{[&, iter = stdr::begin(range), end = stdr::end(range)]() mutable {
            auto copy = iter;
            return Exact_Opt(*copy, ++iter != end);
        }};
    }

    //Per default hooks into every single container supporting begin_it end_it
    template <stdr::forward_range T>
    constexpr auto custom_invoke(T&& range, PerElementDummy) noexcept 
    { 
        return make_expr(std::forward<T>(range)); 
    }

    
    #undef Exact_Opt
    #undef Exact_Expr

    #undef unary_mixfix_op
    #undef bin_dual_op
    #undef bin_mono_op_left
    #undef bin_mono_op_right
    #undef bin_mono_op
    #undef bin_op
    #undef bin_assign_op
    #undef unary_prefix_op
    #undef unary_postfix_op
}

#include "undefs.h"