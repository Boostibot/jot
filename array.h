#pragma once

#include <cstddef>
#include <cassert>
#include <utility>
#include "meta.h"
#include "slice.h"
#include "defines.h"

namespace jot
{
    //Custom minimal std::array-like array implementation
    //   is identitical to std::array but access to members is done directly (because typing () is annoying)
    //Should be compatible with any algorhitms requiring begin(), end(), size() proc as they 
    //   still extist but only as non members which removes the named collsions (defined generically inside span.h)
    //Also includes slicing syntax in the form arr(5, END - 1)
    template<typename T, size_t size_, typename Size = size_t>
    struct Array
    {
        using Tag = StaticContainerTag;
        using slice_type = Slice<T, Size>;
        using const_slice_type = Slice<const T, Size>;

        static constexpr Size size = cast(Size) size_;
        static constexpr Size capacity = size;
        T data[size ? size : 1];

        pure constexpr bool operator==(const Array&) const noexcept requires(size != 0) = default;
        func operator<=>(const Array&) const noexcept requires(size != 0) = default;

        pure constexpr bool operator==(const Array&) const noexcept requires(size == 0) { return true; };
        func operator<=>(const Array&) const noexcept requires(size == 0) { return 0; };

        constexpr operator T*() noexcept             { return this->data; }
        constexpr operator const T*() const noexcept { return this->data; }

        #include "span_array_shared_text.h"
    };

    //deduction guide
    template <class First, class... Rest>
    Array(First, Rest...) -> Array<First, 1 + sizeof...(Rest)>;

    //Adds self to slice
    template <class T, size_t size, class Size>
    Slice(Array<T, size, Size>) -> Slice<T, Size, size>;

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