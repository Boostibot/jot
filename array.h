#pragma once

#include "meta.h"
#include "types.h"
#include "slice.h"
#include "defines.h"

namespace jot
{
    template<typename T, isize size_>
    struct Array
    {
        static constexpr isize size = size_;
        static constexpr isize capacity = size;
        T data[size > 0 ? size : 1];

        nodisc constexpr bool operator==(const Array&) const noexcept requires(size != 0) = default;
        constexpr func operator<=>(const Array&) const noexcept requires(size != 0) = default;

        nodisc constexpr bool operator==(const Array&) const noexcept requires(size == 0) { return true; };
        constexpr func operator<=>(const Array&) const noexcept requires(size == 0) { return 0; };

        constexpr operator T*() noexcept             { return this->data; }
        constexpr operator const T*() const noexcept { return this->data; }

        #include "slice_op_text.h"

    };

    //deduction guide
    template <class First, class... Rest>
    Array(First, Rest...) -> Array<First, 1 + sizeof...(Rest)>;

    template<typename Container>
    concept static_direct_container = requires(Container container)
    {
        { container.data } -> ::std::convertible_to<void*>;
        { Container::size } -> ::std::convertible_to<isize>;
        requires(!same<decltype(container.data), void*>);
    };

    template <class T, isize N>
    constexpr func slice(Array<T, N>* array) -> Slice<T> {
        return Slice<T>{array->data, N};
    }

    template <class T, isize N, typename Size>
    constexpr func slice(Array<T, N> in array) -> Slice<const T> {
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
    constexpr func get(Cont& arr) noexcept -> typename Cont::value_type&
    {
        static_assert(I < arr.size, "access out of bounds");
        return arr[I];
    }

    template<size_t I, jot::static_direct_container Cont>
    constexpr func get(Cont&& arr) noexcept -> typename Cont::value_type&&
    {
        static_assert(I < arr.size, "access out of bounds");
        return move(arr[I]);
    }

    template<size_t I, jot::static_direct_container Cont>
    constexpr func get(const Cont& arr) noexcept -> const typename Cont::value_type&
    {
        static_assert(I < arr.size, "access out of bounds");
        return arr[I];
    } 

    template<size_t I, jot::static_direct_container Cont>
    constexpr func get(const Cont&& arr) noexcept -> const typename Cont::value_type&&
    {
        static_assert(I < arr.size, "access out of bounds");
        return move(arr[I]);
    }

    template<class T, jot::static_direct_container Cont>
    constexpr func get(Cont& arr) noexcept -> T&
    {
        static_assert(is_same_v<T, typename Cont::value_type> && arr.size == 1, "exactly one element needs to satisfy the type");
        return arr[0];
    }
    template<class T, jot::static_direct_container Cont>
    constexpr func get(Cont&& arr) noexcept -> T&&
    {
        static_assert(is_same_v<T, typename Cont::value_type> && arr.size == 1, "exactly one element needs to satisfy the type");
        return move(arr[0]);
    }

    template<class T, jot::static_direct_container Cont>
    constexpr func get(const Cont& arr) noexcept -> const T&
    {
        static_assert(is_same_v<T, typename Cont::value_type> && arr.size == 1, "exactly one element needs to satisfy the type");
        return arr[0];
    }
    template<class T, jot::static_direct_container Cont>
    constexpr func get(const Cont&& arr) noexcept -> const T&&
    {
        static_assert(is_same_v<T, typename Cont::value_type> && arr.size == 1, "exactly one element needs to satisfy the type");
        return move(arr[0]);
    }
}


#include "undefs.h"