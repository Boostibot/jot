#pragma once
#include "standards.h"
#include "defines.h"

namespace jot
{
    enum Ok_Type {OK};
    enum Error_Type {ERROR};

    template<>
    struct Hasable<State>
    {
        static constexpr func perform(State state) -> bool {
            return state == OK_STATE;
        }
    };

    template<>
    struct Hasable<bool>
    {
        static constexpr func perform(bool state) -> bool {
            return state;
        }
    };

    template <typename T>
    struct Nullable
    {
        T value;
    };

    template<typename T>
    struct Hasable<Nullable<T>>
    {
        static constexpr func perform(Nullable<T> ptr) noexcept -> bool {
            return ptr.value != nullptr;
        }
    };

    template<typename T>
    constexpr func value(Nullable<T> ptr) noexcept -> T {
        return ptr.value;
    }

    template <hasable T> func operator==(T in left, Ok_Type) noexcept -> bool { return has(left); }
    template <hasable T> func operator!=(T in left, Ok_Type) noexcept -> bool { return has(left) == false; }
    template <hasable T> func operator==(T in left, Error_Type) noexcept -> bool { return has(left) == false; }
    template <hasable T> func operator!=(T in left, Error_Type) noexcept -> bool { return has(left); }

    template <hasable T> proc force(T in value) -> void {
        if(has(value) == false)
            throw value;
    }

    template <hasable T> proc force_error(T in value) -> void {
        if(has(value))
            throw value;
    }
}
#include "undefs.h"