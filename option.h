#pragma once
#include "standards.h"
#include "panic.h"
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
        if(failed(new_state) && failed(*into) == false)
            *into = new_state;
    }
    
    constexpr 
    void _force(bool failed, const char* message, Line_Info const& line_info)
    {
        if(failed)
            throw Panic{message, line_info};
    }

    #define force(condition) _force(failed(condition), #condition, GET_LINE_INFO())
    #define force_msg(condition, msg) _force(failed(condition), msg, GET_LINE_INFO())

    inline
    void operator*(State state)
    {
        force(failed(state) == false);
    }
    
    template<typename T> nodisc constexpr
    T dup(T const& in)
    {
        T copied;
        *copy(&copied, in);
        return copied;
    }
}

#include "undefs.h"