#pragma once
#include <random>

#include "memory.h"
#include "allocator_arena.h"
#include "allocator_stack_ring.h"
#include "allocator_ring.h"
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
        Allocator* tested = def;

        Stack<isize> size_table;
        Stack<isize> align_table;
        Stack<Slice<u8>> allocs;
        
        isize max_alloced_storage = 320 * memory_constants::MEBI_BYTE;
        Stack<u8> ring_storage;
        Stack<u8> stack_storage;
        Stack<u8> stack_simple_storage;

        force(resize(&ring_storage, max_alloced_storage));
        force(resize(&stack_storage, max_alloced_storage));
        force(resize(&stack_simple_storage, max_alloced_storage));

        Failing_Allocator       failling;
        Ring_Allocator          ring       = Ring_Allocator(slice(&ring_storage), def);
        Intrusive_Stack_Scan    stack_scan = Intrusive_Stack_Scan(slice(&ring_storage), def);
        Intrusive_Stack_Resize  stack_resi = Intrusive_Stack_Resize(slice(&ring_storage), def);
        Stack_Ring_Allocator    stack_ring = Stack_Ring_Allocator(slice(&ring_storage), def);
        Intrusive_Stack_Simple  stack_simp = Intrusive_Stack_Simple(slice(&ring_storage), def);
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

            for(isize i = 0; i < block_size; i++)
            {
                size_table[i]  = cast(isize) ((1ll << size_distribution(gen)) + size_noise_distribution(gen));
                align_table[i] = cast(isize) (1ll << align_distribution(gen));
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
        
        //allocate in order then deallocate in the opposite order
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
        
        //allocate and then imidietelly deallocate in a loop
        const auto test_allocs_temp = [&]() {

            for(isize i = 0; i < block_size; i++)
            {
                Allocation_Result result = tested->allocate(size_table[i], align_table[i]);
                force(result.state);
                fill_slice(result.items);
                force(tested->deallocate(result.items, align_table[i]));
            }

            unbound.reset();
        };

        //allocate in order then deallocate half of the allocations
        // in the same order then try to reize the remaining allocations 
        // to two times their original size. if cant resize allocates them instead.
        // then deallocates all remaining in order.
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

            unbound.reset();   
        };
        
        //allocate in order then read the data 10 times (summing bytes but the op doesnt matter) and dealloc the data
        // is mostly for benchmarks
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

        const auto test_single = [&](Allocator* tested_)
        {
              tested = tested_;

              test_allocs_fifo();
              test_allocs_lifo();
              test_allocs_temp();
              test_allocs_resi();
              test_allocs_read();
        };

        for(int i = 0; i < 10; i ++)
        {
            set_up_test(10, {4, 8}, {0, 5}, true);
            test_single(&memory_globals::NEW_DELETE_ALLOCATOR);
            test_single(&unbound);
            test_single(&ring);
            test_single(&stack_resi);
            test_single(&stack_scan);
            test_single(&stack_ring);
            test_single(&stack_simp);
        
            set_up_test(100, {1, 10}, {0, 10}, true);
            test_single(&memory_globals::NEW_DELETE_ALLOCATOR);
            test_single(&unbound);
            test_single(&ring);
            test_single(&stack_resi);
            test_single(&stack_scan);
            test_single(&stack_ring);
            test_single(&stack_simp);
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