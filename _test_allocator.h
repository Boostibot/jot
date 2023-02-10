#pragma once
#include <random>

#include "memory.h"
#include "allocator_arena.h"
#include "allocator_stack_ring.h"
#include "allocator_stack.h"
#include "stack.h"
#include "_test.h"
#include "defines.h"

namespace jot::tests::allocator
{
    template<typename T>
    struct Range 
    {
        T from;
        T to;
    };

    using IRange = Range<isize>;

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

    const auto test_stats_plausibility(Allocator* tested_)
    {
        isize alloced = tested_->bytes_allocated();
        isize max_alloced = tested_->max_bytes_allocated();
        isize used = tested_->bytes_used();
        isize max_used = tested_->max_bytes_used();

        test(max_used >= used);
        test(max_alloced >= alloced);
        test(used >= alloced || used == Allocator::SIZE_NOT_TRACKED);
    }

    void test_stack_ring()
    {
        //We test if the wrap around works correctly.
        //We also test if the size tracking is plausible
        alignas(256) u8 stack_ring_storage[400];
        {
            Stack_Ring_Allocator stack_ring = Stack_Ring_Allocator(Slice{stack_ring_storage, 400}, &memory_globals::FAILING_ALLOCATOR);
            test_stats_plausibility(&stack_ring);

            Slice<u8> first = stack_ring.allocate(10, 8).items;
            Slice<u8> second = stack_ring.allocate(20, 256).items;
            Slice<u8> third = stack_ring.allocate(30, 8).items;

            test_stats_plausibility(&stack_ring);
            force(stack_ring.deallocate(second, 256));

            Allocation_Result result = stack_ring.resize(first, 8, 25);
            force(result.state);
            first = result.items;

            result = stack_ring.resize(first, 8, 40 + 256);
            test(result.state == ERROR);

            force(stack_ring.deallocate(first, 8));
            force(stack_ring.deallocate(third, 8));
            test_stats_plausibility(&stack_ring);
        }

        {
            Stack_Ring_Allocator stack_ring = Stack_Ring_Allocator(Slice{stack_ring_storage, 256}, &memory_globals::FAILING_ALLOCATOR);
            Slice<u8> a1 = stack_ring.allocate(64, 8).items;
            Slice<u8> a2 = stack_ring.allocate(64, 8).items;
            Slice<u8> a3 = stack_ring.allocate(64, 8).items;
            test_stats_plausibility(&stack_ring);

            force(stack_ring.deallocate(a1, 8));
            force(stack_ring.deallocate(a2, 8));
            
            Slice<u8> a4 = stack_ring.allocate(64, 8).items;
            Slice<u8> a5 = stack_ring.allocate(64, 8).items;

            test(stack_ring.allocate(64, 8).state == ERROR);
            
            force(stack_ring.deallocate(a3, 8));
            force(stack_ring.deallocate(a4, 8));
            force(stack_ring.deallocate(a5, 8));
            test_stats_plausibility(&stack_ring);
        }
    }

