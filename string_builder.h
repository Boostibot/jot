#pragma once

#include "stack.h"
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

        explicit constexpr String_Builder_(string_type string) : Stack_Base{}

        constexpr String_Builder_() = default;
        constexpr String_Builder_(Stack&& other) noexcept { swap(this, &other); }
        explicit constexpr String_Builder_(Size size) { alloc_data(size); }

        constexpr Stack(
            T* data, Size size, Size capacity, 
            Alloc alloc, const T (&static_data)[static_capacity]
        ) noexcept requires (has_static_storage)
            : Stack_Data{alloc, data, size, capacity} 
        {
            for(Size i = 0; i < static_capacity; i++)
                this->static_data()[i] = static_data[i];
        }

        constexpr Stack(T* data, Size size, Size capacity, Alloc alloc = Alloc()) noexcept
            : Stack_Data{alloc, data, size, capacity} {}

        constexpr Stack(Alloc alloc) noexcept
            : Stack_Data{alloc} {}

        constexpr Stack(const Stack& other)
            requires (std::is_copy_constructible_v<T>) : Stack_Data{other.alloc()}
        {
            alloc_data(this, other.size);
            copy_construct_elems(this, other);
            this->size = other.size;
        }

        constexpr ~Stack() noexcept
        {
            destroy_elems(this);
            dealloc_data(this);
        }
    };

    using String_Builder = String_Builder_<char>;

    String_Builder builder;
}