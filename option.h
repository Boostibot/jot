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
        nodisc static constexpr 
        bool failed(State state) 
        {
            return state != OK_STATE;
        }
    };

    template<>
    struct Failable<bool>
    {
        nodisc static constexpr 
        bool failed(bool state) 
        {
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
        nodisc static constexpr 
        bool failed(Nullable<T> ptr) noexcept
        {
            return ptr.value == nullptr;
        }
    };

    template<typename T> nodisc constexpr 
    T value(Nullable<T> ptr) noexcept
    {
        return ptr.value;
    }
    

    template <typename T, Enable_If<failable<T>> = ENABLED> nodisc constexpr 
    bool operator==(T const& left, Ok_Type) noexcept { return failed(left) == false; }
    
    template <typename T, Enable_If<failable<T>> = ENABLED> nodisc constexpr 
    bool operator!=(T const& left, Ok_Type) noexcept { return failed(left); }
    
    template <typename T, Enable_If<failable<T>> = ENABLED> nodisc constexpr 
    bool operator==(T const& left, Error_Type) noexcept { return failed(left); }
    
    template <typename T, Enable_If<failable<T>> = ENABLED> nodisc constexpr 
    bool operator!=(T const& left, Error_Type) noexcept { return failed(left) == false; }

    template <typename T> constexpr
    void force(T const& value)
    {
        if(failed(value))
            throw value;
    }

    template <typename T> constexpr
    void force_error(T const& value)
    {
        if(failed(value) == false)
            throw value;
    }
    
    //the forcing operator (cause I am lazy)
    template <typename T, Enable_If<failable<T>> = ENABLED> nodisc constexpr 
    void operator<<(Ok_Type, T const& left) noexcept { force(left); }

    inline constexpr
    State acumulate(State prev, State new_state)
    {
        if(failed(prev))
            return prev;
        else
            return new_state;
    }
    
    inline constexpr
    void acumulate(State* into, State new_state)
    {
        if(failed(*into) == false)
            *into = new_state;
    }
}

#include "undefs.h"