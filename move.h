#pragma once
#include "defines.h"

namespace jot
{
    template <typename T>
    func move_(T* val) noexcept -> T && {
        return cast(T &&) *val;
    };

    #define move(...) move_(__VA_ARGS__)
    #ifdef USE_FUNC_CALL_MOVE
        template<typename T>
        func forward_(std::remove_reference_t<T>& t) noexcept -> T && {
            return static_cast<T &&>(t);
        }

        template<typename T>
        func forward_(std::remove_reference_t<T> && t) noexcept -> T && {
            return static_cast<T &&>(t);
        }

        #define forward(T, ...) forward_<T>(__VA_ARGS__)
    #else
        #define forward(T, ...) (T &&) (__VA_ARGS__)
    #endif // USE_FUNC_CALL_MOVE

    template<typename T>
    concept swappable = requires(T* left, T* right)
    {
        swap(left, right);
    };

    template<typename T>
    concept unswappable = !swappable<T>;

    template <unswappable T>
    proc swap(T* left, T* right) noexcept -> void {
        T temp = move(left);
        *left = move(right);
        *right = move(&temp);
    }
}

#include "undefs.h"