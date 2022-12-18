#pragma once

#include <type_traits>
#include <concepts>

namespace jot 
{
    using std::integral;
    using std::floating_point;

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
    struct Consume {};

    template<class T>
    struct Id { using type = T; };

    #if 0
    namespace detail
    {
        template<class T>
        struct Void_Escaper
        {
            using type = T;
        };

        template<>
        struct Void_Escaper<void>
        {
            using type = Unit;
        };
    }

    template<class T>
    using Escape_Void = detail::Void_Escaper<T>::type;
    #endif

    //stops infering of arguments
    template<typename T>
    using No_Infer = Id<T>::type;
    #define no_infer(...) No_Infer<__VA_ARGS__> 

    namespace example
    {
        template <typename T>
        void take_two(T a, no_infer(T) b)
        {
            (void) a; (void) b;
        };

        //take_two(1.0, 1); //without No_Infer doesnt compile
    }

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

    //A lightweight compile time only tuple used for storing types
    template<class... Types >
    struct type_collection;

    namespace detail 
    {
        //template<int I, class T>
        //struct tuple_element_impl;

        template<int I, class Head, class... Tail >
        struct tuple_element_impl : tuple_element_impl<I-1, type_collection<Tail...>> { };

        template<class Head, class... Tail >
        struct tuple_element_impl<0, type_collection<Head, Tail...>> {
            using type = Head;
        };
    }

    template<int I, class T>
    struct tuple_element;

    template<int I, class... Types >
    struct tuple_element<I, type_collection<Types...>>
    {
        static_assert(I < sizeof...(Types), "index must be in range");
        using type = typename detail::tuple_element_impl<I, type_collection<Types...>>::type;
    };

    template< class What, class T>
    struct tuple_has : False {};

    template< class What, class... Types >
    struct tuple_has<What, type_collection<Types...>> : Const<is_present<What, Types...>, bool> {};

    template<int I, class T>
    using tuple_element_t = typename tuple_element<I, T>::type;

    template<int I, class... Types>
    using nth_type = typename tuple_element<I, type_collection<Types...>>::type;

    template<class... Types >
    struct type_collection 
    {
        template<int I>
        using get = nth_type<I, Types...>;
        static constexpr int size = sizeof...(Types);

        template <class What>
        static constexpr bool has = is_present<What, Types...>;
    };

    constexpr auto is_const_eval() noexcept -> bool 
    {
        return std::is_constant_evaluated();
    }


}
