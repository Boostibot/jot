#pragma once
#include <random>

#include "time.h"
#include "memory.h"
#include "allocator_arena.h"
#include "allocator_stack_ring.h"
#include "allocator_ring.h"
#include "format.h"

#define cast(...) (__VA_ARGS__)
#define nodisc [[nodiscard]]

namespace jot
{
    template<typename T>
    struct Range 
    {
        T from;
        T to;
    };

    using IRange = Range<isize>;

    nodisc constexpr 
    bool is_invarinat(IRange range) {
        return range.from <= range.to;
    }

    nodisc constexpr 
    bool in_range(IRange range, isize index) {
        return (range.from <= index && index < range.to);
    }

    nodisc constexpr 
    bool in_inclusive_range(IRange range, isize index) {
        return (range.from <= index && index <= range.to);
    }

    nodisc constexpr 
    IRange sized_range(isize from, isize size) {
        return IRange{from, from + size};
    }

    void test_align()
    {
        u8 dummy = 0;
        u8* aligned = align_forward(&dummy, 32);
        usize ptr_num = cast(usize) aligned;
        force(ptr_num/32*32 == ptr_num);

        force(align_forward(aligned + 1, 4) == align_backward(aligned + 7, 4));
        force(align_forward(aligned + 1, 8) == align_backward(aligned + 15, 8));
        force(align_forward(aligned + 3, 16) == align_backward(aligned + 27, 16));
        force(align_forward(aligned + 13, 16) == align_backward(aligned + 17, 16));
    }

    void test_stack_ring()
    {
        u8 stack_ring_storage[400];
        {
            Stack_Ring_Allocator stack_ring = Stack_Ring_Allocator(Slice{stack_ring_storage, 400}, &memory_globals::FAILING_ALLOCATOR);
            Slice<u8> first = stack_ring.allocate(10, 8).items;
            Slice<u8> second = stack_ring.allocate(20, 256).items;
            Slice<u8> third = stack_ring.allocate(30, 8).items;

            force(stack_ring.deallocate(second, 8));

            Allocation_Result result = stack_ring.resize(first, 8, 25);
            force(result.state);
            first = result.items;

            result = stack_ring.resize(first, 8, 40 + 256);
            force(result.state == ERROR);

            force(stack_ring.deallocate(first, 8));
            force(stack_ring.deallocate(third, 8));
        }

        {
            Stack_Ring_Allocator stack_ring = Stack_Ring_Allocator(Slice{stack_ring_storage, 256}, &memory_globals::FAILING_ALLOCATOR);
            Slice<u8> a1 = stack_ring.allocate(64, 8).items;
            Slice<u8> a2 = stack_ring.allocate(64, 8).items;
            Slice<u8> a3 = stack_ring.allocate(64, 8).items;

            force(stack_ring.deallocate(a1, 8));
            force(stack_ring.deallocate(a2, 8));
            
            Slice<u8> a4 = stack_ring.allocate(64, 8).items;
            Slice<u8> a5 = stack_ring.allocate(64, 8).items;

            force(stack_ring.allocate(64, 8).state == ERROR);
            
            force(stack_ring.deallocate(a3, 8));
            force(stack_ring.deallocate(a4, 8));
            force(stack_ring.deallocate(a5, 8));
        }
    }

    #if 0
    TESTSMALL SIZES NO TOUCH
    ===========
    iters: 10
    size:  16 - 256
    align: 1 - 32
                    fifo                      lifo                    temp                    resize                  read
    unbound:       9.681e-05:2.324e+01       9.892e-05:9.209e-01     1.094e-04:1.501e+00     1.094e-04:1.501e+00     1.324e-04:4.324e+01
    stack resi:    1.221e-04:2.824e+00       1.135e-04:2.624e+01     1.337e-04:5.588e+02     1.337e-04:5.588e+02     1.643e-04:2.997e+01
    stack scan:    1.151e-04:5.759e+00       1.084e-04:1.503e+02     1.242e-04:9.506e+01     1.242e-04:9.506e+01     1.541e-04:3.581e+01
    stack ring:    1.248e-04:8.334e+01       1.234e-04:2.424e+01     1.387e-04:6.222e+01     1.387e-04:6.222e+01     1.657e-04:2.363e+01
    stack simp:    1.091e-04:5.375e+01       1.034e-04:1.456e+02     1.197e-04:5.541e+01     1.197e-04:5.541e+01     1.491e-04:2.669e+01


