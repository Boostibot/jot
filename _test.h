#pragma once

#include "types.h"

namespace jot::tests
{
    template<typename T>
    struct Not_Copyable
    {
        T val;

        constexpr Not_Copyable(T val) noexcept : val(val) {};
        constexpr Not_Copyable(const Not_Copyable&) noexcept = delete;
        constexpr Not_Copyable(Not_Copyable&&) noexcept = default;

        constexpr Not_Copyable& operator =(const Not_Copyable&) noexcept = delete;
        constexpr Not_Copyable& operator =(Not_Copyable&&) noexcept  = default;

        constexpr bool operator==(const Not_Copyable&) const noexcept = default;
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
        
        Tracker(T val = T()) : val(val)         
            { tracker_stats.own_constr++; tracker_stats.alive ++; };
        Tracker(Tracker const& other) : val(other.val)
            { tracker_stats.copy_constr++; tracker_stats.alive ++; };
        Tracker(Tracker && other) : val((T&&) other.val)
            { tracker_stats.move_constr++; tracker_stats.alive ++; };

        Tracker& operator =(Tracker const& other) 
        { 
            this->val = other.val;
            tracker_stats.move_assign++;
            return *this;
        }

        Tracker& operator =(Tracker && other) 
        { 
            this->val = (T&&) other.val;
            tracker_stats.copy_assign++;
            return *this;
        }

        ~Tracker() { tracker_stats.destructed++; tracker_stats.alive --; }

        bool operator ==(const Tracker& other) const { return this->val == other.val; };
    };
}