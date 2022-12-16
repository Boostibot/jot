#pragma once

#include <cassert>

#include "types.h"
#include "meta.h"
#include "slice.h"
#include "array.h"
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
    func div_round_up(T value, no_infer(T) to_multiple_of) noexcept -> auto {
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

    template <typename T>
    struct Maybe
    {
        static constexpr bool has_ptr = std::is_pointer_v<T>;

        struct Ptr_Data {
            T value = T();

            constexpr Ptr_Data(bool has, T value = T())  noexcept
                : value(value) { cast(void) has; }
        };

        struct Regular_Data {
            bool has = false;
            T value = T();
        };

        static_assert(
            std::is_default_constructible_v<T> && 
            std::is_move_constructible_v<T> && 
            std::is_move_assignable_v<T> && "must be a regular type");

        using Data = std::conditional_t<has_ptr, Ptr_Data, Regular_Data>;
        Data payload;

        constexpr Maybe() noexcept
            : payload{false} {}

        constexpr Maybe(T value) noexcept
            : payload{true, move(value)} 
        {}
    };

    template <typename T>
    Maybe(T) -> Maybe<T>;

    func has(bool flag) noexcept -> bool {
        return flag;
    }

    template <typename T>
    func has(Maybe<T> in optional) noexcept -> bool 
    {
        if constexpr (Maybe<T>::has_ptr)
            return optional.payload.value == nullptr;
        else
            return optional.payload.has;
    }

    template <typename T>
    func unwrap(Maybe<T> in optional) noexcept -> T in
    {
        assert(has(optional));
        return optional.payload.value;
    }

    template <typename T>
    func unwrap(Maybe<T>* optional) noexcept -> T*
    {
        assert(has(optional));
        return &optional.payload.value;
    }

    template <typename T>
    proc swap(T* left, T* right) noexcept
    {
        T temp = move(*left);
        *left = move(*right);
        *right = move(temp);
    }

    enum class Iter_Direction
    {
        FORWARD,
        BACKWARD
    };

    template <typename T>
    using Assign_Proc = void(*)(T*, T in);
    
    struct Default_Assign {};

    template<typename T>
    struct Assign : Default_Assign
    {
        static proc perform(T* to, T in from, bool* ok) noexcept -> Unit
        {
            *to = from;
            *ok = true;
            return {};
        }
    };

    template<typename T>
    constexpr bool has_bit_by_bit_assign = std::is_base_of_v<Default_Assign, Assign<T>>
        && std::is_trivially_copyable_v<T>
        && std::is_default_constructible_v<T>;
    
    template <typename T>
    struct Assign_Result
    {
        using Error_Void = decltype(Assign<T>::perform(nullptr, T(), nullptr));
        using Error = std::conditional_t<same<Error_Void, void>, Unit, Error_Void>;

        bool ok = true;
        Error error = Error();
    };
    
    template <typename T>
    func has(Assign_Result<T> in res) noexcept -> bool {
        return res.ok;
    }

    template <typename T>
    func unwrap_error(Assign_Result<T> in res) noexcept -> Assign_Result<T>::Error in {
        assert(has(res) == false);
        return res.error;
    }

    template <typename T>
    func unwrap_error(Assign_Result<T>* res) noexcept -> Assign_Result<T>::Error* {
        assert(has(*res) == false);
        return &res.error;
    }

    template<typename Value, typename State>
    struct Result
    {
        Value value;
        State state;
    };

    template <typename Value, typename State>
    func has(Result<Value, State> in res) -> bool {
        return has(res.state);
    }

    template <typename Value, typename State>
    func unwrap(Result<Value, State> in res) -> Value in {
        assert(has(res));
        return res.error;
    }

    template <typename Value, typename State>
    func unwrap(Result<Value, State>* res) -> Value* {
        assert(has(*res));
        return &res.error;
    }

    template <typename Value, typename State>
    func unwrap_error(Result<Value, State> in res) -> State in {
        assert(has(res) == false);
        return res.error;
    }

    template <typename Value, typename State>
    func unwrap_error(Result<Value, State>* res) -> State* {
        assert(has(*res) == false);
        return &res.error;
    }

    template<typename T>
    func assign(T* to, no_infer(T) in from) -> Assign_Result<T>
    {
        bool ok = true;
        Assign_Result<T> res = {true, Assign<T>::perform(to, from, &ok)};
        res.ok = ok;
        return res;
    }

    template<typename T>
    func copy(T in other) -> Result<T, Assign_Result<T>> 
    {
        Result<T, Assign_Result<T>> res;
        res.state = assign(&res.value, other);
        return res;
    }

    template<typename T>
    proc move_construct_items(Slice<T>* to, Slice<T> from) -> void 
    {
        assert(to->size == from.size);
        assert(are_aliasing<T>(*to, from) == false);

        T* no_alias to_data = to->data;
        const T* no_alias from_data = from.data;

        for (tsize i = 0; i < from.size; i++)
            std::construct_at(to_data + i, move(from_data[i]));
    }

    template<typename T>
    proc destroy_items(Slice<T>* to) -> void 
    {
        for (tsize i = 0; i < to->size; i++)
            to->data[i].~T();
    }

    template<typename T>
    [[nodiscard]] proc assign_construct_items(Slice<T>* to, Slice<T> from) -> Assign_Result<T> 
    {
        assert(to->size == from.size);;
        assert(are_aliasing<T>(*to, from) == false);

        T* no_alias to_data = to->data;
        const T* no_alias from_data = from.data;

        for (tsize i = 0; i < from.size; i++)
        {
            std::construct_at(to_data + i);
            let res = assign(to_data, from_data);
            if(has(res) == false)
                return res;
        }

        return {};
    }
    
    template<typename T>
    [[nodiscard]] proc assign_items(Slice<T>* to, Slice<const T> from, Iter_Direction direction) -> Result<Slice<T>, Assign_Result<T>> 
    {
        assert(to->size == from.size);

        if(has_bit_by_bit_assign<T> && is_const_eval() == false)
        {
            memmove(to->data, from.data, from.size * sizeof(T));
            return {*to, {}};
        }
        
        if(direction == Iter_Direction::FORWARD)
        {
            for (tsize i = 0; i < from.size; i++)
            {
                let state = assign(to->data + i, from.data[i]);
                if(state.ok == false)
                    return {trim(*to, i), state};
            }
        }
        else
        {
            for (tsize i = from.size; i-- > 0; )
            {
                let state = assign(to->data + i, from.data[i]);
                if(state.ok == false)
                    return {trim(*to, i), state};
            }
        }

        return {*to, {}};
    }

    template<typename T>
    [[nodiscard]] proc assign_items_no_alias(Slice<T>* to, Slice<const T> from) -> Result<Slice<T>, Assign_Result<T>> 
    {
        assert(to->size == from.size);
        assert(are_aliasing<T>(*to, from) == false);

        if(has_bit_by_bit_assign<T> && is_const_eval() == false)
        {
            memcpy(to->data, from.data, from.size * sizeof(T));
            return {*to, {}};
        }

        T* no_alias to_data = to->data;
        const T* no_alias from_data = from.data;

        for (tsize i = 0; i < from.size; i++)
        {
            let state = assign(to_data + i, from_data[i]);
            if(state.ok == false)
                return {trim(*to, i), state};
        }

        return {*to, {}};
    }

}


#include "undefs.h"