    iters: 80
    size:  16 - 256
    align: 1 - 32
                    fifo                      lifo                    temp                    resize                  read
    unbound:       7.196e-04:6.842e+01       7.022e-04:1.408e+02     7.385e-04:1.044e+03     7.385e-04:1.044e+03     7.373e-04:1.162e+02
    stack resi:    1.061e-03:1.522e+02       9.263e-04:7.156e+01     1.014e-03:5.717e+02     1.014e-03:5.717e+02     1.096e-03:1.698e+02
    stack scan:    9.931e-04:3.857e+01       8.736e-04:7.723e+01     9.463e-04:4.021e+02     9.463e-04:4.021e+02     1.030e-03:1.771e+02
    stack ring:    1.081e-03:3.978e+02       9.700e-04:3.589e+01     1.059e-03:6.915e+01     1.059e-03:6.915e+01     1.108e-03:7.872e+00
    stack simp:    9.369e-04:2.597e+00       8.395e-04:8.754e+01     9.049e-04:2.282e+02     9.049e-04:2.282e+02     9.728e-04:2.596e+02


    iters: 640
    size:  16 - 256
    align: 1 - 32
                    fifo                      lifo                    temp                    resize                  read
    unbound:       5.558e-03:5.186e+02       5.479e-03:3.446e+02     5.597e-03:2.732e+02     5.597e-03:2.732e+02     5.469e-03:3.003e+01
    stack resi:    9.534e-03:2.011e+01       7.584e-03:1.316e+02     8.174e-03:1.518e+04     8.174e-03:1.518e+04     9.385e-03:3.568e+01
    stack scan:    8.980e-03:1.202e+02       7.128e-03:1.374e+03     7.510e-03:2.508e+03     7.510e-03:2.508e+03     8.836e-03:1.327e+01
    stack ring:    9.367e-03:1.017e+03       7.918e-03:1.420e+02     8.400e-03:3.169e+02     8.400e-03:3.169e+02     9.274e-03:3.714e+02
    stack simp:    8.374e-03:5.738e+02       6.789e-03:5.177e+02     7.139e-03:1.055e+03     7.139e-03:1.055e+03     8.229e-03:1.448e+03


    SMALL SIZES
    ===========
    iters: 10
    size:  16 - 256
    align: 1 - 32
                    fifo                      lifo                    temp                    resize                  read
    unbound:       2.125e-04:3.163e+02       2.115e-04:1.642e+02     2.025e-04:2.708e+02     2.025e-04:2.708e+02     1.322e-04:3.423e+01
    stack resi:    2.174e-04:3.694e+02       2.161e-04:7.032e+02     2.064e-04:1.681e+02     2.064e-04:1.681e+02     4.508e-04:5.552e+01
    stack scan:    2.196e-04:2.539e+01       2.188e-04:1.886e+02     2.064e-04:2.369e+02     2.064e-04:2.369e+02     1.540e-04:8.702e+00
    stack ring:    2.291e-04:2.450e+02       2.283e-04:2.014e+02     2.235e-04:2.359e+02     2.235e-04:2.359e+02     1.658e-04:2.280e+01
    stack simp:    2.100e-04:3.990e+01       2.094e-04:4.956e+01     1.976e-04:1.019e+02     1.976e-04:1.019e+02     1.494e-04:2.632e+01


