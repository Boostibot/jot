#pragma once

#include <cstddef>
#include <cassert>
#include <utility>
#include "meta.h"
#include "span.h"
#include "defines.h"

namespace jot
{
    //Custom std::array-like array implementation
    // is identitical to std::array but access to members is done directly (because typing () is annoying)
    //Should be compatible with any algorhitms requiring begin(), end(), size() proc as they 
    // still extist but only as non members which removes the named collsions
    //Also includes slicing syntax in the form arr(5, END - 1)

    namespace detail
    {
        template<typename T, typename SizeVal, typename Size>
        struct ArrayData
        {
            static constexpr Size size = cast(Size) SizeVal::value;
            static constexpr Size capacity = size;
            T data[size];

            func operator<=>(const ArrayData&) const noexcept = default;
        };

        template<typename T, typename Size>
        struct ArrayData<T, Const<0, size_t>, Size>
        {
            static constexpr Size size = 0;
            static constexpr Size capacity = 0;
            static constexpr T data[1] = {T()};

            func operator<=>(const ArrayData&) const noexcept = default;
        };
    }


    template<typename T, size_t size, typename Size = size_t>
    struct VanishingArray : detail::ArrayData<T, Const<size, size_t>, Size>
    {
        using Tag = StaticContainerTag;
        using ArrayData = detail::ArrayData<T, Const<size>, Size>;

        constexpr VanishingArray() = default;
        constexpr VanishingArray(const T&) noexcept requires (size == 0) : ArrayData{} {}

        template <typename ...Args> 
            requires (size != 0) && (std::is_integral_v<T> == false) && are_same_v<Args...>
        constexpr VanishingArray(Args&&... args) noexcept : ArrayData{std::forward<Args>(args)...} {}

        template <typename ...Args> 
            requires (size != 0) && (std::is_integral_v<T> == true) && (std::is_integral_v<Args> && ...)
        constexpr VanishingArray(Args... args) noexcept : ArrayData{cast(T)(args) ...} {}

        constexpr operator T*()             noexcept { return this->data; }
        constexpr operator const T*() const noexcept { return this->data; }

        #include "span_array_shared_text.h"
    };

    template<typename T, size_t size_, typename Size = size_t>
    struct Array
    {
        using Tag = StaticContainerTag;
        static constexpr Size size = cast(Size) size_;
        static constexpr Size capacity = size;
        T data[size ? size : 1];


        //pure constexpr bool operator==(const Array&) const = default;
        pure constexpr bool operator==(const Array&) const noexcept requires(size != 0) = default;
        //func operator<=>(const Array&) const = default;
        func operator<=>(const Array&) const noexcept requires(size != 0) = default;
        //pure constexpr bool operator>(const Array&) const noexcept = default;
        //pure constexpr bool operator<(const Array&) const noexcept = default;

        pure constexpr bool operator==(const Array&) const noexcept requires(size == 0) { return true; };
        func operator<=>(const Array&) const noexcept requires(size == 0) { return 0; };

        constexpr operator T*() noexcept             { return this->data; }
        constexpr operator const T*() const noexcept { return this->data; }

        #include "span_array_shared_text.h"
    };

    //deduction guide
    template <class First, class... Rest>
    VanishingArray(First, Rest...) -> VanishingArray<First, 1 + sizeof...(Rest)>;

    template <class First, class... Rest>
    Array(First, Rest...) -> Array<First, 1 + sizeof...(Rest)>;

    //Adds self to span
    template <class T, size_t size, class Size>
    Span(VanishingArray<T, size, Size>) -> Span<T, Size, size>;

    template <class T, size_t size, class Size>
    Span(Array<T, size, Size>) -> Span<T, Size, size>;

    namespace detail 
    {
        template <class T, size_t N, size_t... I>
        constexpr Array<std::remove_cv_t<T>, N>
            to_array_impl(const T (&a)[N], std::index_sequence<I...>)
        {
            return { {a[I]...} };
        }

        template <class T, size_t N, size_t... I>
        constexpr Array<std::remove_cv_t<T>, N>
            to_array_impl(T (&&a)[N], std::index_sequence<I...>)
        {
            return { {std::move(a[I])...} };
        }

    }

    template <class T, size_t N>
    constexpr Array<std::remove_cv_t<T>, N> to_array(const T (&a)[N])
    {
        return detail::to_array_impl(a, std::make_index_sequence<N>{});
    }

    template <class T, size_t N>
    constexpr Array<std::remove_cv_t<T>, N> to_array(T (&&a)[N])
    {
        return detail::to_array_impl(std::move(a), std::make_index_sequence<N>{});
    }
}

#include "undefs.h"