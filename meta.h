#pragma once

#include <type_traits>
#include <concepts>

namespace jot 
{
    using std::integral_constant;
    using std::true_type;
    using std::false_type;
    using std::integral;
    using std::floating_point;


    template<class T>
    concept non_void = (std::is_same_v<T, void> == false);

    template<class T>
    struct dummy {};

    template <auto val, class T = decltype(val)>
    using Const = std::integral_constant<T, val>;

    //stops infering of arguments
    template<typename T>
    using No_Infer = std::type_identity<T>::type;
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

    namespace detail 
    {
        template <typename AlwaysVoid, typename... Ts>
        struct has_common_type_impl : false_type {};

        template <typename... Ts>
        struct has_common_type_impl<std::void_t<std::common_type_t<Ts...>>, Ts...> : true_type {};
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
    concept same = (std::is_same_v<T, Ts> && ...); 

    //A lightweight compile time only tuple used for storing types
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
    struct tuple_element<I, type_collection<Types...>>
    {
        static_assert(I < sizeof...(Types), "Index must be in range");
        using type = typename jot::detail::tuple_element_impl<I, type_collection<Types...>>::type;
    };

    template< class What, class T>
    struct tuple_has : false_type {};
    
    template< class What, class... Types >
    struct tuple_has<What, type_collection<Types...>> : std::bool_constant<(std::is_same_v<What, Types> || ...)> {};

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
    struct tuple_element_or<I, type_collection<Types...>, Fallback> : 
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

    //Tagging -- simplify!
    struct No_Tag {};

    struct UnsetTag
    {
        using type = No_Tag;
        static constexpr bool value = false; 
    };

    template <class Class, class tag_type>
    struct Tag_Register : UnsetTag {};

    template <class T, class With>
    concept Contains_Tag = requires(T)
    {
        requires(tuple_has<T, With>::value);
    };

    template <class T, class With>
    concept Direct_Tagged = requires(T)
    {
        requires(std::is_same_v<typename T::tag_type, With> //Has the check directly
                 || Contains_Tag<T, With> //Is type collection with type
                 || std::derived_from<T, With> //is derived from
        );
    };

    template <class T, class With>
    concept Tagged = 
        (std::is_same_v<typename Tag_Register<T, With>::type, No_Tag> && Direct_Tagged<T, With>) || 
        (!std::is_same_v<typename Tag_Register<T, With>::type, No_Tag> && Tag_Register<T, With>::value);


    //If provided arguments is array decays it into ptr else forwards it
    template<typename T, size_t N>
    constexpr auto array_decay(T (&x)[N]) -> T*
    {
        return x;
    }

    template <class _Ty>
    constexpr _Ty&& array_decay(std::remove_reference_t<_Ty>& _Arg) noexcept 
    { // forward an lvalue as either an lvalue or an rvalue
        return static_cast<_Ty&&>(_Arg);
    }

    template <class _Ty>
    constexpr _Ty&& array_decay(std::remove_reference_t<_Ty>&& _Arg) noexcept 
    { // forward an rvalue as an rvalue
        return static_cast<_Ty&&>(_Arg);
    }

}