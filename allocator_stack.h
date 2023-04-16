#pragma once
#include "memory.h"

namespace jot
{
    struct Stack_Allocator : Allocator
    {
        uint8_t* buffer_from = nullptr;
        uint8_t* buffer_to = nullptr;
        uint8_t* last_block_to = nullptr;
        uint8_t* last_block_from = nullptr;

        isize max_alloced = 0;
        isize current_alloced = 0;

        Allocator* parent = nullptr;

        struct Slot
        {
            uint64_t prev_offset;
        };
        
        static constexpr uint64_t USED_BIT = (uint64_t) 1 << 63;

        Stack_Allocator(void* buffer, isize buffer_size, Allocator* parent = default_allocator()) 
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

            if(align <= sizeof(Slot))
                align = sizeof(Slot);

            //get the adress at which to place Slot header
            uint8_t* available_from = last_block_to + sizeof(Slot);

            uint8_t* aligned_from = (uint8_t*) align_forward(available_from, align);
            uint8_t* aligned_to = aligned_from + size;

            if(aligned_to > buffer_to) 
                return parent->allocate(size, size, callee);

            Slot* slot = ((Slot*) aligned_from) - 1;
            slot->prev_offset = (uint64_t) ptrdiff(slot, last_block_from) | USED_BIT; 

            current_alloced += size;
            if(max_alloced < current_alloced)
                max_alloced = current_alloced;

            last_block_to = aligned_to;
            last_block_from = aligned_from;

            assert(last_block_to >= last_block_from);
            assert(buffer_from <= last_block_to && last_block_to <= buffer_to);
            assert(buffer_from <= last_block_from && last_block_from <= buffer_to);

            return aligned_from;
        }

        virtual 
        bool deallocate(void* allocated, isize old_size, isize align, Line_Info callee) noexcept override
        {
            assert(old_size >= 0 && is_power_of_two(align));
            uint8_t* ptr = (uint8_t*) allocated;
            if(ptr < buffer_from || buffer_to <= ptr) 
                return parent->deallocate(allocated, old_size, align, callee);

            Slot *slot = ((Slot*) allocated) - 1;
            slot->prev_offset = slot->prev_offset & ~USED_BIT;

            current_alloced -= old_size;

            while (true) 
            {
                Slot* last_slot = ((Slot*) last_block_from) - 1;
                if(last_slot->prev_offset & USED_BIT)
                    return true;

                last_block_from = ((uint8_t*) last_slot) - last_slot->prev_offset;
                last_block_to = (uint8_t*) last_slot;

                if(last_block_from <= buffer_from)
                {
                    last_block_from = buffer_from;
                    last_block_to = buffer_from;
                    return true;
                }
            }

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
        Stats get_stats() const noexcept override
        {
            Stats stats = {};
            stats.name = "Stack_Allocator";
            stats.supports_resize = true;
            stats.parent = parent;
            stats.bytes_allocated = current_alloced;
            stats.bytes_used = buffer_to - buffer_from;

            stats.max_bytes_allocated = max_alloced;
            stats.max_bytes_used = stats.bytes_used;
            
            return stats;
        }

        virtual
        ~Stack_Allocator() noexcept override {}
    };
}

