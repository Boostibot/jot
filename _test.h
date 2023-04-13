#pragma once

#include "types.h"
#include "array.h"
#include "string.h"
#include "format.h"

namespace jot::tests
{
    namespace test_internal
    {
        enum Test_Flags : u32
        {
            SILENT = 1,
            STRESS = 2,
        };
    }

    using test_internal::Test_Flags;

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

    struct Test_String
    {
        String_Builder content;

        Test_String() = default;
        Test_String(const char* str)
        {
            content = own(str);
        }

        bool operator==(Test_String const& other) const
        {
            return are_bytes_equal(slice(content), slice(other.content));
        }
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

    inline i64 trackers_alive()
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
    
    template<typename T> T dup(T const& val)
    {
        return T(val);
    }

    template<typename T> No_Copy<T> dup(No_Copy<T> const& track)
    {
        return No_Copy<T>{track.val}; 
    }
    
    template<typename T> Tracker<T> dup(Tracker<T> const& track)
    {
        return Tracker<T>{track.val}; 
    }

    template <typename T, isize N> Array<T, N> dup(Array<T, N> const& arr)
    {
        Array<T, N> duped = {};
        for(isize i = 0; i < N; i++)
            duped[i] = dup(arr[i]);

        return duped; 
    }

    #define test(cond) force(cond)
}

namespace jot
{
    template <typename T> 
    struct Formattable<tests::Tracker<T>>
    {
        static
        void format(String_Builder* into, tests::Tracker<T> tracker) noexcept
        {
            format_into(into, "Tracker{ ", tracker.val, " }");
        }
    };

    template <> 
    struct Formattable<tests::Test_String>
    {
        static
        void format(String_Builder* into, tests::Test_String str) noexcept
        {
            format_into(into, str.content);
        }
    };
}