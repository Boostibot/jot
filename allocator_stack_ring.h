#pragma once

#include "memory.h"
  
namespace jot
{
    //Allocates lineary from fixed buffer placing 8 byte headers in front of each allocation.
    //
    //Deallocates from the back of the buffer (stack) keeping data hot. When runs out of memory
    // deallocates from the start and wraps around then behaves like a stack again. 
    // this is important bcause of "snaking": when we allocate in patern of a b a b a b... such that 
    // before each new alloaction of a or b we deallocate the previous data the stack pointer keeps advancing
    // forward even though we are holding on to just two allocations at any given time leaving giant bubble in the back.
    // Wrapping around "pops" the empty space bubble an enabling it to be reused.
    //
    //Headers allow this allocator to traverse the allocations in both directions which is used for deallocation
    // from the font and to potentially resize blocks that arent necessarily at the end of the stack.
    // 
    //This allocator is a good choice for almost-general-purpose scratch allocator. Its is only about
    // 50% slower than arena allocator without touching the data. The fact that this reuses memory  
    // keeps the memory hot which makes it with touching data alot faster than 
    // arena would be particualrly for short term stack order allocations/frees.
    struct Stack_Ring_Allocator : Allocator
    {
        uint8_t* buffer_from = nullptr;
        uint8_t* buffer_to = nullptr;

        uint8_t* last_block_from = nullptr;
        uint8_t* last_block_to = nullptr;
        uint8_t* remainder_from = nullptr;

        int32_t max_alloced = 0;
        int32_t current_alloced = 0;

        Allocator* parent = nullptr;

        struct Slot
        {
            uint32_t prev_offset;
            uint32_t size;
        };

        static constexpr uint32_t STUB_BIT = (uint32_t) 1 << 31;
        static constexpr uint32_t USED_BIT = (uint32_t) 1 << 31;
        static constexpr isize SIZE_MULT = sizeof(isize) == 4 ? 1 : sizeof(Slot);
        static constexpr isize MAX_NOT_MULT_SIZE = ((uint32_t) -1) & ~USED_BIT;
        static constexpr isize MAX_BYTE_SIZE = MAX_NOT_MULT_SIZE * SIZE_MULT;

        Stack_Ring_Allocator(void* buffer, isize buffer_size, Allocator* parent = default_allocator()) 
            : parent(parent) 
        {
            //Slice<uint8_t> aligned = align_forward(buffer, alignof(Slot));

            buffer_from = (uint8_t*) buffer;
            buffer_to = buffer_from + buffer_size ;

            last_block_to = buffer_from;
            remainder_from = buffer_to;
            last_block_from = buffer_from;
        }
        
        
        virtual
        void* allocate(isize size, isize align, Line_Info callee) noexcept override 
        {
            return try_allocate(size, align, callee);
        }

        void* try_allocate(isize size, isize align, Line_Info callee, bool is_second_try = false) noexcept 
        {
            assert(is_invariant());
            assert(size >= 0 && align > 0);

            if(align <= sizeof(Slot))
                align = sizeof(Slot);

            //get the first adress at which it would be possible to place the slot header
            uint8_t* available_from = last_block_to + sizeof(Slot);

            //align the adresses forward 
            // (aligned_to needs to be aligned for the next allocation
            //  and we can use the aligned size to safely divide by SIZE_MULT;
            //  We can then support 8x bigger allocations using the same header
            //  (from 2 GB to 16 GB))
            // This can be switched of by setting SIZE_MULT to 1 but the performance gain
            //  is statistically insignificant
            uint8_t* aligned_from = (uint8_t*) align_forward(available_from, align);
            uint8_t* aligned_to = (uint8_t*) align_forward(aligned_from + size, sizeof(Slot));
            
            if(aligned_to > remainder_from || size > MAX_BYTE_SIZE)
                return handle_wrap_around_and_allocate(size, align, callee, is_second_try);

            //Get the adresses at which to place the headers
            // The headers might be 2 or 1 depending on if they share the same 
            // address or not.
            // The main header is called slot and stores the size, offset to 
            //   the previous and USED_BIT
            // The stub header is used to track padding added by alignment in case of overalignment
            //   It stores only the size of the alignment and STUB_BIT indicating its stub
            // When the alignment is <= sizeof(Slot) the adress of stub == address of slot
            //  so the stub gets completely overwritten
            Slot* stub = (Slot*) last_block_to;
            Slot* slot = ((Slot*) aligned_from) - 1;

            isize slot_size = ptrdiff(aligned_to, aligned_from);
            isize stub_size = ptrdiff(slot, stub) - (isize) sizeof(Slot); //only the size of data in between headers => - sizeof(Slot)
            isize slot_offset = ptrdiff(slot, last_block_from);

            uint32_t reduced_slot_size = (uint32_t) (slot_size / SIZE_MULT);
            uint32_t reduced_stub_size = (uint32_t) (stub_size / SIZE_MULT);
            uint32_t reduced_slot_offset = (uint32_t) (slot_offset / SIZE_MULT);

            assert(slot_size >= 0 && "slot size should never be negative");

            assert((slot_offset >= 0 ? true : stub == slot) && "slot offset is only negtaive when stub == slot (will be overriden)");
            assert((stub_size >= 0 ? true : stub == slot) && "stub size is only negtaive when stub == slot (will be overriden)");

            //becasue stub and slot might be in the same place
            // we first assign stub and then slot so that stub will
            // get overwritten in case they alias
            stub->size = reduced_stub_size;
            stub->prev_offset = STUB_BIT;
            
            slot->size = reduced_slot_size | USED_BIT; 
            slot->prev_offset = reduced_slot_offset; 

            //Recalculate meta data
            last_block_to = aligned_to;
            last_block_from = aligned_from;

            current_alloced += reduced_slot_size;
            if(max_alloced < current_alloced)
                max_alloced = current_alloced;

            assert(check_allocated(aligned_from, size, align) 
                && "the allocation must be valid!"
                && "(in used portion, was not yet freed, is_aligned, sizes match)");

            return aligned_from;
        }
        
