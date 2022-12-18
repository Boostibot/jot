#pragma once
#include "defines.h"

namespace jot
{
    #ifdef USE_FUNC_CALL_MOVE
        template <typename T>
        func move_(T* val) noexcept -> T moved {
            return cast(T moved) *val;
        };

        template<typename T>
        func forward_(std::remove_reference_t<T>& t) noexcept -> T moved {
            return static_cast<T moved>(t);
        }

        template<typename T>
        func forward_(std::remove_reference_t<T> moved t) noexcept -> T moved {
            return static_cast<T moved>(t);
        }

        #define move(...) move_(__VA_ARGS__)
        #define forward(T, ...) forward_<T>(__VA_ARGS__)
    #else
        #define jot_move_impl(val)  cast(decltype(*val) moved) *val

        #define move(...) jot_move_impl((__VA_ARGS__))
        #define forward(T, ...) cast(T moved) (__VA_ARGS__)
    #endif // USE_FUNC_CALL_MOVE


    template<typename T>
    concept swappable = requires(T* left, T* right)
    {
        swap(left, right);
    };

    template<typename T>
    concept unswappable = !swappable<T>;

    template <unswappable T>
    proc swap(T* left, T* right) noexcept -> Unit {
        T temp = move(left);
        *left = move(right);
        *right = move(&temp);
    }
}

#include "undefs.h"