#pragma once
#include "traits.h"
#include "open_enum.h"
#include "defines.h"

open_enum Open_Enum
{
    OPEN_ENUM_ENTRY(OK);
}

namespace jot
{
    using State = ::Open_Enum::Type;
    constexpr State OK_STATE = ::Open_Enum::OK;

    template <typename T, typename Enable = True>
    struct Assignable
    {
        static constexpr func perform(T* to, T in from) -> State {
            static_assert(std::is_nothrow_copy_assignable_v<T>, "must be nothrow copyable! If not provide a custom overload of this function");
            *to = from;

            return OK_STATE;
        };
    };

    template <typename T, typename Enable = True>
    struct Swappable
    {
        static constexpr func perform(T* left, T* right) -> void {
            T temp = move(left);
            *left = move(right);
            *right = move(&temp);
        };
    };

    struct No_Default {};
    template <typename T, typename Enable = True>
    struct Failable : No_Default
    {
        #if 0
        func perform(T in flag) noexcept -> bool {
            return false;
        }
        #endif
    };

    template<typename T>
    concept failable = !std::is_base_of_v<No_Default, Failable<T>>;

    template <typename T>
    constexpr proc assign(T* to, T in from) noexcept -> State {
        return Assignable<T>::perform(to, from);
    }

    template <typename T>
    constexpr proc swap(T* left, T* right) noexcept -> void {
        return Swappable<T>::perform(left, right);
    }

    template <failable T>
    constexpr func failed(T in flag) -> bool {
        return Failable<T>::perform(flag);
    }

    template<typename T>
    constexpr func construct_assign_at(T* to, no_infer(T) in from) -> State
    {
        std::construct_at(to);
        return assign<T>(to, from);
    }

}

#include "undefs.h"