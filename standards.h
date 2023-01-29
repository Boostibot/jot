#pragma once
#include "traits.h"
#include "open_enum.h"
#include "utils.h"
#include "defines.h"

open_enum Open_Enum
{
    static constexpr Type OK = nullptr;
}

namespace jot
{
    using State = ::Open_Enum::Type;
    using State_Holder = ::Open_Enum::Holder;
    constexpr State OK_STATE = ::Open_Enum::OK;

    //Declares open enum with aditional OK value set as the null value
    #define OPEN_STATE_DECLARE(Name)                          \
        OPEN_ENUM_DECLARE_DERIVED(Name, ::jot::State_Holder); \
        static constexpr Type OK = nullptr;                   \

    struct No_Default {};

    template <typename T, typename Enable = Enabled>
    struct Assignable
    {
        nodisc static constexpr
        State assign(T* to, T const& from) noexcept
        {
            static_assert(std::is_nothrow_copy_assignable_v<T>, "must be nothrow copyable! If not provide a custom overload of this function");
            *to = from;

            return OK_STATE;
        };
    };
    
    template <typename T> nodisc constexpr 
    T && move(T* val) 
    { 
        return cast(T &&) *val; 
    };

    template <typename T, typename Enable = Enabled>
    struct Swappable
    {
        static constexpr 
        void swap(T* left, T* right) noexcept
        {
            T temp = move(left);
            *left = move(right);
            *right = move(&temp);
        };
    };

    template <typename T, typename Enable = Enabled>
    struct Failable : No_Default
    {
        nodisc static constexpr
        bool failed(T const& flag) noexcept {
            return false;
        }
    };

    template<typename T>
    static constexpr bool failable = !std::is_base_of_v<No_Default, Failable<T>>;

    template <typename T> nodisc constexpr 
    State assign(T* to, T const& from) noexcept 
    {
        return Assignable<T>::assign(to, from);
    }

    template <typename T> constexpr 
    void swap(T* left, T* right) noexcept 
    {
        return Swappable<T>::swap(left, right);
    }

    template <typename T> nodisc constexpr 
    bool failed(T const& flag) noexcept 
    {
        return Failable<T>::failed(flag);
    }
    
    template <typename T, typename ... Args>
    T* construct_at(T* at, Args &&... args)
    {
        new(at) T(cast(Args &&) args... );
        return at;
    }

    template<typename T> nodisc constexpr 
    State construct_assign_at(T* to, no_infer(T) const& from)
    {
        construct_at(to);
        return assign<T>(to, from);
    }
}

#include "undefs.h"