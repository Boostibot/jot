#pragma once
#include <random>

#include "time.h"
#include "memory.h"
#include "allocator_stack.h"
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

    constexpr nodisc 
    bool is_invarinat(IRange range) {
        return range.from <= range.to;
    }

    constexpr nodisc 
    bool in_range(IRange range, isize index) {
        return (range.from <= index && index < range.to);
    }

    constexpr nodisc 
    bool in_inclusive_range(IRange range, isize index) {
        return (range.from <= index && index <= range.to);
    }

    constexpr nodisc 
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
            Stack_Ring stack_ring = Stack_Ring(Slice{stack_ring_storage, 400}, &memory_globals::FAILING_ALLOCATOR);
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
            Stack_Ring stack_ring = Stack_Ring(Slice{stack_ring_storage, 256}, &memory_globals::FAILING_ALLOCATOR);
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
        Stack_Ring stack_ring = Stack_Ring(slice(&ring_storage), def);
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
            isize max_time_ = 500,
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
        
        const auto ms_per_iter = [&](Bench_Result result, isize iters) {
            return cast(f64) result.time_ns / (result.iters * 1'000'000 * iters);
        };

        force(sizeof(Stack_Ring) == sizeof(Intrusive_Stack_Scan));

        const auto run_tests_on = [&](Allocator* tested_, f64 tested_res[4]){
            tested = tested_;
            tested_res[0] = ms_per_iter(benchmark(max_time, test_allocs_fifo), block_size);
            tested_res[1] = ms_per_iter(benchmark(max_time, test_allocs_lifo), block_size);
            tested_res[2] = ms_per_iter(benchmark(max_time, test_allocs_temp), block_size);
            tested_res[3] = ms_per_iter(benchmark(max_time, test_allocs_resi), block_size);
            tested_res[4] = ms_per_iter(benchmark(max_time, test_allocs_read), block_size);
        };

        const auto print_benchmark_for_block = [&](isize block_size_, IRange size_log2, IRange align_log2, bool touch = true){
            set_up_test(block_size_, size_log2, align_log2, touch);
            println("iters: {}", block_size);
            println("size:  {} - {}", 1ll << size_log2.from, 1ll << size_log2.to);
            println("align: {} - {}", 1ll << align_log2.from, 1ll << align_log2.to);

            f64 new_del_res[5] = {0};
            f64 unbound_res[5] = {0};
            f64 ring_res[5] = {0};
            f64 stack_simp_res[5] = {0};
            f64 stack_scan_res[5] = {0};
            f64 stack_resi_res[5] = {0};
            f64 stack_ring_res[5] = {0};
            f64* tested_res = nullptr;

            //run_tests_on(&memory_globals::NEW_DELETE_ALLOCATOR, new_del_res);
            run_tests_on(&unbound, unbound_res);
            //run_tests_on(&ring, ring_res);
            //run_tests_on(&stack_resi, stack_resi_res);
            //run_tests_on(&stack_scan, stack_scan_res);
            run_tests_on(&stack_ring, stack_ring_res);
            run_tests_on(&stack_simp, stack_simp_res);

            println("               fifo                 \t lifo              \t temp                 \t resize                \t read");
            println("new delete:    {}\t {}\t {}\t {}\t {}\t", new_del_res[0], new_del_res[1], new_del_res[2], new_del_res[2], new_del_res[4]);
            println("unbound:       {}\t {}\t {}\t {}\t {}\t", unbound_res[0], unbound_res[1], unbound_res[2], unbound_res[3], unbound_res[4]);
            println("ring:          {}\t {}\t {}\t {}\t {}\t", ring_res[0], ring_res[1], ring_res[2], ring_res[3], ring_res[4]);
            println("stack resi:    {}\t {}\t {}\t {}\t {}\t", stack_resi_res[0], stack_resi_res[1], stack_resi_res[2], stack_resi_res[3], stack_resi_res[4]);
            println("stack scan:    {}\t {}\t {}\t {}\t {}\t", stack_scan_res[0], stack_scan_res[1], stack_scan_res[2], stack_scan_res[3], stack_scan_res[4]);
            println("stack ring:    {}\t {}\t {}\t {}\t {}\t", stack_ring_res[0], stack_ring_res[1], stack_ring_res[2], stack_ring_res[3], stack_ring_res[4]);
            println("stack simp:    {}\t {}\t {}\t {}\t {}\t", stack_simp_res[0], stack_simp_res[1], stack_simp_res[2], stack_simp_res[3], stack_simp_res[4]);
            println("\n");
        };


        set_up_test(80, {4, 8}, {0, 5}, false);
        tested = &stack_ring;
        test_allocs_fifo();
        test_allocs_resi();
        set_up_test(160, {4, 8}, {0, 5}, false);
        test_allocs_fifo();
        test_allocs_resi();

        println("SMALL SIZES NO TOUCH");
        println("===========");
        print_benchmark_for_block(10, {4, 8}, {0, 5}, false);
        print_benchmark_for_block(80, {4, 8}, {0, 5}, false);
        print_benchmark_for_block(640, {4, 8}, {0, 5}, false);
        
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