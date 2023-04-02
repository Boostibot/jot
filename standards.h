#pragma once

#include <type_traits>
#include "open_enum.h"
#include "utils.h"

#define nodisc [[nodiscard]]


namespace jot
{
    enum Enabled { ENABLED = 0 };

    template<bool cond, class T>
    struct _Enable_If {};
    
    template<class T>
    struct _Enable_If<true, T> { typedef T type; };

    template<bool cond, class T = Enabled>
    using Enable_If = typename _Enable_If<cond, T>::type;

    open_enum State
    {
        OPEN_ENUM_DECLARE("jot::State");
        static constexpr Type OK = nullptr;
    }

    using State_Type = ::jot::State::Type;

    //Declares open enum with aditional OK value set as the null value
    #define OPEN_STATE_DECLARE(name_str)                      \
        OPEN_ENUM_DECLARE_DERIVED(name_str, ::jot::State);    \
        static constexpr Type OK = nullptr;                   \
        
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

    template <typename T> constexpr 
    void swap(T* left, T* right) noexcept 
    {
        return Swappable<T>::swap(left, right);
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
