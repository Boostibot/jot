#pragma once
#include <random>
#include "_test.h"

#include "memory.h"
#include "allocator_arena.h"
#include "allocator_failing.h"
#include "allocator_linear.h"
#include "allocator_stack.h"
#include "allocator_stack_ring.h"

namespace jot::tests::memory
{
    static
    void test_align()
    {
        uint8_t dummy = 0;
        uint8_t* aligned = (uint8_t*) align_forward(&dummy, 32);
        usize ptr_num = (usize) aligned;
        test(ptr_num/32*32 == ptr_num);

        test(align_forward(aligned + 1, 4) == align_backward(aligned + 7, 4));
        test(align_forward(aligned + 1, 8) == align_backward(aligned + 15, 8));
        test(align_forward(aligned + 3, 16) == align_backward(aligned + 27, 16));
        test(align_forward(aligned + 13, 16) == align_backward(aligned + 17, 16));
    }

    static
    void test_aligned_malloc()
    {
        using Seed = std::random_device::result_type;
        std::random_device rd;
        auto seed = rd();
        std::mt19937 gen(seed);
        std::uniform_int_distribution<unsigned> random;
        isize max_align = 12;
        isize max_size = 10000;

        for(isize i = 0; i < 1000; i++)
        {
            isize align = (isize) 1 << (random(gen) % max_align);
            isize size = random(gen) % max_size;

            void* res = aligned_malloc(size, align);
            test(res != nullptr);
        
            memset(res, 0, (size_t) size);

            aligned_free(res, align);
        }
    }

    static
    void test_stats_plausibility(Allocator* alloc)
    {
        Allocator::Stats stats = alloc->get_stats();

        test(stats.max_bytes_used >= stats.bytes_used);
        test(stats.max_bytes_allocated >= stats.bytes_allocated);

        test(stats.bytes_used >= stats.bytes_allocated || stats.bytes_used == stats.NOT_TRACKED);
    }
    
    static
    Slice<uint8_t> allocate_slice(Allocator* alloc, isize size, isize align, Line_Info callee) noexcept
    {
        void* ptr = alloc->allocate(size, align, callee);
        return Slice<uint8_t>{(uint8_t*) ptr, size};
    }
    
    static
    bool resize_slice(Allocator* alloc, Slice<uint8_t>* items_inout, isize new_size, isize align, Line_Info callee) noexcept
    {
        if(alloc->resize(items_inout->data, items_inout->size, new_size, align, callee))
        {
            items_inout->size = new_size;
            return true;
        }
        return false;
    }
    
    static
    bool deallocate_slice(Allocator* alloc, Slice<uint8_t> items, isize align, Line_Info callee) noexcept
    {
        return alloc->deallocate(items.data, items.size, align, callee);
    }

