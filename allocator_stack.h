#pragma once
#include "memory.h"
#include "defines.h"

namespace jot
{
    struct Stack_Allocator : Allocator
    {
        u8* buffer_from = nullptr;
        u8* buffer_to = nullptr;
        u8* last_block_to = nullptr;
        u8* last_block_from = nullptr;

        isize max_alloced = 0;
        isize current_alloced = 0;

        Allocator* parent = nullptr;

        struct Slot
        {
            u64 prev_offset;
        };
        
        static constexpr u32 USED_BIT = cast(u32) 1 << 31;

        Stack_Allocator(Slice<u8> buffer, Allocator* parent) 
            : parent(parent) 
        {
            Slice<u8> aligned = align_forward(buffer, alignof(Slot));

            buffer_from = aligned.data;
            buffer_to = aligned.data + aligned.size;

            last_block_to = buffer_from;
            last_block_from = buffer_from;
        }

        nodisc virtual 
        Allocation_Result allocate(isize size, isize align) noexcept override 
        {
            assert_arg(size >= 0 && align > 0);

            if(align <= alignof(Slot))
                align = alignof(Slot);

            //get the adress at which to place Slot header
            u8* available_from = last_block_to + sizeof(Slot);

            u8* aligned_from = align_forward(available_from, align);
            u8* aligned_to = aligned_from + size;

            isize aligned_size = size;

            u8 is_allowed  = aligned_size >= USED_BIT;
            u8 is_past_end = aligned_to > buffer_to;
            if(is_allowed | is_past_end) 
                return parent->allocate(size, align);

            Slot* slot = (cast(Slot*) aligned_from) - 1;
            slot->prev_offset = cast(u64) ptrdiff(slot, last_block_from) | USED_BIT; 

            current_alloced += size;
            max_alloced = max(max_alloced, current_alloced);

            Slice<u8> output = {aligned_from, size};
            last_block_to = aligned_to;
            last_block_from = aligned_from;

            assert(last_block_to >= last_block_from);
            assert(buffer_from <= last_block_to && last_block_to <= buffer_to);
            assert(buffer_from <= last_block_from && last_block_from <= buffer_to);

            return Allocation_Result{Allocator_State::OK, output};
        }

        virtual 
        Allocator_State_Type deallocate(Slice<u8> allocated, isize align) noexcept override 
        {
            u8* ptr = allocated.data;

            if(ptr < buffer_from || buffer_to <= ptr) 
                return parent->deallocate(allocated, align);

            Slice<u8> used = {buffer_from, buffer_to - buffer_from};
            assert(is_in_slice(allocated.data, used) && "invalid free!");
            assert(is_in_slice(allocated.data + allocated.size, used) && "invalid free!");

            Slot *slot = (cast(Slot*) allocated.data) - 1;
            slot->prev_offset = slot->prev_offset & ~USED_BIT;

            current_alloced -= allocated.size;

            while (true) 
            {
                Slot* last_slot = (cast(Slot*) last_block_from) - 1;
                if(last_slot->prev_offset & USED_BIT)
                    return Allocator_State::OK;

                last_block_from = (cast(u8*) last_slot) - last_slot->prev_offset;
                last_block_to = cast(u8*) last_slot;

                if(last_block_from <= buffer_from)
                {
                    last_block_from = buffer_from;
                    last_block_to = buffer_from;
                    return Allocator_State::OK;
                }
            }

            return Allocator_State::OK;
        } 

        nodisc virtual 
        Allocation_Result resize(Slice<u8> allocated, isize align, isize new_size) noexcept override 
        {
            u8* ptr = allocated.data;
            if(ptr < buffer_from || buffer_to <= ptr) 
                return parent->resize(allocated, align, new_size);

            //if is last block resize else fail
            if(last_block_from != allocated.data)
                return Allocation_Result{Allocator_State::NOT_RESIZABLE};

            //if too big fail
            u8* new_end = align_forward(allocated.data + new_size, alignof(Slot));
            if(new_end > buffer_to)
                return Allocation_Result{Allocator_State::NOT_RESIZABLE};

            last_block_to = new_end;
            current_alloced += new_size - allocated.size;
            return Allocation_Result{Allocator_State::OK, Slice<u8>{allocated.data, new_size}};
        }

        nodisc virtual 
        Nullable<Allocator*> parent_allocator() const noexcept override 
        {
            return {nullptr};
        }

        nodisc virtual 
        isize bytes_allocated() const noexcept override 
        {
            return current_alloced;
        }

        nodisc virtual 
        isize bytes_used() const noexcept override 
        {
            return buffer_to - buffer_from;
        }

        nodisc virtual 
        isize max_bytes_allocated() const noexcept override 
        {
            return max_alloced;
        }

        nodisc virtual 
        isize max_bytes_used() const noexcept override 
        {
            return bytes_used();
        }

        virtual
        ~Stack_Allocator() noexcept override {}
    };
}

#include "undefs.h"