#pragma once
#include <stdint.h>

namespace jot
{
    namespace time_consts
    {
        static constexpr int64_t SECOND_MILISECONDS  = 1'000;
        static constexpr int64_t SECOND_MIRCOSECONDS = 1'000'000;
        static constexpr int64_t SECOND_NANOSECONDS  = 1'000'000'000;
        static constexpr int64_t SECOND_PICOSECONDS  = 1'000'000'000'000;

        static constexpr int64_t MILISECOND_NANOSECONDS = SECOND_NANOSECONDS / SECOND_MILISECONDS;

        static constexpr int64_t MINUTE_SECONDS = 60;
        static constexpr int64_t HOUR_SECONDS = 60 * MINUTE_SECONDS;
        static constexpr int64_t DAY_SECONDS = 24 * HOUR_SECONDS;
        static constexpr int64_t WEEK_SECONDS = 7 * DAY_SECONDS;
    }
    
    inline int64_t clock_ns();
    inline double  clock_s();
}

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)   
#include <windows.h>
#include <profileapi.h>
namespace jot
{
    inline int64_t _query_perf_freq()
    {
        LARGE_INTEGER ticks;
        if (!QueryPerformanceFrequency(&ticks))
            throw "couldnt query the perf counter!";

        return ticks.QuadPart;
    }


    inline int64_t _query_perf_counter()
    {
        LARGE_INTEGER ticks;
        (void) QueryPerformanceCounter(&ticks);
        return ticks.QuadPart;
    }
    
    inline int64_t* _perf_counter_base()
    {
        static int64_t base = _query_perf_counter();
        return &base;
    }

    inline int64_t _perf_freq()
    {
        static int64_t freq = _query_perf_freq(); //doesnt change so we can cache it
        return freq;
    }
    
    inline double _double_perf_freq()
    {
        static double freq = (double) _query_perf_freq();
        return freq;
    }

    inline int64_t clock_ns()
    {
        int64_t freq = _perf_freq();
        int64_t counter = _query_perf_counter() - *_perf_counter_base();

        int64_t sec_to_nanosec = 1'000'000'000;
        //We assume _perf_counter_base is set to some reasonable thing so this will not overflow
        // (with this we are only able to represent 1e12 secons (A LOT) without overflowing)
        return counter * sec_to_nanosec / freq;
    }

    //We might be rightfully scared that after some ammount of time the clock_s will get sufficiently large and
    // we will start loosing pression. This is however not a problem. If we assume the _perf_freq is equal to 
    // 10Mhz = 1e7 (which is very common) then we would like the clock_s to have enough precision to represent
    // 1 / 10Mhz = 1e-7. This precission is held up untill 1e9 secons have passed which is roughly 31 years. 
    // => Please dont run your program for more than 31 years or you will loose precision
    inline double clock_s()
    {
        double freq = _double_perf_freq();
        double counter = (double) (_query_perf_counter() - *_perf_counter_base());
        return counter / freq;
    }
}
#else
#include <time.h>
namespace jot
{
    inline int64_t _clock_ns()
    {
        struct timespec ts;
        (void) clock_gettime(CLOCK_MONOTONIC_RAW, &ts);

        return ts.tv_nsec;
    }

    inline int64_t clock_ns()
    {
        static int64_t base = _clock_ns();
        return _clock_ns() - base;
    }
    
    inline double clock_s()
    {
        return (double) clock_ns() / 1.0e9;
    }
}
#endif