        void* handle_wrap_around_and_allocate(isize size, isize align, Line_Info callee, bool is_second_try) noexcept
        {
            //if even on second try we failed we stop
            // to prevent recursive infinite loop
            //if its never possible to even store the desired size we simply stop
            if(is_second_try || size > bytes_used() || size > MAX_NOT_MULT_SIZE)
                return parent->allocate(size, align, callee);

            //if remainder is not 0 free as much from the remainder 
            if(remainder_from != buffer_to)
            {
                remainder_from = deallocate_from_front(remainder_from, buffer_to);
            }
            //else free from start of the buffer
            else
            {
                uint8_t* free_to = deallocate_from_front(buffer_from, last_block_to);
                isize curr_availible_size = ptrdiff(buffer_to, last_block_to);
                isize new_availible_size = ptrdiff(free_to, buffer_from);
                    
                //if the new size is smaller then the old size stop
                if(new_availible_size <= curr_availible_size)
                    return parent->allocate(size, align, callee);
                    
                //Fill the rest with stub
                Slot* fill_rest = (Slot*) last_block_to;
                isize fill_size = ptrdiff(buffer_to, fill_rest + 1);
                fill_rest->size = (uint32_t) (fill_size / SIZE_MULT);
                fill_rest->prev_offset = STUB_BIT;

                //reset the pointers
                last_block_to = buffer_from;
                remainder_from = free_to;
                last_block_from = buffer_from;
            }

            //try to allocate again this time with the is_second_try flag on
            return try_allocate(size, align, callee, true);
        }

        uint8_t* deallocate_from_front(uint8_t* from, uint8_t* to) noexcept
        {
            //nothing to do
            if(from == to)
                return from;

            Slot* current_used_from = (Slot*) from;
            while (true) 
            {
                Slot* first_slot = current_used_from;
                if(first_slot->size & USED_BIT)
                    break;

                uint8_t* next_used_from = ((uint8_t*) first_slot) + first_slot->size * SIZE_MULT + sizeof(Slot);
                current_used_from = (Slot*) next_used_from;

                //if is past the end or at end collapse all to 0 and end
                if(next_used_from >= to)
                {
                    current_used_from = (Slot*) to;
                    break;
                }
            }

            return (uint8_t*) current_used_from;
        }

        isize bytes_used() const
        {
            return buffer_to - buffer_from;
        }

        void deallocate_from_back() noexcept
        {
            //Try to free slots starting at the front and itarting backwards
            // If we hit used slot or we leave the buffer stop
            // Most of the time stops in the first iteration (dealloced != last_block_from)
            if(last_block_from == buffer_from)
                return;

            while (true) 
            {
                Slot* last_slot = ((Slot*) last_block_from) - 1;

                assert((last_slot->prev_offset & STUB_BIT) == false && "must not be stub");

                if(last_slot->size & USED_BIT)
                    return;
                    
                isize buffer_size = bytes_used();
                assert(((isize) last_slot->prev_offset < buffer_size));
                assert(((isize) last_slot->size < buffer_size));

                last_block_from = ((uint8_t*) last_slot) - last_slot->prev_offset * SIZE_MULT;
                last_block_to = (uint8_t*) last_slot;

                //if is past the end or at end collapse all to 0 and end
                if(last_block_from <= buffer_from)
                {
                    last_block_from = buffer_from;
                    last_block_to = buffer_from;
                    return;
                }
            }

            return;
        }
        
        virtual 
        bool deallocate(void* allocated, isize old_size, isize align, Line_Info callee) noexcept override
        {
            assert(is_invariant());

            uint8_t* ptr = (uint8_t*) allocated;

            if(ptr < buffer_from || buffer_to <= ptr)
                return parent->deallocate(allocated, old_size, align, callee);

            assert(check_allocated(allocated, old_size, align) 
                && "the allocation must be valid!"
                && "(in used portion, was not yet freed, is_aligned, sizes match)");

            //Mark slot as free
            Slot *slot = ((Slot*) allocated) - 1;
            slot->size = slot->size & ~USED_BIT;

            current_alloced -= slot->size;
            
            deallocate_from_back();
            return true;
        } 
        
