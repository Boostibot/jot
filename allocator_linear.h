#pragma once

#include "memory.h"

namespace jot
{
    ///Allocates linearry from a buffer. Once the buffer is filled no more allocations are possible
    struct Linear_Allocator : Allocator
    {
        uint8_t* buffer_from = nullptr;
        uint8_t* buffer_to = nullptr;
        uint8_t* last_block_to = nullptr;
        uint8_t* last_block_from = nullptr;

        isize max_alloced = 0;
        isize current_alloced = 0;

        Allocator* parent = nullptr;

        Linear_Allocator(void* buffer, isize buffer_size, Allocator* parent = default_allocator()) 
            : parent(parent) 
        {
            buffer_from = (uint8_t*) buffer;
            buffer_to = buffer_from + buffer_size ;

            last_block_to = buffer_from;
            last_block_from = buffer_from;
        }
        
        virtual
        void* allocate(isize size, isize align, Line_Info callee) noexcept override
        {
            assert(size >= 0 && is_power_of_two(align));

            uint8_t* aligned_from = (uint8_t*) align_forward(last_block_to, align);
            uint8_t* aligned_to = aligned_from + size;

            if(aligned_to > buffer_to) 
                return parent->allocate(size, align, callee);

            current_alloced += size;
            if(max_alloced < current_alloced)
                max_alloced = current_alloced;

            last_block_to = aligned_to;
            last_block_from = aligned_from;

            return aligned_from;
        }

        virtual 
        bool deallocate(void* allocated, isize old_size, isize align, Line_Info callee) noexcept override
        {
            assert(old_size >= 0 && is_power_of_two(align));
            uint8_t* ptr = (uint8_t*) allocated;
            if(ptr < buffer_from || buffer_to <= ptr) 
                return parent->deallocate(allocated, old_size, align, callee);

            //if was last reset the ptr
            if(ptr == last_block_from && ptr + old_size == last_block_to)
                last_block_to = last_block_from;

            current_alloced -= old_size;

            return true;
        } 
        
        virtual
        bool resize(void* allocated, isize old_size, isize new_size, isize align, Line_Info callee) noexcept override 
        {
            assert(new_size >= 0 && is_power_of_two(align));
            uint8_t* ptr = (uint8_t*) allocated;

            if(ptr < buffer_from || buffer_to <= ptr) 
                return parent->resize(allocated, old_size, new_size, align, callee);

            //if is last block resize else fail
            if(last_block_from != allocated)
                return false;

            //if too big fail
            if(ptr + new_size > buffer_to)
                return false;

            last_block_to = ptr + new_size;
            current_alloced += new_size - old_size;
            return false;
        }
        
        virtual
        Allocator_Stats get_stats() const noexcept override
        {
            Allocator_Stats stats = {};
            stats.name = "Linear_Allocator";
            stats.supports_resize = true;
            stats.parent = parent;
            stats.bytes_allocated = current_alloced;
            stats.bytes_used = buffer_to - buffer_from;

            stats.max_bytes_allocated = max_alloced;
            stats.max_bytes_used = stats.bytes_used;
            
            return stats;
        }

        virtual
        ~Linear_Allocator() noexcept override {}
    };
}

