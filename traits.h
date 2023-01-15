#pragma once

#include <type_traits>
#include <concepts>

namespace jot 
{
    template <auto val, class T = decltype(val)>
    struct Const
    {
        using type = T;
        static constexpr T value  = val;
    };

    using True = Const<true, bool>;
    using False = Const<false, bool>;

    template<class T, class U>
    struct Is_Same : Const<false, bool> {};

    template<class T>
    struct Is_Same<T, T> : Const<true, bool> {};

    template <class T, class... Ts>
    concept same = (Is_Same<T, Ts>::value && ...); 

    template <class T, class... Ts>
    concept is_present = (Is_Same<T, Ts>::value || ...); 

    template<class T>
    concept non_void = (same<T, void> == false);

    static_assert(Is_Same<char, char>::value);
    static_assert(same<char, void> == false);
    static_assert(non_void<char>);

    template<class T>
    struct Id { using type = T; };

    template<class T>
    concept regular_type = 
        std::is_nothrow_default_constructible_v<T> && 
        std::is_nothrow_destructible_v<T> &&
        std::is_nothrow_move_constructible_v<T> && 
        std::is_nothrow_move_assignable_v<T>;

    template<class T>
    concept innert_type = regular_type<T> &&
        std::is_nothrow_copy_assignable_v<T> &&
        std::is_nothrow_copy_constructible_v<T>;

    constexpr auto is_const_eval() noexcept -> bool {
        return std::is_constant_evaluated();
    }
}
