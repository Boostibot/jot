#pragma once

#include "stack.h"
#include "string.h"
#include "defines.h"

namespace jot
{
    template <
        typename T, 
        size_t static_capacity_ = 0, 
        typename Size = Def_Size, 
        typename Alloc = Def_Alloc<T>, 
        typename Grow = Def_Grow<2, 0, 8>
    >
    struct String_Builder_ : Stack<T, static_capacity_, Size, Alloc, Grow>
    {   
        using allocator_type   = Alloc;
        using grow_type        = Grow;

        using stack_type = Stack<T, static_capacity_, Size, Alloc, Grow>;
        using string_type = String_<const T, Size>;
        using slice_type = String_<T, Size>;
        using const_slice_type = String_<const T, Size>;

        using Stack_Base = stack_type;

        constexpr static Size static_capacity = cast(Size) static_capacity_; 
        constexpr static bool has_static_storage = static_capacity != 0;
        constexpr static bool has_stateful_alloc = !std::is_empty_v<Alloc>;

        explicit constexpr String_Builder_(string_type string) : Stack_Base{} {}

        constexpr String_Builder_() = default;
        constexpr String_Builder_(String_Builder_&&) noexcept = default;
        constexpr String_Builder_(String_Builder_ const&) noexcept = default;

        explicit constexpr String_Builder_(Size size) : Stack_Base{size}{}

        constexpr String_Builder_(
            T* data, Size size, Size capacity, 
            Alloc alloc, const T (&static_data)[static_capacity]
        ) noexcept requires (has_static_storage)
            : Stack_Base{data, size, capacity, alloc, static_data} {}

        constexpr String_Builder_(T* data, Size size, Size capacity, Alloc alloc = Alloc()) noexcept
            : Stack_Base{data, size, capacity, alloc} {}

        constexpr String_Builder_(Alloc alloc) noexcept
            : Stack_Base{alloc} {}

        constexpr operator String_<T, Size>() noexcept 
            requires are_same_v<std::remove_const_t<T>, T>
            { return String_<T, Size>{this->data, this->size}; }

        constexpr operator string_type() const noexcept 
            { return string_type{this->data, this->size}; }

        #include "slice_op_text.h"
    };

    using String_Builder = String_Builder_<char>;
}