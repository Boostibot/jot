#pragma once
#include <cassert>
#include <cstdint>
#include <cmath>
#include <chrono>

#define nodisc [[nodiscard]]
#define cast(...) (__VA_ARGS__)

namespace jot
{
    //sorry c++ chrono but I really dont want to iteract with all those templates
    nodisc 
    int64_t clock() noexcept {
        auto duration = std::chrono::high_resolution_clock::now().time_since_epoch();
        return duration_cast<std::chrono::nanoseconds>(duration).count(); 
    }

    template <typename Fn> nodisc 
    int64_t ellapsed_time(Fn fn) noexcept
    {
        int64_t from = clock();
        fn();
        return clock() - from;
    }
    
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

    struct Bench_Stats
    {
        int64_t block_count = 0;
        int64_t block_size = 0;
        int64_t time_sum = 0; //in ns
        int64_t squared_time_sum = 0; //in ns
    };

    template <typename Fn> nodisc 
    Bench_Stats gather_bench_stats(
        int64_t max_time_ns, Fn tested_function, 
        int64_t warm_up_ns = 99999*100 * time_consts::MILISECOND_NANOSECONDS, 
        int64_t block_time_ns = 50 * time_consts::MILISECOND_NANOSECONDS, 
        int64_t base_block_size = 1, 
        int64_t min_end_checks = 10) noexcept
    {
        assert(min_end_checks > 0);
        assert(base_block_size > 0);
        assert(max_time_ns >= 0);
        assert(warm_up_ns >= 0);
        
        int64_t to_time = warm_up_ns;
        if(to_time > max_time_ns)
            to_time = max_time_ns;

        Bench_Stats stats;
        stats.block_size = base_block_size;
        stats.block_count = 0;
        stats.time_sum = 0;
        stats.squared_time_sum = 0;

        int64_t start = clock();

        while(true)
        {
            int64_t from = clock();
            for(int64_t i = 0; i < stats.block_size; i++)
                tested_function();

            int64_t now = clock();
            int64_t block_time = now - from;
            int64_t total_time = now - start;

            stats.time_sum         += block_time;
            stats.squared_time_sum += block_time * block_time;
            stats.block_count      += 1;

            if(total_time > to_time)
            {
                //if is over break
                if(total_time > max_time_ns)
                    break;
                    
                //else compute the block size so that we will finish on time
                // while checking at least min_end_checks times if we are finished
                isize remaining = max_time_ns - total_time;
                isize num_checks = remaining / block_time_ns;
                if(num_checks < min_end_checks)
                    num_checks = min_end_checks;

                isize iters = stats.block_count * stats.block_size;

                //total_expected_iters = (iters / total_time) * remaining
                //block_size = total_expected_iters / num_checks
                stats.block_size = (iters * remaining) / (total_time * num_checks);
                if(stats.block_size < 1)
                    stats.block_size = 1;

                //discard stats so far
                stats.block_count = 0; 
                stats.time_sum = 0;
                stats.squared_time_sum = 0;
                to_time = max_time_ns;
            }
        }
     
        return stats;
    }

    struct Bench_Result
    {
        double mean_ms = 0.0;
        double deviation_ms = 0.0;
    };

    Bench_Result process_stats(Bench_Stats stats, isize tested_function_calls_per_run = 1)
    {
        //tested_function_calls_per_run is in case we 'batch' our tested function: 
        // ie instead of running the tested function once we run it 100 times
        // this just means that the block_size is multiplied tested_function_calls_per_run times
        stats.block_size *= tested_function_calls_per_run;

        //Welford's algorithm for calculating normal distribution
        // (since we are operating withing the range of exact integeres we can use them for one part of the
        //  calculationg enbaling us to use the naive version)
        
        //variance = (sum2 - (sum * sum) / n) / (n - 1.0)
        //         = (sum2 * n - (sum * sum)) / (n*(n - 1.0))
        //         = num / den
        //         = deviation^2

        int64_t iters = stats.block_size * (stats.block_count);
        int64_t n = stats.block_count;
        int64_t sum = stats.time_sum;
        int64_t sum2 = stats.squared_time_sum;
            
        int64_t num = sum2*n - sum*sum;
        int64_t den = n*(n-1) * time_consts::MILISECOND_NANOSECONDS;

        double block_deviation_ms = sqrt(cast(double) num / cast(double) den);
        //sigma_sampling = sigma / sqrt(samples) (Central limit theorem)
        // => sigma = sigma_sampling * sqrt(samples)

        Bench_Result result;
        result.deviation_ms = block_deviation_ms * sqrt(cast(double) (stats.block_size));
        result.mean_ms = cast(double) stats.time_sum / cast(double) (iters * time_consts::MILISECOND_NANOSECONDS); 
        return result;
    }

    template <typename Fn> nodisc 
    Bench_Result benchmark(int64_t max_time_ms, Fn tested_function, isize tested_function_calls_per_run = 1) noexcept
    {
        Bench_Stats stats = gather_bench_stats(max_time_ms * time_consts::MILISECOND_NANOSECONDS, tested_function);
        return process_stats(stats, tested_function_calls_per_run);
    }

    void use_pointer(char const volatile*) {}

    #ifndef FORCE_INLINE
        #if defined(__GNUC__)
            #define FORCE_INLINE __attribute__((always_inline))
        #elif defined(_MSC_VER) && !defined(__clang__)
            #define FORCE_INLINE __forceinline
        #else
            #define FORCE_INLINE
        #endif
    #endif
    
    //modified version of DoNotOptimize and ClobberMemmory from google test
    #if (defined(__GNUC__) || defined(__clang__)) && !defined(__pnacl__) && !defined(EMSCRIPTN)
        #define HAS_INLINE_ASSEMBLY
    #endif

    #ifdef HAS_INLINE_ASSEMBLY
        template <typename T> FORCE_INLINE 
        void do_no_optimize(T const& value)
        {
            asm volatile("" : : "r,m"(value) : "memory");
        }

        template <typename T> FORCE_INLINE 
        void do_no_optimize(T& value)
        {
            #if defined(__clang__)
                asm volatile("" : "+r,m"(value) : : "memory");
            #else
                asm volatile("" : "+m,r"(value) : : "memory");
            #endif
        }

        FORCE_INLINE 
        void read_write_barrier(){
            asm volatile("" : : : "memory");
        }
    #elif defined(_MSC_VER)
        template <typename T> FORCE_INLINE 
        void do_no_optimize(T const& value) {
            use_pointer(cast(char const volatile*) cast(void*) &value);
            _ReadWriteBarrier();
        }

        FORCE_INLINE 
        void read_write_barrier() {
            _ReadWriteBarrier();
        }
    #else
        template <typename T> FORCE_INLINE 
        void do_no_optimize(T const& value) {
            use_pointer(cast(char const volatile*) cast(void*) &value);
        }

        FORCE_INLINE 
        void read_write_barrier() {}
    #endif
}

#undef nodisc 
#undef cast