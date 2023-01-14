#pragma once
#include <random>

#include "time.h"
#include "memory.h"
#include "allocator_stack.h"
#include "allocator_ring.h"
#include "format.h"

#define cast(...) (__VA_ARGS__)

void test_allocators()
{
    using namespace jot;

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
    Ring_Allocator  ring = Ring_Allocator(slice(&ring_storage), &failling);
    //Intrusive_Stack stack = Intrusive_Stack(slice(&ring_storage), &failling);
    Intrusive_Stack_Scan stack_scan = Intrusive_Stack_Scan(slice(&ring_storage), &failling);
    Intrusive_Stack_Resize stack_resize = Intrusive_Stack_Resize(slice(&ring_storage), &failling);
    Intrusive_Stack_Simple stack_simple = Intrusive_Stack_Simple(slice(&ring_storage), &failling);
    Unbound_Stack_Allocator unbound = Unbound_Stack_Allocator(def);

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
            size_table[i] = (1ll << size_distribution(gen)) + size_noise_distribution(gen);
            align_table[i] = 1ll << align_distribution(gen);
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
                {
                    Slice<u8> before_added = trim(result.items, old_size);
                    memcpy_slice<u8>(&before_added, old_data);
                }

                force(tested->deallocate(old_data, align));
            }

            allocs[i] = result.items;
            Slice<u8> added = slice(result.items, old_size);
            if(touch)
                null_slice(&added);
        }

        for(isize i = 1; i < block_size; i += 2)
        {
            if(allocs[i].data != nullptr)
                force(tested->deallocate(allocs[i], align_table[i]));
        }

        unbound.reset();   
    };


    const auto ms_per_iter = [&](Bench_Result result, isize iters) {
        return cast(f64) result.time_ns / (result.iters * 1'000'000 * iters);
    };

    const auto print_benchmark_for_block = [&](isize block_size_, IRange size_log2, IRange align_log2, bool touch = true){
        set_up_test(block_size_, size_log2, align_log2, touch);
        println("iters: {}", block_size);
        println("size:  {} - {}", 1ll << size_log2.from, 1ll << size_log2.to);
        println("align: {} - {}", 1ll << align_log2.from, 1ll << align_log2.to);

        f64 new_del_res[4];
        f64 unbound_res[4];
        f64 ring_res[4];
        f64 stack_simp_res[4];
        f64 stack_scan_res[4];
        f64 stack_resi_res[4];
        f64* teste_res;

        tested = &memory_globals::NEW_DELETE_ALLOCATOR;
        teste_res = new_del_res;
        teste_res[0] = ms_per_iter(benchmark(max_time, test_allocs_fifo), block_size);
        teste_res[1] = ms_per_iter(benchmark(max_time, test_allocs_lifo), block_size);
        teste_res[2] = ms_per_iter(benchmark(max_time, test_allocs_temp), block_size);
        teste_res[3] = ms_per_iter(benchmark(max_time, test_allocs_resi), block_size);

        tested = &unbound;
        teste_res = unbound_res;
        teste_res[0] = ms_per_iter(benchmark(max_time, test_allocs_fifo), block_size);
        teste_res[1] = ms_per_iter(benchmark(max_time, test_allocs_lifo), block_size);
        teste_res[2] = ms_per_iter(benchmark(max_time, test_allocs_temp), block_size);
        teste_res[3] = ms_per_iter(benchmark(max_time, test_allocs_resi), block_size);

        tested = &ring;
        teste_res = ring_res;
        teste_res[0] = ms_per_iter(benchmark(max_time, test_allocs_fifo), block_size);
        teste_res[1] = ms_per_iter(benchmark(max_time, test_allocs_lifo), block_size);
        teste_res[2] = ms_per_iter(benchmark(max_time, test_allocs_temp), block_size);
        teste_res[3] = ms_per_iter(benchmark(max_time, test_allocs_resi), block_size);

        tested = &stack_resize;
        teste_res = stack_resi_res;
        teste_res[0] = ms_per_iter(benchmark(max_time, test_allocs_fifo), block_size);
        teste_res[1] = ms_per_iter(benchmark(max_time, test_allocs_lifo), block_size);
        teste_res[2] = ms_per_iter(benchmark(max_time, test_allocs_temp), block_size);
        teste_res[3] = ms_per_iter(benchmark(max_time, test_allocs_resi), block_size);

        tested = &stack_scan;
        teste_res = stack_scan_res;
        teste_res[0] = ms_per_iter(benchmark(max_time, test_allocs_fifo), block_size);
        teste_res[1] = ms_per_iter(benchmark(max_time, test_allocs_lifo), block_size);
        teste_res[2] = ms_per_iter(benchmark(max_time, test_allocs_temp), block_size);
        teste_res[3] = ms_per_iter(benchmark(max_time, test_allocs_resi), block_size);

        tested = &stack_simple;
        teste_res = stack_simp_res;
        teste_res[0] = ms_per_iter(benchmark(max_time, test_allocs_fifo), block_size);
        teste_res[1] = ms_per_iter(benchmark(max_time, test_allocs_lifo), block_size);
        teste_res[2] = ms_per_iter(benchmark(max_time, test_allocs_temp), block_size);
        teste_res[3] = ms_per_iter(benchmark(max_time, test_allocs_resi), block_size);

        println("new delete:    {}\t {}\t {}\t {}\t", new_del_res[0], new_del_res[1], new_del_res[2], new_del_res[2]);
        println("unbound:       {}\t {}\t {}\t {}\t", unbound_res[0], unbound_res[1], unbound_res[2], unbound_res[3]);
        println("ring:          {}\t {}\t {}\t {}\t", ring_res[0], ring_res[1], ring_res[2], ring_res[3]);
        println("stack resize:  {}\t {}\t {}\t {}\t", stack_resi_res[0], stack_resi_res[1], stack_resi_res[2], stack_resi_res[3]);
        println("stack scan:    {}\t {}\t {}\t {}\t", stack_scan_res[0], stack_scan_res[1], stack_scan_res[2], stack_scan_res[3]);
        println("stack simple:  {}\t {}\t {}\t {}\t", stack_simp_res[0], stack_simp_res[1], stack_simp_res[2], stack_simp_res[3]);
        println("\n");
    };

    {
        u8 arr[256] = {0};
        Slice<u8> arr_s = {arr, 64};
        Slice<u8> aligned_s = align_forward(arr_s, 32);
        u8* aligned = aligned_s.data;

        assert(align_forward(aligned + 1, 4) == align_backward(aligned + 7, 4));
        assert(align_forward(aligned + 1, 8) == align_backward(aligned + 15, 8));
        assert(align_forward(aligned + 3, 16) == align_backward(aligned + 27, 16));
        assert(align_forward(aligned + 13, 16) == align_backward(aligned + 17, 16));
    }


    #if 0
    Slice<u8> zeroth = stack.allocate(1000, 8).items;
    Slice<u8> first = stack.allocate(10, 8).items;
    Slice<u8> second = stack.allocate(20, 256).items;
    Slice<u8> third = stack.allocate(30, 8).items;

    force(stack.deallocate(zeroth, 8));
    force(stack.deallocate(second, 8));

    Allocation_Result result = stack.resize(first, 8, 25);
    force(result.state);
    first = result.items;

    result = stack.resize(first, 8, 40 + 256);
    assert(result.state == ERROR);

    force(stack.deallocate(first, 8));
    force(stack.deallocate(third, 8));
    #endif

    //set_up_test(2, {4, 8}, {0, 5}, false);
    //tested = &stack;
    //test_allocs_resi();

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

#undef cast