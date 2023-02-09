#pragma once

#include "types.h"
#include "array.h"
#include "standards.h"

namespace jot::tests
{
    template<typename T>
    struct No_Copy
    {
        T val;

        constexpr No_Copy(T val = T()) noexcept : val(val) {};
        constexpr No_Copy(const No_Copy&) noexcept = delete;
        constexpr No_Copy(No_Copy&&) noexcept = default;

        constexpr No_Copy& operator =(const No_Copy&) noexcept = delete;
        constexpr No_Copy& operator =(No_Copy&&) noexcept = default;

        constexpr bool operator==(const No_Copy& other) const noexcept { return other.val == this->val; };
        constexpr bool operator!=(const No_Copy& other) const noexcept { return other.val != this->val; };
    };

    struct Tracker_Stats
    {
        i64 own_constr = 0;
        i64 copy_constr = 0;
        i64 move_constr = 0;
        i64 destructed = 0;
        
        //just for info
        i64 copy_assign = 0;
        i64 move_assign = 0;
        i64 alive = 0;
    };

    Tracker_Stats tracker_stats;
    i64 trackers_alive()
    {
        return tracker_stats.alive;
    }

    template<typename T>
    struct Tracker
    {
        T val;
        
        Tracker(T val = T()) noexcept : val(val)       
        { 
            tracker_stats.own_constr++; 
            tracker_stats.alive ++; 
        };
        Tracker(Tracker const& other) noexcept : val(other.val)
        { 
            tracker_stats.copy_constr++; 
            tracker_stats.alive ++; 
        };
        Tracker(Tracker && other) noexcept : val((T&&) other.val)
        { 
            tracker_stats.move_constr++; 
            tracker_stats.alive ++; 
        };

        ~Tracker() noexcept
        { 
            tracker_stats.destructed++; 
            tracker_stats.alive --; 
        }

        Tracker& operator =(Tracker const& other) noexcept
        { 
            this->val = other.val;
            tracker_stats.move_assign++;
            return *this;
        }

        Tracker& operator =(Tracker && other) noexcept
        { 
            this->val = (T&&) other.val;
            tracker_stats.copy_assign++;
            return *this;
        }

        bool operator ==(const Tracker& other) const { return this->val == other.val; };
        bool operator !=(const Tracker& other) const { return this->val != other.val; };
    };

    template <typename T>
    struct Copyable<No_Copy<T>>
    {
        using No_Copy = jot::tests::No_Copy<T>;

        static constexpr
        State copy(No_Copy* to, No_Copy const& from) noexcept
        {
            to->val = from.val;
            return OK_STATE;
        };
    };

    template <typename T, isize N>
    struct Copyable<Array<T, N>>
    {
        static constexpr
        State copy(Array<T, N>* to, Array<T, N> const& from) noexcept
        {
            State state = OK_STATE;
            for(isize i = 0; i < N; i++)
            {
                State new_state = Copyable<T>::copy(&(*to)[i], from[i]);
                acumulate(&state, new_state);
            }

            return state;
        };
    };

    template<typename T> constexpr
    T dup(T const& in)
    {
        T copy;
        force(Copyable<T>::copy(&copy, in));
        return copy;
    }
    
    void test(bool cond)
    {
        force(cond);
    }
}