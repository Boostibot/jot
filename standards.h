#pragma once
#include "traits.h"
#include "open_enum.h"
#include "utils.h"

#define nodisc [[nodiscard]]

open_enum Open_Enum
{
    static constexpr Type OK = Type{nullptr};
}

namespace jot
{
    using State = ::Open_Enum::Type;
    constexpr State OK_STATE = ::Open_Enum::OK;

    //Declares open enum with aditional OK value set as the null value
    #define OPEN_STATE_DECLARE(name_str)                      \
        OPEN_ENUM_DECLARE_DERIVED(name_str, ::jot::State);    \
        static constexpr Type OK = Type{nullptr};             \
        
    template <typename T> nodisc constexpr 
    T && move(T* val) 
    { 
        return (T &&) *val; 
    };

    struct No_Default {};

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
        new(at) T((Args &&) args... );
        return at;
    }

    //@TODO: remove
    template<typename T> 
    T* copy_construct_at(T* to, no_infer(T) const& from) noexcept
    {
        return construct_at(to, from);
    }
}

#undef nodisc