    iters: 80
    size:  16 - 256
    align: 1 - 32
                    fifo                      lifo                    temp                    resize                  read
    unbound:       2.816e-03:1.970e+02       2.783e-03:3.293e+02     1.463e-03:1.280e+03     1.463e-03:1.280e+03     7.377e-04:1.318e+02
    stack resi:    5.337e-03:2.739e+02       5.275e-03:6.591e+01     2.564e-03:2.567e+02     2.564e-03:2.567e+02     3.681e-03:4.340e+02
    stack scan:    2.450e-03:2.195e+01       2.335e-03:2.061e+01     1.698e-03:6.166e+02     1.698e-03:6.166e+02     1.033e-03:5.377e+02
    stack ring:    2.527e-03:1.391e+01       2.427e-03:3.929e+02     1.841e-03:1.826e+02     1.841e-03:1.826e+02     1.107e-03:1.097e+02
    stack simp:    2.367e-03:3.425e+02       2.264e-03:1.968e+02     1.623e-03:4.691e+01     1.623e-03:4.691e+01     9.700e-04:4.764e+02


    iters: 640
    size:  16 - 256
    align: 1 - 32
                    fifo                      lifo                    temp                    resize                  read
    unbound:       2.312e-02:5.289e+00       2.299e-02:1.431e+02     1.124e-02:1.620e+02     1.124e-02:1.620e+02     5.461e-03:1.486e+02
    stack resi:    7.738e-02:4.763e+05       7.533e-02:2.234e+03     2.623e-02:1.356e+03     2.623e-02:1.356e+03     5.295e-02:3.546e+03
    stack scan:    2.057e-02:2.929e+02       1.887e-02:2.956e+02     1.349e-02:1.312e+03     1.349e-02:1.312e+03     8.677e-03:1.955e+02
    stack ring:    2.096e-02:7.766e+01       1.966e-02:1.972e+02     1.451e-02:1.267e+03     1.451e-02:1.267e+03     9.165e-03:1.192e+02
    stack simp:    2.016e-02:8.292e+02       1.851e-02:3.521e+01     1.279e-02:5.833e+02     1.279e-02:5.833e+02     8.284e-03:1.053e+02
    #endif

