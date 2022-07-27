#pragma once

#include "stack.h"
#include "defines.h"

namespace jot
{
    //Specifies settings to the Stack struct 
    // All values set to -1 are null values and skipped
    // Clamps the specified number of elements in each categroy (so alloc_elems and static_elems) by the min max byte sizes for each and uses the result
    // The to_byte_size value works by specifying the total size of the resulting Stack struct and 
    //  filling all availible space with static elems. This value only takes efect if static_elems is not specified
    // The growth_mult is standard grwoth factor for the storage while growth_add is linear growth added to the
    //  result of using growth_mult on each allocation (new_space = old_space * growth_mult + growth_add)
    // The alloc_growth can be used to supply own implementation of growth function

    //This sounds very complex but in practice is really simple. The default values can be read as follows:
    // "I would ideally like allocated 8 elements or enough elements for 64 bytes"
    // and "I would like the struct to take up 64 bytes and use any not needed space as storage"
    // and "On each allocation the avalible storage space should double"
    struct Stack_Settings
    {
        i64 static_elems        = -1;
        i64 static_bytes_min    = -1;
        i64 static_bytes_max    = -1;

        i64 to_byte_size        = -1;

        i64 alloc_elems         = -1;
        i64 alloc_bytes_min     = -1;
        i64 alloc_bytes_max     = -1;

        i64 growth_mult   = -1;
        i64 growth_add    = -1;
    };

    constexpr Stack_Settings DEF_STACK_SETTINGS = {
        .static_elems = 0,
        .alloc_elems = 8,
        .growth_mult = 2,
        .growth_add = 0,
    };

    namespace detail 
    {
        func def_val_clamp(i64 val, i64 min, i64 max) -> i64
        {
            if(val < min && min != -1)
                return min;
            if(val > max && max != -1)
                return max;
            else
                return val;
        }

        func calc_static_size(size_t elem_size, size_t base_size, Stack_Settings stngs) -> size_t
        {
            if(stngs.to_byte_size >= 0)
            {
                let remaining = stngs.to_byte_size - cast(i64) base_size;
                if(remaining < 0)
                    return cast(size_t) DEF_STACK_SETTINGS.static_elems;
                else
                    return cast(size_t) (remaining - remaining % elem_size) / elem_size;
            }
            else if(stngs.static_elems >= 0)
            {
                return cast(size_t) def_val_clamp(
                    stngs.static_elems * elem_size, 
                    stngs.static_bytes_min, 
                    stngs.static_bytes_max);
            }
            else
            {
                return cast(size_t) DEF_STACK_SETTINGS.static_elems;
            }

        }

        func calc_alloc_size(size_t elem_size, Stack_Settings stngs) -> size_t
        {
            if(stngs.alloc_elems < 0)
                return cast(size_t) DEF_STACK_SETTINGS.alloc_elems;

            return cast(size_t) def_val_clamp(
                stngs.alloc_elems * elem_size, 
                stngs.alloc_bytes_min, 
                stngs.alloc_bytes_max);
        }

        func calc_growth_mult(Stack_Settings stngs) -> size_t
        {
            if(stngs.growth_mult < 0)
                return cast(size_t) DEF_STACK_SETTINGS.growth_mult;
            return cast(size_t) stngs.growth_mult;
        }

        func calc_growth_add(Stack_Settings stngs) -> size_t
        {
            if(stngs.growth_add < 0)
                return cast(size_t) DEF_STACK_SETTINGS.growth_add;
            return cast(size_t) stngs.growth_add;
        }

        template<size_t elem_size, Stack_Settings stngs, typename Grow = void>
        using Calc_Grow = std::conditional_t<are_same_v<Grow, void>,
            Def_Grow<calc_growth_mult(stngs), calc_growth_add(stngs), calc_alloc_size(elem_size, stngs)>,
            Grow
        >;

        template <typename T, typename Size, typename Alloc>
        struct Base_Size {
            using Stack_T = Stack_Data<T, Size, Alloc, 1>;
            static constexpr size_t value = offsetof(Stack_T, static_data_);
        };
    }

    template <
        typename T, 
        Stack_Settings settings, 
        typename Size = size_t, 
        typename Alloc = Def_Alloc<T>, 
        typename Grow = void>
    using Custom_Stack = Stack<
        T, 
        detail::calc_static_size(sizeof(T), detail::Base_Size<T, Size, Alloc>::value, settings), 
        Size, 
        Alloc,
        detail::Calc_Grow<sizeof(T), settings, Grow>
    >;

    namespace example 
    {
        void example_fn()
        {
            //stack that fills 64 bytes and grows lineary allocating 128 new elems at a time
            constexpr let settings = Stack_Settings{
                .to_byte_size = 64,
                .alloc_elems = 128,
                .growth_mult = 0,
                .growth_add = 128,
            };

            Custom_Stack<int, settings> stack;

            static_assert(sizeof(Stack<int, 10>) == 64);
            constexpr let base_size = detail::Base_Size<int, size_t, Def_Alloc<int>>::value;
            static_assert(base_size == 24);

            //static_assert(detail::calc_static_size(sizeof(int), base_size, settings) == 10);

            //static_assert(sizeof(stack) == 64);
        }
    }
}

#include "undefs.h"