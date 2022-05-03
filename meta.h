#pragma once

#include <type_traits>
#include <concepts>

namespace jot 
{
    template<class T >
    struct dummy {};

    using std::integral_constant;
    using std::true_type;
    using std::false_type;

    template <auto val, class T = decltype(val)>
    using Const = std::integral_constant<T, val>;

    namespace detail 
    {
        template <typename AlwaysVoid, typename... Ts>
        struct has_common_type_impl : std::false_type {};

        template <typename... Ts>
        struct has_common_type_impl<std::void_t<std::common_type_t<Ts...>>, Ts...> : std::true_type {};
    }

    template <typename... Ts>
    using has_common_type = typename detail::has_common_type_impl<void, Ts...>::type; 

    template <typename... Ts>
    static constexpr bool has_common_type_v = has_common_type<Ts...>::value; 

    template <typename... Ts>
    struct common_type 
    {
        constexpr static bool value = has_common_type<Ts...>::value;
        using type = typename std::conditional<value, std::common_type<Ts...>, std::type_identity<void>>::type::type;
    };

    template <class T, class... Ts>
    struct are_same : std::bool_constant<(std::is_same_v<T, Ts> && ...)> {};

    template <class T, class... Ts>
    static constexpr bool are_same_v = are_same<T, Ts...>::value; 

    template<class... Types >
    struct type_collection;

    namespace detail 
    {
        template<size_t I, class T >
        struct tuple_element_impl;

        template<size_t I, class Head, class... Tail >
        struct tuple_element_impl<I, type_collection<Head, Tail...>>
            : tuple_element_impl<I-1, type_collection<Tail...>> { };

        template<class Head, class... Tail >
        struct tuple_element_impl<0, type_collection<Head, Tail...>> {
            using type = Head;
        };
    }

    template<size_t I, class T >
    struct tuple_element;

    template< size_t I, class... Types >
    struct tuple_element<I, jot::type_collection<Types...>>
    {
        static_assert(I < sizeof...(Types), "Index must be in range");
        using type = typename jot::detail::tuple_element_impl<I, jot::type_collection<Types...>>::type;
    };

    template< class What, class T>
    struct tuple_has : std::false_type {};
    
    template< class What, class... Types >
    struct tuple_has<What, jot::type_collection<Types...>> : std::bool_constant<(std::is_same_v<What, Types> || ...)> {};

    template< size_t I, class T >
    using tuple_element_t = typename tuple_element<I, T>::type;

    namespace detail 
    {
        template<size_t I, typename Fallback, bool safe, typename... Types>
        struct safe_tuple_element 
        {
            using type = tuple_element_t<I, type_collection<Types...>>;
        };

        template<size_t I, typename Fallback, typename... Types>
        struct safe_tuple_element<I, Fallback, false, Types...>
        {
            using type = Fallback;
        };
    }

    template<size_t I, class T, typename Fallback>
    struct tuple_element_or {using type = Fallback;};

    template< size_t I, class... Types, typename Fallback >
    struct tuple_element_or<I, jot::type_collection<Types...>, Fallback> : 
        detail::safe_tuple_element<I, Fallback, I < sizeof...(Types), Types...> {};

    template< size_t I, class T, typename Fallback >
    using tuple_element_or_t = typename tuple_element_or<I, T, Fallback>::type;

    template<size_t I, class... Types>
    using nth_type = typename tuple_element<I, type_collection<Types...>>::type;

    template<class... Types >
    struct type_collection 
    {
        template<size_t I>
        using get = nth_type<I, Types...>;
        static constexpr size_t size = sizeof...(Types);

        template <class What>
        static constexpr bool has = (std::is_same_v<What, Types> || ...);
    };

    struct NoTag {};

    struct UnsetTag
    {
        using type = NoTag;
        static constexpr bool value = false; 
    };

    template <class Class, class Tag>
    struct TagRegister : UnsetTag {};

    template <class T, class With>
    concept ContainsTag = requires(T)
    {
        requires(tuple_has<T, With>::value);
    };

    template <class T, class With = NoTag>
    concept DirectTagged = requires(T)
    {
        requires(std::is_same_v<typename T::Tag, With> //Has the check directly
                 || std::is_same_v<With, NoTag>  //No checking required
                 || ContainsTag<T, With> //Is type collection with type
        );
    };

    template <class T, class With = NoTag>
    concept Tagged = 
        (std::is_same_v<typename TagRegister<T, With>::type, NoTag> && DirectTagged<T, With>) || 
        (!std::is_same_v<typename TagRegister<T, With>::type, NoTag> && TagRegister<T, With>::value);
}

namespace std
{
    template< size_t I, class... Types >
    struct tuple_element< I, jot::type_collection<Types...> > : jot::tuple_element<I, Types...> {};
}