    void test_allocators()
    {
        test_align();
        //test_stack_ring();

        using Generator = std::mt19937;

        isize block_size = 100;
        isize max_time = 5'000;
        isize warm_up = 250;
        isize size_log2_min = 6;
        isize size_log2_max = 12;

        isize align_log2_min = 1;
        isize align_log2_max = 5;

        Generator gen;
        auto size_distribution = std::uniform_int_distribution<decltype(1ll)>(size_log2_min, size_log2_max);
        auto size_noise_distribution = std::uniform_int_distribution<decltype(1ll)>(0, 10);
        auto align_distribution = std::uniform_int_distribution<decltype(1ll)>(align_log2_min, align_log2_max);

        Allocator* def = memory_globals::default_allocator();

        Allocator* tested = def;

        isize max_alloced_storage = 320 * memory_constants::MEBI_BYTE;
        Stack<u8> ring_storage;
        Stack<u8> stack_storage;
        Stack<u8> stack_simple_storage;

        force(resize(&ring_storage, max_alloced_storage));
        force(resize(&stack_storage, max_alloced_storage));
        force(resize(&stack_simple_storage, max_alloced_storage));

        Failing_Allocator failling;
        Ring_Allocator  ring = Ring_Allocator(slice(&ring_storage), def);
        Intrusive_Stack_Scan stack_scan = Intrusive_Stack_Scan(slice(&ring_storage), def);
        Intrusive_Stack_Resize stack_resi = Intrusive_Stack_Resize(slice(&ring_storage), def);
        Stack_Ring_Allocator stack_ring = Stack_Ring_Allocator(slice(&ring_storage), def);
        Intrusive_Stack_Simple stack_simp = Intrusive_Stack_Simple(slice(&ring_storage), def);
        Arena_Allocator unbound = Arena_Allocator(def);

        Stack<isize> size_table;
        Stack<isize> align_table;
        Stack<Slice<u8>> allocs;

        const auto resize_size_tables = [&](isize block_size_){
            block_size = block_size_;
            force(resize(&size_table, block_size));
            force(resize(&align_table, block_size));
            force(resize(&allocs, block_size));

            for(isize i = 0; i < block_size; i++)
            {
                size_table[i]  = cast(isize) ((1ll << size_distribution(gen)) + size_noise_distribution(gen));
                align_table[i] = cast(isize) (1ll << align_distribution(gen));
            }
        };

        bool touch = true;
        const auto fill_slice = [&](Slice<u8> slice){
            if(touch == false)
                return;

            Slice<u32> u32s = cast_slice<u32>(slice);
            for(isize i = 0; i < u32s.size; i++)
                u32s[i] = 0xAABBCCDD;
        };

        const auto set_up_test = [&](
            isize block_size_,
            IRange size_log2,
            IRange align_log2,
            bool touch_,
            isize max_time_ = 1000,
            isize warm_up_ = 100)
        {
            resize_size_tables(block_size_);

            max_time = max_time_;
            warm_up = warm_up_;
            size_log2_min = size_log2.from;
            size_log2_max = size_log2.to;

            align_log2_min = align_log2.from;
            align_log2_max = align_log2.to;

            touch = touch_;
        };

        const auto test_allocs_fifo = [&]() {
            for(isize i = 0; i < block_size; i++)
            {
                isize size = size_table[i];
                isize align = align_table[i];
                Allocation_Result result = tested->allocate(size, align);
                force(result.state);
                fill_slice(result.items);
                allocs[i] = result.items;
            }

            for(isize i = 0; i < block_size; i++)
            {
                Slice<u8> alloced = allocs[i];
                isize align = align_table[i];
                force(tested->deallocate(alloced, align));
            }

            unbound.reset();
        };   

        const auto test_allocs_lifo = [&]() {
            for(isize i = 0; i < block_size; i++)
            {
                Allocation_Result result = tested->allocate(size_table[i], align_table[i]);
                force(result.state);
                fill_slice(result.items);
                allocs[i] = result.items;
            }

            for(isize i = block_size; i-- > 0;)
                force(tested->deallocate(allocs[i], align_table[i]));

            unbound.reset();
        };   

        const auto test_allocs_temp = [&]() {

            for(isize i = 0; i < block_size; i++)
            {
                Allocation_Result result = tested->allocate(size_table[i], align_table[i]);
                do_no_optimize(result);
                force(result.state);
                fill_slice(result.items);
                force(tested->deallocate(result.items, align_table[i]));
            }

            unbound.reset();
        };

        const auto test_allocs_resi = [&]() {
            for(isize i = 0; i < block_size; i++)
            {
                Allocation_Result result = tested->allocate(size_table[i], align_table[i]);
                force(result.state);
                fill_slice(result.items);
                allocs[i] = result.items;
            }

            for(isize i = 0; i < block_size; i += 2)
            {
                force(tested->deallocate(allocs[i], align_table[i]));
            }

            for(isize i = 1; i < block_size; i += 2)
            {
                Slice<u8> old_data = allocs[i];
                isize old_size = old_data.size;
                isize new_size = old_size * 2;
                isize align = align_table[i];

                Allocation_Result result = tested->resize(old_data, align, new_size);
                if(result.state == ERROR)
                {
                    result = tested->allocate(new_size, align);
                    force(result.state);

                    if(touch)
                        copy_bytes<u8>(&result.items, old_data);

                    force(tested->deallocate(old_data, align));
                }

                allocs[i] = result.items;
                Slice<u8> added = slice(result.items, old_size);
                if(touch)
                    null_bytes(&added);
            }

            for(isize i = 1; i < block_size; i += 2)
            {
                if(allocs[i].data != nullptr)
                    force(tested->deallocate(allocs[i], align_table[i]));
            }

            unbound.reset();   
        };
        
        //Alloc data, then read the data 10 times (summing bytes but the op doesnt matter), dealloc data fifo
        const auto test_allocs_read = [&]() {
            for(isize i = 0; i < block_size; i++)
            {
                isize size = size_table[i];
                isize align = align_table[i];
                Allocation_Result result = tested->allocate(size, align);
                force(result.state);
                allocs[i] = result.items;
            }

            isize sum = 0;
            for(isize j = 0; j < 100; j++)
            {
                for(isize i = 0; i < block_size; i++)
                {
                    Slice<u8> alloced = allocs[i];
                    for(isize k = 0; k < alloced.size; k++)
                        sum += alloced[k];
                }
            }
            
            for(isize i = 0; i < block_size; i++)
            {
                Slice<u8> alloced = allocs[i];
                isize align = align_table[i];
                force(tested->deallocate(alloced, align));
            }

            unbound.reset();
        };

        const auto format_benchmark_result = [](Bench_Result result){
            String_Builder builder;
            String_Appender appender = {&builder};
            force(format_float_into(&appender, result.mean_ms, std::chars_format::scientific, 3));
            force(format_into(&appender, ":"));
            force(format_float_into(&appender, result.deviation_ms, std::chars_format::scientific, 3));

            return builder;
        };

        const auto run_and_print_tests = [&](cstring text, Allocator* tested_){
            tested = tested_;

            String_Builder res[5];
            res[0] = format_benchmark_result(benchmark(max_time, test_allocs_fifo, block_size));
            res[1] = format_benchmark_result(benchmark(max_time, test_allocs_lifo, block_size));
            res[2] = format_benchmark_result(benchmark(max_time, test_allocs_temp, block_size));
            res[3] = format_benchmark_result(benchmark(max_time, test_allocs_resi, block_size));
            res[4] = format_benchmark_result(benchmark(max_time, test_allocs_read, block_size));

            println("{} {}\t {}\t {}\t {}\t {}\t", text, res[0], res[1], res[2], res[2], res[4]);
        };

        const auto print_benchmark_for_block = [&](isize block_size_, IRange size_log2, IRange align_log2, bool touch = true){
            set_up_test(block_size_, size_log2, align_log2, touch);
            println("iters: {}", block_size);
            println("size:  {} - {}", 1ll << size_log2.from, 1ll << size_log2.to);
            println("align: {} - {}", 1ll << align_log2.from, 1ll << align_log2.to);
            
            println("               fifo                 \t lifo              \t temp                 \t resize                \t read");
            run_and_print_tests("new delete:   ", &memory_globals::NEW_DELETE_ALLOCATOR);
            run_and_print_tests("unbound:      ", &unbound);
            run_and_print_tests("ring:         ", &ring);
            run_and_print_tests("stack resi:   ", &stack_resi);
            run_and_print_tests("stack scan:   ", &stack_scan);
            run_and_print_tests("stack ring:   ", &stack_ring);
            run_and_print_tests("stack simp:   ", &stack_simp);
            println("\n");
        };


        set_up_test(80, {4, 8}, {0, 5}, false);
        tested = &stack_ring;
        test_allocs_fifo();
        test_allocs_resi();
        set_up_test(160, {4, 8}, {0, 5}, false);
        test_allocs_fifo();
        test_allocs_resi();

        //println("SMALL SIZES NO TOUCH");
        //println("===========");
        //print_benchmark_for_block(10, {4, 8}, {0, 5}, false);
        //print_benchmark_for_block(80, {4, 8}, {0, 5}, false);
        //print_benchmark_for_block(640, {4, 8}, {0, 5}, false);
        
        println("SMALL SIZES");
        println("===========");
        print_benchmark_for_block(10, {4, 8}, {0, 5});
        print_benchmark_for_block(80, {4, 8}, {0, 5});
        print_benchmark_for_block(640, {4, 8}, {0, 5});

        println("BIG SIZES");
        println("===========");
        print_benchmark_for_block(10, {8, 16}, {0, 5});
        print_benchmark_for_block(80, {8, 16}, {0, 5});
        print_benchmark_for_block(640, {8, 16}, {0, 5});
    }

}
#undef cast