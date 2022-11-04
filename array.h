#pragma once

#include <cstddef>
#include <cassert>
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
    template<typename T, size_t size_, typename Size = Def_Size>
    struct Array_
    {
        using tag_type = Static_Container_Tag;
        using slice_type = Slice_<T, Size>;
        using const_slice_type = Slice_<const T, Size>;

        static constexpr Size size = cast(Size) size_;
        static constexpr Size capacity = size;
        T data[size ? size : 1];

        pure constexpr bool operator==(const Array_&) const noexcept requires(size != 0) = default;
        func operator<=>(const Array_&) const noexcept requires(size != 0) = default;

        pure constexpr bool operator==(const Array_&) const noexcept requires(size == 0) { return true; };
        func operator<=>(const Array_&) const noexcept requires(size == 0) { return 0; };

        constexpr operator T*() noexcept             { return this->data; }
        constexpr operator const T*() const noexcept { return this->data; }

        #include "slice_op_text.h"

    };

    //deduction guide
    template <class First, class... Rest>
    Array_(First, Rest...) -> Array_<First, 1 + sizeof...(Rest)>;

    //Adds self to slice
    //template <class T, size_t size, class Size>
    //Slice_(Array_<T, size, Size>) -> Slice_<T, Size, size>;

    namespace detail 
    {
        template <class T, size_t N, size_t... I>
        constexpr Array_<std::remove_cv_t<T>, N>
            to_array_impl(const T (&a)[N], std::index_sequence<I...>)
        {
            return { {a[I]...} };
        }

        template <class T, size_t N, size_t... I>
        constexpr Array_<std::remove_cv_t<T>, N>
            to_array_impl(T (&&a)[N], std::index_sequence<I...>)
        {
            return { {std::move(a[I])...} };
        }

    }

    template <class T, size_t N>
    func to_array(const T (&a)[N]) -> Array_<std::remove_cv_t<T>, N>
    {
        return detail::to_array_impl(a, std::make_index_sequence<N>{});
    }

    template <class T, size_t N>
    func to_array(T (&&a)[N]) -> Array_<std::remove_cv_t<T>, N>
    {
        return detail::to_array_impl(std::move(a), std::make_index_sequence<N>{});
    }
}

namespace std
{
    #define templ template <class T, size_t size, class Size>
    template <class T, size_t size, class Size>
    func begin(jot::Array_<T,size, Size>& arr) noexcept {return arr.data;}
    template <class T, size_t size, class Size>
    func begin(const jot::Array_<T,size, Size>& arr) noexcept {return arr.data;}

    template <class T, size_t size, class Size>
    func end(jot::Array_<T,size, Size>& arr) noexcept {return arr.data + arr.size;}
    template <class T, size_t size, class Size>
    func end(const jot::Array_<T,size, Size>& arr) noexcept {return arr.data + arr.size;}

    template <class T, size_t size, class Size>
    func cbegin(const jot::Array_<T,size, Size>& arr) noexcept {return arr.data;}
    template <class T, size_t size, class Size>
    func cend(const jot::Array_<T,size, Size>& arr) noexcept {return arr.data + arr.size;}

    template <class T, size_t _size, class Size>
    func size(const jot::Array_<T,_size, Size>& arr) noexcept {return arr.size;}
}

#include "undefs.h"