    static
    void test_stack_ring()
    {
        Failing_Allocator failing;
        //We test if the wrap around works correctly.
        //We also test if the size tracking is plausible
        alignas(256) uint8_t stack_ring_storage[400];
        {
            Stack_Ring_Allocator stack_ring = Stack_Ring_Allocator(stack_ring_storage, 400, &failing);
            test_stats_plausibility(&stack_ring);

            Slice<uint8_t> first = allocate_slice(&stack_ring, 10, 8, GET_LINE_INFO());
            Slice<uint8_t> second = allocate_slice(&stack_ring, 20, 256, GET_LINE_INFO());
            Slice<uint8_t> third = allocate_slice(&stack_ring, 30, 8, GET_LINE_INFO());

            test_stats_plausibility(&stack_ring);
            test(deallocate_slice(&stack_ring, second, 256, GET_LINE_INFO()));

            bool s1 = resize_slice(&stack_ring, &first, 8, 25, GET_LINE_INFO());
            test(s1 == true);
            
            bool s2 = resize_slice(&stack_ring, &first, 8, 40 + 256, GET_LINE_INFO());
            test(s2 == false); 

            test(deallocate_slice(&stack_ring, first, 8, GET_LINE_INFO()));
            test(deallocate_slice(&stack_ring, third, 8, GET_LINE_INFO()));
            test_stats_plausibility(&stack_ring);
        }

        {
            Stack_Ring_Allocator stack_ring = Stack_Ring_Allocator(stack_ring_storage, 256, &failing);
            Slice<uint8_t> a1 = allocate_slice(&stack_ring, 64, 8, GET_LINE_INFO());
            Slice<uint8_t> a2 = allocate_slice(&stack_ring, 64, 8, GET_LINE_INFO());
            Slice<uint8_t> a3 = allocate_slice(&stack_ring, 64, 8, GET_LINE_INFO());
            test(a1.data != nullptr && a2.data != nullptr && a2.data != nullptr);

            test_stats_plausibility(&stack_ring);
            
            test(deallocate_slice(&stack_ring, a1, 8, GET_LINE_INFO()));
            test(deallocate_slice(&stack_ring, a2, 8, GET_LINE_INFO()));

            Slice<uint8_t> a4 = allocate_slice(&stack_ring, 64, 8, GET_LINE_INFO());
            Slice<uint8_t> a5 = allocate_slice(&stack_ring, 64, 8, GET_LINE_INFO());
            test(a4.data != nullptr && a5.data != nullptr);

            test(stack_ring.allocate(64, 8, GET_LINE_INFO()) == nullptr);
            
            test(deallocate_slice(&stack_ring, a3, 8, GET_LINE_INFO()));
            test(deallocate_slice(&stack_ring, a4, 8, GET_LINE_INFO()));
            test(deallocate_slice(&stack_ring, a5, 8, GET_LINE_INFO()));
            test_stats_plausibility(&stack_ring);
        }
    }
    
