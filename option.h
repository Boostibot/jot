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
        static constexpr nodisc 
        bool perform(State state) 
        {
            return state != OK_STATE;
        }
    };

    template<>
    struct Failable<bool>
    {
        static constexpr nodisc 
        bool perform(bool state) 
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
        static constexpr nodisc 
        bool perform(Nullable<T> ptr) noexcept
        {
            return ptr.value == nullptr;
        }
    };

    template<typename T> constexpr nodisc 
    T value(Nullable<T> ptr) noexcept
    {
        return ptr.value;
    }

    template <failable T> constexpr nodisc 
    bool operator==(T const& left, Ok_Type) noexcept { return failed(left) == false; }
    
    template <failable T> constexpr nodisc 
    bool operator!=(T const& left, Ok_Type) noexcept { return failed(left); }
    
    template <failable T> constexpr nodisc 
    bool operator==(T const& left, Error_Type) noexcept { return failed(left); }
    
    template <failable T> constexpr nodisc 
    bool operator!=(T const& left, Error_Type) noexcept { return failed(left) == false; }


    template <failable T> 
    void force(T const& value)
    {
        if(failed(value))
            throw value;
    }

    template <failable T> 
    void force_error(T const& value)
    {
        if(failed(value) == false)
            throw value;
    }
}

#include "undefs.h"