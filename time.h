#pragma once
#include <cassert>
#include <cstdint>
#include <chrono>

#define func auto [[nodiscard]]
#define proc auto
#define cast(...) (__VA_ARGS__)

namespace jot
{
    using i64 = int64_t;

    //sorry c++ chrono but I really dont want to iteract with all those templates
    func clock() noexcept -> i64 {
        auto duration = std::chrono::high_resolution_clock::now().time_since_epoch();
        return duration_cast<std::chrono::nanoseconds>(duration).count(); 
    }

    template <typename Fn>
    func ellapsed_time(Fn fn) noexcept -> i64
    {
        i64 from = clock();
        fn();
        return clock() - from;
    }

    struct Bench_Result
    {
        i64 iters = 0;
        i64 time_ns = 0;
    };

    template <typename Fn>
    func benchmark_ns(i64 max_time_ns, i64 warm_up_ns, Fn fn, i64 base_block_size = 1, i64 num_checks = 5) noexcept -> Bench_Result
    {
        assert(num_checks > 0);
        assert(base_block_size > 0);
        assert(max_time_ns >= 0);
        assert(warm_up_ns >= 0);

        i64 to_time = warm_up_ns;

        Bench_Result result;
        i64 block_size = base_block_size;

        i64 start = clock();

        while(true)
        {
            i64 from = clock();
            for(i64 i = 0; i < block_size; i++)
                fn();

            i64 now = clock();
            i64 block_time = now - from;
            i64 total_time = now - start;

            result.time_ns += block_time;
            result.iters += block_size;
            if(total_time > to_time)
            {
                if(total_time > max_time_ns)
                    break;

                block_size = result.iters * max_time_ns / (total_time * num_checks);
                if(block_size < 1)
                    block_size = 1;

                result.iters = 0; //discard so far
                result.time_ns = 0;
                to_time = max_time_ns;
            }
        }

        return result;
    }

    template <typename Fn>
    func benchmark(i64 max_time_ms, Fn fn) noexcept -> Bench_Result
    {
        i64 max_time_ns = max_time_ms * 1'000'000;
        return benchmark_ns(max_time_ns, max_time_ns / 10, fn);
    }

    void use_pointer(char const volatile*) {}

    //modified version of DoNotOptimize and ClobberMemmory from google test
    #if defined(__GNUC__)
        #define FORCE_INLINE __attribute__((always_inline))
    #elif defined(_MSC_VER) && !defined(__clang__)
        #define FORCE_INLINE __forceinline
    #else
        #define FORCE_INLINE
    #endif

    #if (defined(__GNUC__) || defined(__clang__)) && !defined(__pnacl__) && !defined(EMSCRIPTN)
        #define HAS_INLINE_ASSEMBLY
    #endif

    #ifdef HAS_INLINE_ASSEMBLY
        template <typename T>
        FORCE_INLINE proc do_no_optimize(T const& value)
        {
            if(std::is_constant_evaluated())
                return;
            asm volatile("" : : "r,m"(value) : "memory");
        }

        template <typename T>
        FORCE_INLINE proc do_no_optimize(T& value)
        {
            if(std::is_constant_evaluated())
                return;
            #if defined(__clang__)
                asm volatile("" : "+r,m"(value) : : "memory");
            #else
                asm volatile("" : "+m,r"(value) : : "memory");
            #endif
        }

        FORCE_INLINE proc read_write_barrier(){
            if(std::is_constant_evaluated())
                return;

            asm volatile("" : : : "memory");
        }
    #elif defined(_MSC_VER)
        template <typename T>
        FORCE_INLINE proc do_no_optimize(T const& value) {
            if(std::is_constant_evaluated())
                return;

            use_pointer(cast(char const volatile*) cast(void*) &value);
            _ReadWriteBarrier();
        }

        FORCE_INLINE proc read_write_barrier() {
            if(std::is_constant_evaluated())
                return;

            _ReadWriteBarrier();
        }
    #else
        template <typename T>
        FORCE_INLINE proc do_no_optimize(T const& value) {
            use_pointer(cast(char const volatile*) cast(void*) &value);
        }

        FORCE_INLINE proc read_write_barrier() {}
    #endif
}

#undef func 
#undef proc
#undef cast