        virtual
        bool resize(void* allocated, isize old_size, isize new_size, isize align, Line_Info callee) noexcept override 
        {
            assert(is_invariant());

            uint8_t* ptr = (uint8_t*) allocated;
            if(ptr < buffer_from || buffer_to <= ptr)
                return parent->resize(allocated, old_size, new_size, align, callee);
                
            assert(check_allocated(allocated, old_size, align) 
                && "the allocation must be valid!"
                && "(in used portion, was not yet freed, is_aligned, sizes match)");

            Slot *slot = ((Slot*) allocated) - 1;
            Slot* current_slot = slot;
            isize new_reduced_size = 0; 

            //we iterate forward empty slots from the current positions 
            // once we hit a slot that is free and the cumulative size is big enough
            // we resize and break
            //we also require that the final resized up to slot is not stub
            // this is so that deallocation can skip stubs completely which is a significant
            // performance optimization - when alignments differ cuts the iterations to free the stack
            // by half
            while (true) 
            {
                //Get the current size and use it to calculate where the next slot is located
                uint32_t current_size = current_slot->size & ~USED_BIT; 
                Slot* next_slot = (Slot*) ((uint8_t*) current_slot +current_size * SIZE_MULT) + 1;

                bool is_used = next_slot->size & USED_BIT;
                bool is_stub = next_slot->prev_offset & STUB_BIT;

                //if next is past end simply allocate
                if((uint8_t*) next_slot >= last_block_to)
                {
                    uint8_t* new_end_ptr = ptr + new_size;
                    if(new_end_ptr > buffer_to)
                        return false;

                    uint8_t* aligned_end = (uint8_t*) align_forward(new_end_ptr, sizeof(Slot));
                    new_reduced_size = ptrdiff(aligned_end, ptr) / SIZE_MULT;
                    last_block_to = aligned_end;
                    break;
                }

                //if have enough size & is not a stub (& is not at end)
                if(ptrdiff(next_slot, ptr) >= new_size && is_stub == false)
                {
                    uint8_t* aligned_end = (uint8_t*) next_slot;
                    new_reduced_size = ptrdiff(aligned_end, ptr) / SIZE_MULT;
                    next_slot->prev_offset = (uint32_t) new_reduced_size;
                    break;
                }

                if(is_used)
                    return false;


                current_slot = next_slot;
            }
            
            
            int32_t old_redced_size = (int32_t) (slot->size & ~USED_BIT);

            slot->size = (uint32_t) new_reduced_size | USED_BIT;;
            current_alloced += (int32_t) new_reduced_size - old_redced_size;

            assert(check_allocated(allocated, old_size, align) 
                && "the allocation must be valid!"
                && "(in used portion, was not yet freed, is_aligned, sizes match)");
            return true;
        }

        //Checks if stack is in correct state (used in debug only)
        bool is_invariant() const noexcept
        {
            bool is_last_block_aligned = align_forward(last_block_to, sizeof(Slot)) == last_block_to; 
            bool last_pointers_make_range = last_block_to >= last_block_from;
            bool buffer_pointers_make_range = buffer_to >= buffer_from;
            bool last_pointers_within_buffer = buffer_from <= last_block_from && last_block_to <= buffer_to;

            return is_last_block_aligned && last_pointers_make_range && buffer_pointers_make_range && last_pointers_within_buffer;
        }

        //Checks if the allocation is valid (used in debug only)
        bool check_allocated(void* allocated, isize old_size, isize align) const noexcept
        {
            uint8_t* ptr = (uint8_t*) allocated;
            Slot *slot = ((Slot*) allocated) - 1;

            bool is_in_buffer = buffer_from <= ptr && ptr + old_size <= buffer_to;
            bool is_aligned = align_forward(allocated, align) == allocated; 
            bool is_used = slot->size & USED_BIT;

            uint8_t* aligned = (uint8_t*) align_forward(ptr + old_size, sizeof(Slot));
            isize aligned_size = aligned - ptr;
            isize slot_size = (slot->size & ~USED_BIT);
            bool sizes_match = slot_size * SIZE_MULT >= aligned_size;
            assert(sizes_match);

            return is_in_buffer && is_used && is_aligned && sizes_match;
        }
        
        virtual
        Stats get_stats() const noexcept override
        {
            Stats stats = {};
            stats.name = "Stack_Ring_Allocator";
            stats.supports_resize = true;
            stats.parent = parent;
            stats.bytes_allocated = (isize) current_alloced * SIZE_MULT;
            stats.bytes_used = buffer_to - buffer_from;

            stats.max_bytes_allocated = (isize) max_alloced * SIZE_MULT;
            stats.max_bytes_used = stats.bytes_used;
            
            return stats;
        }

        virtual
        ~Stack_Ring_Allocator() noexcept override {}
    };
}
