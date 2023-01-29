#pragma once

#include <type_traits>

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
    
    enum Enabled { ENABLED = 0 };

    template<bool cond, class T>
    struct _Enable_If {};
    
    template<class T>
    struct _Enable_If<true, T> { typedef T type; };

    template<bool cond, class T = Enabled>
    using Enable_If = typename _Enable_If<cond, T>::type;

    template<class T>
    struct Id { using type = T; };

    template<class T>
    static constexpr bool regular_type = 
        std::is_nothrow_default_constructible_v<T> && 
        std::is_nothrow_destructible_v<T> &&
        std::is_nothrow_move_constructible_v<T> && 
        std::is_nothrow_move_assignable_v<T>;

    template<class T>
    static constexpr bool innert_type = regular_type<T> &&
        std::is_nothrow_copy_assignable_v<T> &&
        std::is_nothrow_copy_constructible_v<T>;
}