    static
    void stress_test()
    {
        struct IRange 
        {
            isize from;
            isize to;
        };

        enum Touch
        {
            TOUCH,
            ONLY_ALLOC
        };

        using Generator = std::mt19937;
        isize block_size = 100;
        isize max_time = 250;
        Touch touch = TOUCH;

        Generator gen;
        std::uniform_int_distribution<long long> size_distribution;
        std::uniform_int_distribution<long long> size_noise_distribution;
        std::uniform_int_distribution<long long> align_distribution;

        Array<isize> size_array;
        Array<isize> align_array;
        Array<void*> alloc_array;
        isize total_size_in_size_array = 0;
        
        isize max_alloced_storage = 320 * memory_constants::MEBI_BYTE;
        Array<uint8_t> stack_storage;
        Array<uint8_t> stack_ring_storage;
        Array<uint8_t> linear_storage;
        
        resize(&stack_storage, max_alloced_storage);
        resize(&stack_ring_storage, max_alloced_storage);
        resize(&linear_storage, max_alloced_storage);
        
        Allocator*              def = default_allocator();
        Malloc_Allocator        malloc;
        Failing_Allocator       failling;
        Linear_Allocator        linear     = Linear_Allocator(data(&linear_storage), size(linear_storage), def);
        Stack_Allocator         stack      = Stack_Allocator(data(&stack_storage), size(stack_storage), def);
        Stack_Ring_Allocator    stack_ring = Stack_Ring_Allocator(data(&stack_ring_storage), size(stack_ring_storage), def);
        Arena_Allocator         arena      = Arena_Allocator(def);

        const auto set_up_test = [&](
            isize block_size_,
            IRange size_log2,
            IRange align_log2,
            Touch touch_,
            isize max_time_ = 250)
        {
            size_distribution = std::uniform_int_distribution<long long>(size_log2.from, size_log2.to);
            align_distribution = std::uniform_int_distribution<long long>(align_log2.from, align_log2.to);
            size_noise_distribution = std::uniform_int_distribution<long long>(0, 10);

            block_size = block_size_;
            resize(&size_array, block_size);
            resize(&align_array, block_size);
            resize(&alloc_array, block_size);

            total_size_in_size_array = 0;
            for(isize i = 0; i < block_size; i++)
            {
                size_array[i]  = (isize) ((1ll << size_distribution(gen)) + size_noise_distribution(gen));
                align_array[i] = (isize) (1ll << align_distribution(gen));
                total_size_in_size_array += size_array[i];
            }

            max_time = max_time_;
            touch = touch_;
        };
        
        const auto fill_slice = [&](Slice<uint8_t> slice){
            if(touch == ONLY_ALLOC)
                return;

            Slice<u32> u32s = cast_slice<u32>(slice);
            for(isize i = 0; i < u32s.size; i++)
                u32s[i] = 0xAABBCCDD;
        };

        //allocate in order then deallocate in the same order
        const auto test_allocs_fifo = [&](Allocator* tested) {
            test_stats_plausibility(tested);
            isize alloced_before = tested->get_stats().bytes_allocated;

            for(isize i = 0; i < block_size; i++)
            {
                alloc_array[i] = tested->allocate(size_array[i], align_array[i], GET_LINE_INFO());
                test(alloc_array[i] != nullptr);

                if(touch == TOUCH)
                    memset(alloc_array[i], 255, (size_t) size_array[i]);
            }

            for(isize i = 0; i < block_size; i++)
                test(tested->deallocate(alloc_array[i], size_array[i], align_array[i], GET_LINE_INFO()));
            
            isize alloced_after = tested->get_stats().bytes_allocated;
            test(alloced_before == alloced_after);

            test_stats_plausibility(tested);

            arena.reset();
        };   
        
        //allocate in order then deallocate in the opposite order
        const auto test_allocs_lifo = [&](Allocator* tested) {
            test_stats_plausibility(tested);
            isize alloced_before = tested->get_stats().bytes_allocated;

            for(isize i = 0; i < block_size; i++)
            {
                alloc_array[i] = tested->allocate(size_array[i], align_array[i], GET_LINE_INFO());
                test(alloc_array[i] != nullptr);

                if(touch == TOUCH)
                    memset(alloc_array[i], 255, (size_t) size_array[i]);
            }

            for(isize i = block_size; i-- > 0;)
                test(tested->deallocate(alloc_array[i], size_array[i], align_array[i], GET_LINE_INFO()));
                
            isize alloced_after = tested->get_stats().bytes_allocated;
            test(alloced_before == alloced_after);
            test_stats_plausibility(tested);
            arena.reset();
        };   
        
        //allocate and then imidietelly deallocate in a loop
        const auto test_allocs_temp = [&](Allocator* tested) {
            test_stats_plausibility(tested);

            for(isize i = 0; i < block_size; i++)
            {
                alloc_array[i] = tested->allocate(size_array[i], align_array[i], GET_LINE_INFO());
                test(alloc_array[i] != nullptr);

                if(touch == TOUCH)
                    memset(alloc_array[i], 255, (size_t) size_array[i]);
                
                test(tested->deallocate(alloc_array[i], size_array[i], align_array[i], GET_LINE_INFO()));
            }

            test_stats_plausibility(tested);
            arena.reset();
        };


        //allocate in order then deallocate half of the allocations
        // in the same order then try to reize the remaining allocations 
        // to two times their original size. if cant resize allocates them instead.
        // then deallocates all remaining in order.
        const auto test_allocs_resi = [&](Allocator* tested) {
            test_stats_plausibility(tested);
            isize alloced_before = tested->get_stats().bytes_allocated;

            for(isize i = 0; i < block_size; i++)
            {
                alloc_array[i] = tested->allocate(size_array[i], align_array[i], GET_LINE_INFO());
                test(alloc_array[i] != nullptr);

                if(touch == TOUCH)
                    memset(alloc_array[i], 255, (size_t) size_array[i]);
            }

            for(isize i = 0; i < block_size; i += 2)
                test(tested->deallocate(alloc_array[i], size_array[i], align_array[i], GET_LINE_INFO()));

            for(isize i = 1; i < block_size; i += 2)
            {
                uint8_t* old_data = (uint8_t*) alloc_array[i];
                isize old_size = size_array[i];
                isize align = align_array[i];
                isize new_size = old_size * 3/2;

                if(tested->resize(old_data, old_size, new_size, align, GET_LINE_INFO()) == false)
                {
                    uint8_t* new_data = (uint8_t*) tested->allocate(new_size, align, GET_LINE_INFO());
                    test(new_data != nullptr);

                    if(touch == TOUCH)
                        memcpy(new_data, old_data, (size_t) old_size);
                        
                    test(tested->deallocate(old_data, old_size, align, GET_LINE_INFO()));
                    old_data = new_data;
                }

                if(touch == TOUCH)
                    memset(old_data + old_size, 0, (size_t) new_size - (size_t) old_size);

                alloc_array[i] = old_data;
                size_array[i] = new_size;
            }

            for(isize i = 1; i < block_size; i += 2)
                test(tested->deallocate(alloc_array[i], size_array[i], align_array[i], GET_LINE_INFO()));
            
            isize alloced_after = tested->get_stats().bytes_allocated;
            test(alloced_before == alloced_after);

            test_stats_plausibility(tested);
            arena.reset();   
        };

        //Allocs, resizes the allocation and then imidiatelly deallocates it again in a loop
        const auto test_allocs_resize_last = [&](Allocator* tested) {
            test_stats_plausibility(tested);

            for(isize i = 0; i < block_size; i++)
            {
                isize old_size = size_array[i];
                isize new_size = old_size * 3/2;
                isize align = align_array[i];

                uint8_t* old_data = (uint8_t*) tested->allocate(old_size, align, GET_LINE_INFO());
                test(old_data != nullptr);

                if(touch == TOUCH)
                    memset(old_data, 255, (size_t) size_array[i]);

                    
                if(tested->resize(old_data, old_size, new_size, align, GET_LINE_INFO()) == false)
                {
                    uint8_t* new_data = (uint8_t*) tested->allocate(new_size, align, GET_LINE_INFO());
                    test(new_data != nullptr);

                    if(touch == TOUCH)
                        memcpy(new_data, old_data, (size_t) old_size);
                        
                    test(tested->deallocate(old_data, old_size, align, GET_LINE_INFO()));
                    old_data = new_data;
                }
                
                test(tested->deallocate(old_data, new_size, align, GET_LINE_INFO()));
            }

            test_stats_plausibility(tested);
            arena.reset();
        };
        
        //allocate in order then read the data 10 times (summing bytes but the op doesnt matter) and dealloc the data
        // is mostly for benchmarks
        const auto test_allocs_read = [&](Allocator* tested) {
            for(isize i = 0; i < block_size; i++)
            {
                alloc_array[i] = tested->allocate(size_array[i], align_array[i], GET_LINE_INFO());
                test(alloc_array[i] != nullptr);

                if(touch == TOUCH)
                    memset(alloc_array[i], 255, (size_t) size_array[i]);
            }

            isize sum = 0;
            for(isize j = 0; j < 100; j++)
            {
                for(isize i = 0; i < block_size; i++)
                {
                    uint8_t* ptr = (uint8_t*) alloc_array[i];
                    for(isize k = 0; k < size_array[i]; k++)
                        sum += ptr[k];
                }
            }
            
            for(isize i = 0; i < block_size; i++)
                test(tested->deallocate(alloc_array[i], size_array[i], align_array[i], GET_LINE_INFO()));

            arena.reset();
        };

        const auto test_single = [&](Allocator* tested)
        {
            test_allocs_fifo(tested);
            test_allocs_lifo(tested);
            test_allocs_temp(tested);
            test_allocs_resi(tested);
            test_allocs_resize_last(tested);
        };
        
        for(int i = 0; i < 5; i ++)
        {
            set_up_test(10, {4, 8}, {0, 5}, TOUCH);
            test_single(memory_globals::malloc_allocator());
            test_single(&arena);
            test_single(&stack_ring);
            test_single(&stack);
        
            set_up_test(200, {1, 10}, {0, 10}, TOUCH);
            test_single(memory_globals::malloc_allocator());
            test_single(&arena);
            test_single(&stack_ring);
            test_single(&stack);
        }
    }
    
    static
    void test_memory()
    {
        test_align();
        test_aligned_malloc();
        test_stack_ring();
        stress_test();
    }
}
