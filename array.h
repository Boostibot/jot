#pragma once

#include "meta.h"
#include "types.h"
#include "slice.h"
#include "defines.h"

namespace jot
{
    //Custom minimal std::array-like array implementation
    //   is identitical to std::array but access to members is done directly (because typing () is annoying)
    //Should be compatible with any algorhitms requiring begin(), end(), size() proc as they 
    //   still extist but only as non members which removes the named collsions (defined generically inside span.h)
    //Also includes slicing syntax in the form arr(5, END - 1)
    template<typename T, tsize size_>
    struct Array
    {
        using Size = tsize;
        static constexpr tsize size = size_;
        static constexpr tsize capacity = size;
        T data[size > 0 ? size : 1];

        nodisc constexpr bool operator==(const Array&) const noexcept requires(size != 0) = default;
        func operator<=>(const Array&) const noexcept requires(size != 0) = default;

        nodisc constexpr bool operator==(const Array&) const noexcept requires(size == 0) { return true; };
        func operator<=>(const Array&) const noexcept requires(size == 0) { return 0; };

        constexpr operator T*() noexcept             { return this->data; }
        constexpr operator const T*() const noexcept { return this->data; }

        #include "slice_op_text.h"

    };

    //deduction guide
    template <class First, class... Rest>
    Array(First, Rest...) -> Array<First, 1 + sizeof...(Rest)>;

    namespace detail 
    {
        template <class T, tsize N, tsize... I>
        constexpr Array<std::remove_cv_t<T>, N>
            to_array_impl(const T (&a)[N], std::index_sequence<I...>)
        {
            return { {a[I]...} };
        }

        template <class T, tsize N, tsize... I>
        constexpr Array<std::remove_cv_t<T>, N>
            to_array_impl(T (&&a)[N], std::index_sequence<I...>)
        {
            return { {std::move(a[I])...} };
        }

    }

    template <class T, tsize N>
    func to_array(const T (&a)[N]) -> Array<std::remove_cv_t<T>, N> {
        return detail::to_array_impl(a, std::make_index_sequence<N>{});
    }

    template <class T, tsize N>
    func to_array(T (&&a)[N]) -> Array<std::remove_cv_t<T>, N> {
        return detail::to_array_impl(std::move(a), std::make_index_sequence<N>{});
    }

    template<typename Container>
    concept static_direct_container = requires(Container container)
    {
        { container.data } -> ::std::convertible_to<void*>;
        { Container::size } -> ::std::convertible_to<tsize>;
        requires(!same<decltype(container.data), void*>);
    };

    template <class T, tsize N>
    func slice(Array<T, N>* array) -> Slice<T> {
        return Slice<T>{array->data, N};
    }

    template <class T, tsize N, typename Size>
    func slice(Array<T, N> in array) -> Slice<const T> {
        return Slice<const T>{array->data, N};
    }
}

namespace std
{
    template<jot::static_direct_container Cont>
    struct tuple_size<Cont> : integral_constant<size_t, Cont::size> {};

    template<size_t I, jot::static_direct_container Cont>
    struct tuple_element<I, Cont> 
    {using type = typename Cont::value_type;};

    template<size_t I, jot::static_direct_container Cont>
    func get(Cont& arr) noexcept -> typename Cont::value_type&
    {
        static_assert(I < arr.size, "access out of bounds");
        return arr[I];
    }

    template<size_t I, jot::static_direct_container Cont>
    func get(Cont&& arr) noexcept -> typename Cont::value_type&&
    {
        static_assert(I < arr.size, "access out of bounds");
        return move(arr[I]);
    }

    template<size_t I, jot::static_direct_container Cont>
    func get(const Cont& arr) noexcept -> const typename Cont::value_type&
    {
        static_assert(I < arr.size, "access out of bounds");
        return arr[I];
    } 

    template<size_t I, jot::static_direct_container Cont>
    func get(const Cont&& arr) noexcept -> const typename Cont::value_type&&
    {
        static_assert(I < arr.size, "access out of bounds");
        return move(arr[I]);
    }

    template<class T, jot::static_direct_container Cont>
    func get(Cont& arr) noexcept -> T&
    {
        static_assert(is_same_v<T, typename Cont::value_type> && arr.size == 1, "exactly one element needs to satisfy the type");
        return arr[0];
    }
    template<class T, jot::static_direct_container Cont>
    func get(Cont&& arr) noexcept -> T&&
    {
        static_assert(is_same_v<T, typename Cont::value_type> && arr.size == 1, "exactly one element needs to satisfy the type");
        return move(arr[0]);
    }

    template<class T, jot::static_direct_container Cont>
    func get(const Cont& arr) noexcept -> const T&
    {
        static_assert(is_same_v<T, typename Cont::value_type> && arr.size == 1, "exactly one element needs to satisfy the type");
        return arr[0];
    }
    template<class T, jot::static_direct_container Cont>
    func get(const Cont&& arr) noexcept -> const T&&
    {
        static_assert(is_same_v<T, typename Cont::value_type> && arr.size == 1, "exactly one element needs to satisfy the type");
        return move(arr[0]);
    }
}


#include "undefs.h"