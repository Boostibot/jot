#pragma once

#include <cassert>

#include "types.h"
#include "meta.h"
#include "slice.h"
#include "array.h"
#include "option.h"
#include "assign.h"
#include "defines.h"

namespace jot 
{
    template<typename T>
    func max(T in first) noexcept -> T in {   
        return first;
    }

    template<typename T>
    func min(T in first) noexcept -> T in {   
        return first;
    }

    template<typename T>
    func max(T in a, no_infer(T) in b) noexcept -> T in {
        return a > b ? a : b;
    }

    template<typename T>
    func min(T in a, no_infer(T) in b) noexcept -> T in {
        return a < b ? a : b;
    }

    template<typename T>
    func div_round_up(T in value, no_infer(T) in to_multiple_of) noexcept -> T {
        return (value + to_multiple_of - 1) / to_multiple_of;
    }

    template <typename T>
    func are_aliasing(Slice<const T> left, Slice<const T> right) noexcept -> bool
    { 
        uintptr_t left_pos = cast(uintptr_t) left.data;
        uintptr_t right_pos = cast(uintptr_t) right.data;
        if(right_pos < left_pos)
        {
            //[ right ]      [ left ]
            //[ ?? right size ?? ]

            uintptr_t diff = left_pos - right_pos;
            return diff < right.size;
        }
        else
        {
            //[ left ]      [ right ]
            //[ ?? left size ?? ]
            uintptr_t diff = right_pos - left_pos;
            return diff < left.size;
        }
    }

    template <typename T>
    func are_one_way_aliasing(Slice<const T> before, Slice<const T> after) noexcept -> bool
    { 
        return (before.data + before.size > after.data) && (after.data > before.data);
    }

    enum class Iter_Direction
    {
        FORWARD,
        BACKWARD
    };

    template<typename T>
    proc move_construct_items(Slice<T>* to, Slice<const T> from) noexcept -> void 
    {
        assert(to->size == from.size);
        assert(are_aliasing<T>(*to, from) == false);

        T* no_alias to_data = to->data;
        const T* no_alias from_data = from.data;

        for (tsize i = 0; i < from.size; i++)
            std::construct_at(to_data + i, move(&from_data[i]));
    }

    template<typename T>
    proc move_assign_items(Slice<T>* to, Slice<const T> from) noexcept -> void 
    {
        assert(to->size == from.size);
        assert(are_aliasing<T>(*to, from) == false);

        T* no_alias to_data = to->data;
        const T* no_alias from_data = from.data;

        for (tsize i = 0; i < from.size; i++)
            to_data[i] = move(&from_data[i]);
    }

    template<typename T>
    proc swap_items_no_alias(Slice<T>* to, Slice<T>* from) noexcept -> void
    {
        assert(to->size == from.size);
        assert(are_aliasing<T>(*to, from) == false);

        T* no_alias to_data = to->data;
        const T* no_alias from_data = from.data;

        for (tsize i = 0; i < from.size; i++)
            swap(to_data + i, from_data + i);
    }

    template<typename T>
    proc destroy_items(Slice<T>* to) noexcept -> void 
    {
        for (tsize i = 0; i < to->size; i++)
            to->data[i].~T();
    }

    template <typename T>
    struct Slice_Assign_Error
    {
        Assign_Error<T> error = {};
        IRange succesfully_completed = {0, 0};
    };

    template<typename T>
    [[nodiscard]] proc assign_construct_items(Slice<T>* to, Slice<T> from) noexcept -> Error_Option<Slice_Assign_Error<T>> 
    {
        assert(to->size == from.size);;
        assert(are_aliasing<T>(*to, from) == false);

        T* no_alias to_data = to->data;
        const T* no_alias from_data = from.data;

        for (tsize i = 0; i < from.size; i++)
        {
            std::construct_at(to_data + i);
            let state = assign(to_data, from_data);
            if(state == Error())
                return Error{
                    Slice_Assign_Error<T>{
                        error(move(&state)), {0, i}
                    }
                };
        }

        return Value();
    }
    
    template<typename T>
    [[nodiscard]] proc assign_items(Slice<T>* to, Slice<const T> from, Iter_Direction direction) noexcept -> Error_Option<Slice_Assign_Error<T>> 
    {
        assert(to->size == from.size);

        if(has_bit_by_bit_assign<T> && is_const_eval() == false)
        {
            memmove(to->data, from.data, from.size * sizeof(T));
            return Value();
        }
        
        if(direction == Iter_Direction::FORWARD)
        {
            for (tsize i = 0; i < from.size; i++)
            {
                let state = assign(to->data + i, from.data[i]);
                if(state == Error())
                    return Error{
                        Slice_Assign_Error<T>{
                            error(move(&state)), {0, i}
                        }
                    };
            }
        }
        else
        {
            for (tsize i = from.size; i-- > 0; )
            {
                let state = assign(to->data + i, from.data[i]);
                if(state == Error())
                    return Error{
                        Slice_Assign_Error<T>{
                            error(move(&state)), {i, from.size}
                        }
                    };
            }
        }

        return Value();
    }

    template<typename T>
    [[nodiscard]] proc assign_items_no_alias(Slice<T>* to, Slice<const T> from) noexcept -> Error_Option<Slice_Assign_Error<T>> 
    {
        assert(to->size == from.size);
        assert(are_aliasing<T>(*to, from) == false);

        if(has_bit_by_bit_assign<T> && is_const_eval() == false)
        {
            memcpy(to->data, from.data, from.size * sizeof(T));
            return Value();
        }

        T* no_alias to_data = to->data;
        const T* no_alias from_data = from.data;

        for (tsize i = 0; i < from.size; i++)
        {
            let state = assign(to_data + i, from_data[i]);
            
            if(state == Error())
                return Error{
                    Slice_Assign_Error<T>{
                        error(move(&state)), {0, i}
                    }
                };
        }

        return Value();
    }


}


#include "undefs.h"