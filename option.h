#pragma once
#include "standards.h"
#include "defines.h"

#ifdef ERROR
    #undef ERROR
#endif // ERROR

#ifdef OK
    #undef OK
#endif // OK

namespace jot
{
    enum Ok_Type {OK};
    enum Error_Type {ERROR};

    template<>
    struct Failable<State>
    {
        static constexpr func perform(State state) -> bool {
            return state != OK_STATE;
        }
    };

    template<>
    struct Failable<bool>
    {
        static constexpr func perform(bool state) -> bool {
            return state == false;
        }
    };

    template <typename T>
    struct Nullable
    {
        T value;
    };

    template<typename T>
    struct Failable<Nullable<T>>
    {
        static constexpr func perform(Nullable<T> ptr) noexcept -> bool {
            return ptr.value == nullptr;
        }
    };

    template<typename T>
    constexpr func value(Nullable<T> ptr) noexcept -> T {
        return ptr.value;
    }

    template <failable T> func operator==(T in left, Ok_Type) noexcept -> bool { return failed(left) == false; }
    template <failable T> func operator!=(T in left, Ok_Type) noexcept -> bool { return failed(left); }
    template <failable T> func operator==(T in left, Error_Type) noexcept -> bool { return failed(left); }
    template <failable T> func operator!=(T in left, Error_Type) noexcept -> bool { return failed(left) == false; }

    template <failable T> proc force(T in value) -> void {
        if(failed(value))
            throw value;
    }

    template <failable T> proc force_error(T in value) -> void {
        if(failed(value) == false)
            throw value;
    }
}

#include "undefs.h"