    void stress_test()
    {
        using Generator = std::mt19937;
        isize block_size = 100;
        isize max_time = 250;

        Generator gen;
        std::uniform_int_distribution<long long> size_distribution;
        std::uniform_int_distribution<long long> size_noise_distribution;
        std::uniform_int_distribution<long long> align_distribution;
        
        bool touch = true;
        Allocator* def = memory_globals::default_allocator();

        Stack<isize> size_table;
        Stack<isize> align_table;
        Stack<Slice<u8>> allocs;
        isize total_size_in_size_table = 0;
        
        isize max_alloced_storage = 320 * memory_constants::MEBI_BYTE;
        Stack<u8> stack_storage;
        Stack<u8> stack_simple_storage;

        force(resize(&stack_storage, max_alloced_storage));
        force(resize(&stack_simple_storage, max_alloced_storage));

        Failing_Allocator       failling;
        Stack_Ring_Allocator    stack_ring = Stack_Ring_Allocator(slice(&stack_storage), def);
        Stack_Allocator         stack = Stack_Allocator(slice(&stack_simple_storage), def);
        Arena_Allocator         unbound    = Arena_Allocator(def);

        const auto set_up_test = [&](
            isize block_size_,
            IRange size_log2,
            IRange align_log2,
            bool touch_,
            isize max_time_ = 250)
        {
            size_distribution = std::uniform_int_distribution<long long>(size_log2.from, size_log2.to);
            align_distribution = std::uniform_int_distribution<long long>(align_log2.from, align_log2.to);
            size_noise_distribution = std::uniform_int_distribution<long long>(0, 10);

            block_size = block_size_;
            force(resize(&size_table, block_size));
            force(resize(&align_table, block_size));
            force(resize(&allocs, block_size));

            total_size_in_size_table = 0;
            for(isize i = 0; i < block_size; i++)
            {
                size_table[i]  = cast(isize) ((1ll << size_distribution(gen)) + size_noise_distribution(gen));
                align_table[i] = cast(isize) (1ll << align_distribution(gen));
                total_size_in_size_table += size_table[i];
            }

            max_time = max_time_;
            touch = touch_;
        };
        
        const auto fill_slice = [&](Slice<u8> slice){
            if(touch == false)
                return;

            Slice<u32> u32s = cast_slice<u32>(slice);
            for(isize i = 0; i < u32s.size; i++)
                u32s[i] = 0xAABBCCDD;
        };

        //allocate in order then deallocate in the same order
        const auto test_allocs_fifo = [&](Allocator* tested) {
            test_stats_plausibility(tested);
            isize alloced_before = tested->bytes_allocated();

            for(isize i = 0; i < block_size; i++)
            {
                Allocation_Result result = tested->allocate(size_table[i], align_table[i]);
                force(result.state);
                fill_slice(result.items);
                allocs[i] = result.items;
            }
            
            isize alloced_max = tested->bytes_allocated();
            test(alloced_max >= alloced_before + total_size_in_size_table  //the size must reflect the alloced size
                || alloced_max == alloced_before                           // or it can be not tracked and be a constant
            );

            for(isize i = 0; i < block_size; i++)
                force(tested->deallocate(allocs[i], align_table[i]));
            
            test_stats_plausibility(tested);

            unbound.reset();
        };   
        
        //allocate in order then deallocate in the opposite order
        const auto test_allocs_lifo = [&](Allocator* tested) {
            test_stats_plausibility(tested);
            isize alloced_before = tested->bytes_allocated();

            for(isize i = 0; i < block_size; i++)
            {
                Allocation_Result result = tested->allocate(size_table[i], align_table[i]);
                force(result.state);
                fill_slice(result.items);
                allocs[i] = result.items;
            }

            isize alloced_max = tested->bytes_allocated();
            test(alloced_max >= alloced_before + total_size_in_size_table
                || alloced_max == alloced_before                         
            );

            for(isize i = block_size; i-- > 0;)
                force(tested->deallocate(allocs[i], align_table[i]));
                
            test_stats_plausibility(tested);
            unbound.reset();
        };   
        
        //allocate and then imidietelly deallocate in a loop
        const auto test_allocs_temp = [&](Allocator* tested) {
            test_stats_plausibility(tested);

            for(isize i = 0; i < block_size; i++)
            {
                Allocation_Result result = tested->allocate(size_table[i], align_table[i]);
                force(result.state);
                fill_slice(result.items);
                force(tested->deallocate(result.items, align_table[i]));
            }

            test_stats_plausibility(tested);
            unbound.reset();
        };


        //allocate in order then deallocate half of the allocations
        // in the same order then try to reize the remaining allocations 
        // to two times their original size. if cant resize allocates them instead.
        // then deallocates all remaining in order.
        const auto test_allocs_resi = [&](Allocator* tested) {
            test_stats_plausibility(tested);
            isize alloced_before = tested->bytes_allocated();

            for(isize i = 0; i < block_size; i++)
            {
                Allocation_Result result = tested->allocate(size_table[i], align_table[i]);
                force(result.state);
                fill_slice(result.items);
                allocs[i] = result.items;
            }

            isize alloced_max = tested->bytes_allocated();
            test(alloced_max >= alloced_before + total_size_in_size_table
                || alloced_max == alloced_before                         
            );

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
                        copy_items<u8>(&result.items, old_data);

                    force(tested->deallocate(old_data, align));
                }

                allocs[i] = result.items;
                Slice<u8> added = slice(result.items, old_size);
                if(touch)
                    null_items(&added);
            }

            for(isize i = 1; i < block_size; i += 2)
            {
                if(allocs[i].data != nullptr)
                    force(tested->deallocate(allocs[i], align_table[i]));
            }
            
            test_stats_plausibility(tested);
            unbound.reset();   
        };

        //Allocs, resizes the allocation and then imidiatelly deallocates it again in a loop
        const auto test_allocs_resize_last = [&](Allocator* tested) {
            test_stats_plausibility(tested);

            for(isize i = 0; i < block_size; i++)
            {
                isize size = size_table[i];
                isize new_size = size * 2;
                isize align = align_table[i];

                Allocation_Result result = tested->allocate(size, align);
                force(result.state);
                fill_slice(result.items);

                Allocation_Result resize_result = tested->resize(result.items, align, new_size);
                if(resize_result.state == ERROR)
                {
                    resize_result = tested->allocate(new_size, align);
                    force(resize_result.state);

                    if(touch)
                        copy_items<u8>(&resize_result.items, result.items);

                    force(tested->deallocate(result.items, align));
                }

                force(resize_result.state);
                fill_slice(resize_result.items);

                force(tested->deallocate(resize_result.items, align));
            }

            test_stats_plausibility(tested);
            unbound.reset();
        };
        
        //allocate in order then read the data 10 times (summing bytes but the op doesnt matter) and dealloc the data
        // is mostly for benchmarks
        const auto test_allocs_read = [&](Allocator* tested) {
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

        const auto test_single = [&](Allocator* tested)
        {
            test_allocs_fifo(tested);
            test_allocs_lifo(tested);
            test_allocs_temp(tested);
            test_allocs_resi(tested);
            test_allocs_resize_last(tested);
            //test_allocs_read(tested);
        };
        
        for(int i = 0; i < 5; i ++)
        {
            set_up_test(10, {4, 8}, {0, 5}, true);
            test_single(&memory_globals::NEW_DELETE_ALLOCATOR);
            test_single(&unbound);
            test_single(&stack_ring);
            test_single(&stack);
        
            set_up_test(200, {1, 10}, {0, 10}, true);
            test_single(&memory_globals::NEW_DELETE_ALLOCATOR);
            test_single(&unbound);
            test_single(&stack_ring);
            test_single(&stack);
        }
    }
    
    void test_allocators()
    {
        test_align();
        test_stack_ring();
        stress_test();
    }
}
#undef cast