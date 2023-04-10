#pragma once

#pragma once

#include "memory.h"
#include "defines.h"
  
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

        uint8_t* stack_to = nullptr;
        uint8_t* last_block_from = nullptr;
        uint8_t* remainder_from = nullptr;

        int32_t max_alloced = 0;
        int32_t current_alloced = 0;

        Allocator* parent = nullptr;

        struct Slot
        {
            uint32_t prev_offset;
            uint32_t size;
        };

        static constexpr uint32_t STUB_BIT = cast(uint32_t) 1 << 31;
        static constexpr uint32_t USED_BIT = cast(uint32_t) 1 << 31;
        static constexpr isize SIZE_MULT = sizeof(isize) == 4 ? 1 : sizeof(Slot);
        static constexpr isize MAX_NOT_MULT_SIZE = (cast(uint32_t) -1) & ~USED_BIT;
        static constexpr isize MAX_BYTE_SIZE = MAX_NOT_MULT_SIZE * SIZE_MULT;


        Stack_Ring_Allocator(Slice<uint8_t> buffer, Allocator* parent) 
            : parent(parent) 
        {
            Slice<uint8_t> aligned = align_forward(buffer, alignof(Slot));

            buffer_from = aligned.data;
            buffer_to = aligned.data + aligned.size;

            stack_to = buffer_from;
            remainder_from = buffer_to;
            last_block_from = buffer_from;
        }
        
        nodisc virtual 
        Allocation_State allocate(Slice<uint8_t>* output, isize size, isize align) noexcept override 
        {
            return try_allocate(output, size, align);
        }
        
        nodisc
        Allocation_State handle_wrap_around_and_allocate(Slice<uint8_t>* output, isize size, isize align, bool is_second_try) noexcept
        {
            //if even on second try we failed we stop
            // to prevent recursive infinite loop
            //if its never possible to even store the desired size we simply stop
            if(is_second_try || size > bytes_used() || size > MAX_NOT_MULT_SIZE)
                return parent->allocate(output, size, align);

            //if remainder is not 0 free as much from the remainder 
            if(remainder_from != buffer_to)
            {
                remainder_from = deallocate_from_front(remainder_from, buffer_to);
            }
            //else free from start of the buffer
            else
            {
                uint8_t* free_to = deallocate_from_front(buffer_from, stack_to);
                isize curr_availible_size = ptrdiff(buffer_to, stack_to);
                isize new_availible_size = ptrdiff(free_to, buffer_from);
                    
                //if the new size is smaller then the old size stop
                if(new_availible_size <= curr_availible_size)
                    return parent->allocate(output, size, align);
                    
                //Fill the rest with stub
                Slot* fill_rest = cast(Slot*) stack_to;
                isize fill_size = ptrdiff(buffer_to, fill_rest + 1);
                fill_rest->size = cast(uint32_t) (fill_size / SIZE_MULT);
                fill_rest->prev_offset = STUB_BIT;

                //reset the pointers
                stack_to = buffer_from;
                remainder_from = free_to;
                last_block_from = buffer_from;
            }

            //try to allocate again this time with the is_second_try flag on
            return try_allocate(output, size, align, true);
        }

        nodisc
        Allocation_State try_allocate(Slice<uint8_t>* output, isize size, isize align, bool is_second_try = false) noexcept 
        {
            assert(is_invariant());
            assert(size >= 0 && align > 0);

            if(align <= sizeof(Slot))
                align = sizeof(Slot);

            //get the first adress at which it would be possible to place the slot header
            uint8_t* available_from = stack_to + sizeof(Slot);

            //align the adresses forward 
            // (aligned_to needs to be aligned for the next allocation
            //  and we can use the aligned size to safely divide by SIZE_MULT;
            //  We can then support 8x bigger allocations using the same header
            //  (from 2 GB to 16 GB))
            // This can be switched of by setting SIZE_MULT to 1 but the performance gain
            //  is statistically insignificant
            uint8_t* aligned_from = align_forward(available_from, align);
            uint8_t* aligned_to = align_forward(aligned_from + size, sizeof(Slot));
            
            if(aligned_to > remainder_from || size > MAX_BYTE_SIZE)
                return handle_wrap_around_and_allocate(output, size, align, is_second_try);

            //Get the adresses at which to place the headers
            // The headers might be 2 or 1 depending on if they share the same 
            // address or not.
            // The main header is called slot and stores the size, offset to 
            //   the previous and USED_BIT
            // The stub header is used to track padding added by alignment in case of overalignment
            //   It stores only the size of the alignment and STUB_BIT indicating its stub
            // When the alignment is <= sizeof(Slot) the adress of stub == address of slot
            //  so the stub gets completely overwritten
            Slot* stub = cast(Slot*) stack_to;
            Slot* slot = (cast(Slot*) aligned_from) - 1;

            isize slot_size = ptrdiff(aligned_to, aligned_from);
            isize stub_size = ptrdiff(slot, stub) - cast(isize) sizeof(Slot); //only the size of data in between headers => - sizeof(Slot)
            isize slot_offset = ptrdiff(slot, last_block_from);

            uint32_t reduced_slot_size = cast(uint32_t) (slot_size / SIZE_MULT);
            uint32_t reduced_stub_size = cast(uint32_t) (stub_size / SIZE_MULT);
            uint32_t reduced_slot_offset = cast(uint32_t) (slot_offset / SIZE_MULT);

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
            *output = Slice<uint8_t>{aligned_from, size};
            stack_to = aligned_to;
            last_block_from = aligned_from;

            current_alloced += reduced_slot_size;
            max_alloced = max(max_alloced, current_alloced);

            assert(check_allocated(*output, align));
            return Allocation_State::OK;
        }
        
        uint8_t* deallocate_from_front(uint8_t* from, uint8_t* to) noexcept
        {
            //nothing to do
            if(from == to)
                return from;

            Slot* current_used_from = cast(Slot*) from;
            while (true) 
            {
                Slot* first_slot = current_used_from;
                if(first_slot->size & USED_BIT)
                    break;

                uint8_t* next_used_from = (cast(uint8_t*) first_slot) + first_slot->size * SIZE_MULT + sizeof(Slot);
                current_used_from = cast(Slot*) next_used_from;

                //if is past the end or at end collapse all to 0 and end
                if(next_used_from >= to)
                {
                    current_used_from = cast(Slot*) to;
                    break;
                }
            }

            return cast(uint8_t*) current_used_from;
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
                Slot* last_slot = (cast(Slot*) last_block_from) - 1;

                assert((last_slot->prev_offset & STUB_BIT) == false && "must not be stub");

                if(last_slot->size & USED_BIT)
                    return;
                    
                isize buffer_size = bytes_used();
                assert((cast(isize) last_slot->prev_offset < buffer_size));
                assert((cast(isize) last_slot->size < buffer_size));

                last_block_from = (cast(uint8_t*) last_slot) - last_slot->prev_offset * SIZE_MULT;
                stack_to = cast(uint8_t*) last_slot;

                //if is past the end or at end collapse all to 0 and end
                if(last_block_from <= buffer_from)
                {
                    last_block_from = buffer_from;
                    stack_to = buffer_from;
                    return;
                }
            }

            return;
        }

        virtual 
        Allocation_State deallocate(Slice<uint8_t> allocated, isize align) noexcept override 
        {
            assert(is_invariant());

            uint8_t* ptr = allocated.data;

            if(ptr < buffer_from || buffer_to <= ptr)
                return parent->deallocate(allocated, align);

            assert(check_allocated(allocated, align) 
                && "the allocation must be valid!"
                && "(in used portion, was not yet freed, is_aligned, sizes match)");

            //Mark slot as free
            Slot *slot = (cast(Slot*) allocated.data) - 1;
            slot->size = slot->size & ~USED_BIT;

            current_alloced -= slot->size;
            
            deallocate_from_back();
            return Allocation_State::OK;
        } 
        
        //@TODO move out!

        nodisc virtual 
        Allocation_State resize(Slice<uint8_t>* output, Slice<uint8_t> allocated, isize new_size, isize align) noexcept override 
        {
            assert(is_invariant());

            uint8_t* ptr = allocated.data;
            if(ptr < buffer_from || buffer_to <= ptr)
                return parent->resize(output, allocated, align, new_size);

            assert(check_allocated(allocated, align) 
                && "the allocation must be valid!"
                && "(in used portion, was not yet freed, is_aligned, sizes match)");

            Slot *slot = (cast(Slot*) allocated.data) - 1;
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
                Slot* next_slot = cast(Slot*) (cast(uint8_t*) current_slot +current_size * SIZE_MULT) + 1;

                bool is_used = next_slot->size & USED_BIT;
                bool is_stub = next_slot->prev_offset & STUB_BIT;

                //if next is past end simply allocate
                if(cast(uint8_t*) next_slot >= stack_to)
                {
                    uint8_t* new_end_ptr = allocated.data + new_size;
                    if(new_end_ptr > buffer_to)
                        return Allocation_State::OUT_OF_MEMORY;

                    uint8_t* aligned_end = align_forward(new_end_ptr, sizeof(Slot));
                    new_reduced_size = ptrdiff(aligned_end, allocated.data) / SIZE_MULT;
                    stack_to = aligned_end;
                    break;
                }

                //if have enough size & is not a stub (& is not at end)
                if(ptrdiff(next_slot, allocated.data) >= new_size && is_stub == false)
                {
                    uint8_t* aligned_end = cast(uint8_t*) next_slot;
                    new_reduced_size = ptrdiff(aligned_end, allocated.data) / SIZE_MULT;
                    next_slot->prev_offset = cast(uint32_t) new_reduced_size;
                    break;
                }

                if(is_used)
                    return Allocation_State::NOT_RESIZABLE;


                current_slot = next_slot;
            }
            
            
            int32_t old_redced_size = cast(int32_t) (slot->size & ~USED_BIT);

            slot->size = cast(uint32_t) new_reduced_size | USED_BIT;;
            current_alloced += cast(int32_t) new_reduced_size - old_redced_size;

            *output = Slice<uint8_t>{allocated.data, new_size};
            return Allocation_State::OK;
        }

        //Checks if stack is in correct state (used in debug only)
        bool is_invariant() const noexcept
        {
            bool is_last_block_aligned = align_forward(stack_to, sizeof(Slot)) == stack_to; 
            bool last_pointers_make_range = stack_to >= last_block_from;
            bool buffer_pointers_make_range = buffer_to >= buffer_from;
            bool last_pointers_within_buffer = buffer_from <= last_block_from && stack_to <= buffer_to;

            return is_last_block_aligned && last_pointers_make_range && buffer_pointers_make_range && last_pointers_within_buffer;
        }

        //Checks if the allocation is valid (used in debug only)
        bool check_allocated(Slice<uint8_t> allocated, isize align) const noexcept
        {
            Slice<uint8_t> used = {buffer_from, buffer_to - buffer_from};
            Slot *slot = (cast(Slot*) allocated.data) - 1;

            bool is_front_in_slice = is_in_slice(allocated.data, used); 
            bool is_back_in_slice  = is_in_slice(allocated.data + allocated.size, used); 
            bool is_aligned = align_forward(allocated.data, align) == allocated.data; 
            bool is_used = slot->size & USED_BIT;

            uint8_t* aligned = align_forward(allocated.data + allocated.size, sizeof(Slot));
            isize aligned_size = aligned - allocated.data;
            isize slot_size = (slot->size & ~USED_BIT);
            bool sizes_match = slot_size * SIZE_MULT >= aligned_size;
            assert(sizes_match);

            return is_front_in_slice && is_used && is_back_in_slice && is_aligned && sizes_match;
        }

        nodisc virtual 
        isize bytes_allocated() const noexcept override 
        {
            return cast(isize) current_alloced * SIZE_MULT;
        }

        nodisc virtual 
        isize bytes_used() const noexcept override 
        {
            return buffer_to - buffer_from;
        }

        nodisc virtual 
        isize max_bytes_allocated() const noexcept override 
        {
            return cast(isize) max_alloced * SIZE_MULT;
        }

        nodisc virtual 
        isize max_bytes_used() const noexcept override 
        {
            return bytes_used();
        }

        virtual
        ~Stack_Ring_Allocator() noexcept override {}
    };
}

#include "undefs.h"