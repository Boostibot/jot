#pragma once

#include "slice.h"
#include "defines.h"

namespace jot
{
    func strlen(cstring str) noexcept 
    {
        size_t size = 0;
        while(str[size++] != '\0');
        return size;
    };


    template<typename T, typename Size = size_t, auto extent = DYNAMIC_EXTENT>
    struct String_ : Slice<T, Size, extent>
    {
        using Slice_Base = Slice<T, Size, extent>;
        using slice_type = String_<T, Size>;
        using const_slice_type = String_<const T, Size>;

        constexpr String_() = default;


        template <typename T> 
        concept literal_compatible = are_same_v<T, const char8_t> || are_same_v<T, const char>;

        static constexpr bool is_literal_compatible = literal_compatible<T>;

        constexpr String_(cstring str) noexcept
            requires (is_literal_compatible)
            : Slice_Base{str, strlen(str)} {}

        constexpr String_(const Slice_Base& slice) noexcept 
            : Slice_Base{slice} {}

        //Dynamic
        template <detail::cont_iter It> 
            requires (!is_static) && detail::matching_iter<It, T>
        constexpr Slice(It it, Size size) noexcept
            : Slice_Base{ it, size } {};

        template <detail::cont_iter Begin, detail::cont_iter End>  
            requires (!is_static) && detail::matching_iter<Begin, T>
        constexpr Slice(Begin begin, End end) noexcept
            : Slice_Base{ begin, end} {};

        template <detail::cont_range R> 
            requires (!is_static) && detail::matching_range<R, T>
        constexpr Slice(R& range) noexcept
            : Slice_Base{ range } {};

        //Static
        template <detail::cont_iter It> 
            requires (is_static) && detail::matching_iter<It, T>
        constexpr Slice(It it) noexcept
            : Slice_Base{ it } {};

        template <detail::cont_range R> 
            requires (is_static) && detail::matching_range<R, T>
        constexpr Slice(R& range) noexcept
            : Slice_Base{ range } {};

        template <detail::cont_iter Begin, detail::cont_iter End>  
            requires (is_static) && detail::matching_iter<Begin, T>
        constexpr Slice(Begin begin, End end) noexcept
            : Slice_Base{ begin, end } {};


        constexpr operator Slice<T, Size, extent>() const noexcept 
            { return Slice<T, Size>{this->data, this->size}; }

        constexpr operator String_<T, Size>() const noexcept requires (is_static) 
            { return String_<T, Size>{this->data, this->size}; }

        #include "slice_op_text.h"
    };


    using String = String_<const char>;
}


#include "undefs.h"