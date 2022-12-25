#pragma once
#include "traits.h"

namespace jot
{
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
}