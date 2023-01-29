#pragma once

#include "memory.h"
#include "defines.h"

namespace jot
{
    namespace detail
    {
        template<typename T> nodisc static
        T* offset_ptr(T* ptr1, isize by_bytes)
        {
            u8* address = cast(u8*) cast(void*) ptr1;
            return cast(T*) cast(void*) (address + by_bytes);
        }

        nodisc static
        u8* u8_ptr(void* ptr)
        {
            return cast(u8*) ptr;
        }

        struct Slot 
        {
            u32 size = 0; 
        };

        static_assert(sizeof(Slot) == sizeof(u32) && alignof(Slot) == alignof(u32), "slot must be transparent");

        static constexpr u32 SLOT_PAD_VALUE = 0xffff'ffff;
        static constexpr u32 STUB_BIT = cast(u32) 1 << 31;
        static constexpr u32 USED_BIT = cast(u32) 1 << 31;
        static constexpr isize SIZE_MULT = sizeof(isize) == 4 ? 1 : sizeof(Slot);
        static constexpr isize MAX_NOT_MULT_SIZE = (cast(u32) -1) & ~USED_BIT;
        static constexpr isize MAX_BYTE_SIZE = MAX_NOT_MULT_SIZE * SIZE_MULT;

        nodisc static
        isize size(Slot* slot) 
        {
            return (slot->size & ~USED_BIT) * SIZE_MULT;
        }

        nodisc static
        Slot* slot(void* ptr) 
        {
            u32* padding_ptr = cast(u32*) offset_ptr(ptr, -cast(isize) sizeof(Slot));
            while(*padding_ptr == SLOT_PAD_VALUE)
                padding_ptr -= 1;

            return cast(Slot*) cast(void*) padding_ptr;
        }

        nodisc static
        Slice<u8> data(Slot* slot, isize align) 
        {
            isize slot_size = size(slot); 
            u8* data = align_forward(u8_ptr(slot) + sizeof(Slot), align);

            return Slice<u8>{data, slot_size};
        }

        static 
        void place_slot(Slot* at, u32 size, void* data_start, bool used)
        {
            at->size = size / cast(u32) SIZE_MULT;
            if(used)
                at->size |= USED_BIT;

            u32* padding_ptr = (cast(u32*) cast(void*) at) + 1;
            while(padding_ptr < data_start)
            {
                *padding_ptr = SLOT_PAD_VALUE;
                padding_ptr += 1;
            }
        }
    }

    //Allocate lienarry from a buffer. Once the end of buffer is reached wrap around to the 
    // start an override memory (if freed). Can deallocate any allocation. 
    //Inserts 32 bits before each allocation
    struct Ring_Allocator : Allocator
    {
        u8* used_from = nullptr;
        u8* used_to = nullptr;

        u8* buffer_from = nullptr; 
        u8* buffer_to = nullptr;

        isize max_alloced = 0;
        isize current_alloced = 0;

        Allocator* parent = nullptr;

        Ring_Allocator(Slice<u8> buffer, Allocator* parent) 
            : parent(parent) 
        {
            using namespace detail;
            Slice<u8> aligned = align_forward(buffer, alignof(Slot));

            buffer_from = aligned.data;
            buffer_to = aligned.data + aligned.size;

            used_from = buffer_from;
            used_to = buffer_from;

            assert(align_forward(buffer_from, alignof(Slot)) == buffer_from && "slot must be aligned"); 
            assert(align_forward(used_to, alignof(Slot)) == used_to && "slot must be aligned"); 
        }

        bool in_use(void *p) const
        {
            if (used_to > used_from)
                return p >= used_from && p < used_to;

            if (used_from == used_to) 
                return false;

            return p >= used_from || p < used_to;
        }

        nodisc virtual 
        Allocation_Result allocate(isize size, isize align) noexcept override 
        {
            using namespace detail;
            assert_arg(size >= 0 && align > 0);

            //get the adress at which to place Slot header
            assert(align_forward(used_to, alignof(Slot)) == used_to && "slot must be aligned"); 

            u8* slot_from = used_to;
            u8* available_from = slot_from + sizeof(Slot);

            u8* aligned_from = align_forward(available_from, align);
            u8* aligned_to = align_forward(aligned_from + size, alignof(Slot));

            //if past end wrap
            if(aligned_to > buffer_to) 
            {
                Slot* fill_rest = cast(Slot*) slot_from;
                fill_rest->size = cast(u32) ptrdiff(buffer_to, fill_rest) & ~USED_BIT;

                //recalc the ptrs
                slot_from = buffer_from;
                available_from = slot_from + sizeof(Slot);
                aligned_from = align_forward(available_from, align);
                aligned_to = align_forward(aligned_from + size, alignof(Slot));
            }   

            isize aligned_size = ptrdiff(aligned_to, available_from);
            u8 is_free = in_use(aligned_to);
            u8 is_allowed = aligned_size > MAX_BYTE_SIZE;
            u8 did_overflow_twice = aligned_to > buffer_to;
            if(is_allowed | did_overflow_twice | is_free) 
                return parent->allocate(size, align);

            place_slot(cast(Slot*) slot_from, cast(u32) aligned_size, aligned_from, true);

            used_to = aligned_to;

            current_alloced += size;
            max_alloced = max(max_alloced, current_alloced);
            return Allocation_Result{Allocator_State::OK, Slice<u8>{aligned_from, size}};
        }

        virtual 
        Allocator_State_Type deallocate(Slice<u8> allocated, isize align) noexcept override 
        {
            using namespace detail;
            u8* ptr = allocated.data;

            if(ptr < buffer_from || buffer_to <= ptr) 
                return parent->deallocate(allocated, align);

            // Mark this slot as free
            Slot *s = slot(ptr);
            s->size = s->size & ~USED_BIT;

            current_alloced -= allocated.size;

            // Advance the free pointer past all free slots.
            while (true) {
                Slot* first_slot = cast(Slot*) used_from;
                if(first_slot->size & USED_BIT)
                    return Allocator_State::OK;

                //since USED_BIT is 0 we can directly use the size
                Slot* next_slot = offset_ptr(first_slot + 1, first_slot->size * SIZE_MULT);

                //assert(first_slot->size <= jot::memory_constants::KIBI_BYTE / SIZE_MULT * 2);
                used_from = u8_ptr(next_slot);
                if(used_from == used_to)
                    break;

                if (used_from >= buffer_to)
                {
                    used_from = buffer_from;
                    break;
                }
            }

            return Allocator_State::OK;
        } 

        nodisc virtual 
        Allocation_Result resize(Slice<u8> allocated, isize align, isize new_size) noexcept override 
        {
            using namespace detail;
            u8* ptr = allocated.data;
            if(ptr < buffer_from || ptr <= buffer_to) 
                return parent->resize(allocated, align, new_size);

            u8* first_stub_u8 = align_forward(allocated.data + allocated.size, alignof(Slot));
            Slot* first_stub = cast(Slot*) first_stub_u8;
            Slot* current_slot = first_stub;

            //because we need to perserve the internal structure of the memory
            // we need to reize so that we exactly end up on another block
            isize resized_to_size = 0;

            while(true)
            {
                isize current_dist = ptrdiff(current_slot, ptr);
                if(current_dist >= new_size)
                {
                    resized_to_size = current_dist;
                    break;
                }

                if(in_use(current_slot) == false)
                {
                    u8* current_slot_u8 = cast(u8*) current_slot;

                    isize available = 0;
                    if(current_slot_u8 > used_from)
                        available = buffer_to - current_slot_u8;
                    else
                        available = used_from - current_slot_u8;

                    if(available >= new_size)
                    {
                        resized_to_size = new_size;
                        used_to = align_forward(current_slot_u8, alignof(Slot));
                        break;
                    }
                    else
                        return Allocation_Result{Allocator_State::NOT_RESIZABLE};
                }


                //Here we could check two times but we can actually just merge the two checks into 
                // one. If the USED_BIT is set the size will be huge and we will probably 
                // not pass the wrap test. If we still do we can still check even the USED_BIT.

                //Slot* first_slot = cast(Slot*) used_from;
                //if((first_slot->size & USED_BIT))
                //return Allocation_Result{Allocator_State::NOT_RESIZABLE};

                Slot* next_slot = offset_ptr(current_slot + 1, current_slot->size * SIZE_MULT);

                //if needs to wrap over or isnt free
                if(u8_ptr(next_slot) >= buffer_to || current_slot->size & USED_BIT)
                    return Allocation_Result{Allocator_State::NOT_RESIZABLE};

                current_slot = next_slot;
            }

            Slot* first_slot = slot(ptr);
            first_slot->size = cast(u32) resized_to_size | USED_BIT;

            current_alloced += new_size - allocated.size;
            return Allocation_Result{Allocator_State::OK, Slice<u8>{ptr, new_size}};
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
        ~Ring_Allocator() noexcept override
        {
            isize alloced = bytes_allocated();
            assert(alloced == 0);
        }
    };

    struct Intrusive_Stack_Simple : Allocator
    {
        Slice<u8> buffer;

        u8* buffer_from = nullptr;
        u8* buffer_to = nullptr;
        u8* last_block_to = nullptr;
        u8* last_block_from = nullptr;

        isize max_alloced = 0;
        isize current_alloced = 0;

        Allocator* parent = nullptr;

        struct Slot
        {
            u32 prev_offset;
            u32 size;
        };

        Intrusive_Stack_Simple(Slice<u8> buffer, Allocator* parent) 
            : parent(parent) 
        {
            using namespace detail;
            Slice<u8> aligned = align_forward(buffer, alignof(Slot));
            this->buffer = aligned;

            buffer_from = aligned.data;
            buffer_to = aligned.data + aligned.size;

            last_block_to = buffer_from;
            last_block_from = buffer_from;
        }

        nodisc virtual 
        Allocation_Result allocate(isize size, isize align) noexcept override 
        {
            using namespace detail;
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
            slot->size = cast(u32) size | USED_BIT; 
            slot->prev_offset = cast(u32) ptrdiff(slot, last_block_from); 

            #ifdef DO_ALLOCATOR_STATS
            current_alloced += size;
            max_alloced = max(max_alloced, current_alloced);
            #endif

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
            using namespace detail;
            u8* ptr = allocated.data;

            if(ptr < buffer_from || buffer_to <= ptr) 
                return parent->deallocate(allocated, align);

            Slice<u8> used = {buffer_from, buffer_to - buffer_from};
            assert(is_in_slice(allocated.data, used) && "invalid free!");
            assert(is_in_slice(allocated.data + allocated.size, used) && "invalid free!");

            Slot *slot = (cast(Slot*) allocated.data) - 1;
            slot->size = slot->size & ~USED_BIT;

            #ifdef DO_ALLOCATOR_STATS
            current_alloced -= allocated.size;
            #endif

            while (true) 
            {
                Slot* last_slot = (cast(Slot*) last_block_from) - 1;
                if(last_slot->size & USED_BIT)
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
            using namespace detail;
            u8* ptr = allocated.data;
            if(ptr < buffer_from || buffer_to <= ptr) 
                return parent->resize(allocated, align, new_size);

            return Allocation_Result{Allocator_State::NOT_RESIZABLE};
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
        ~Intrusive_Stack_Simple() noexcept override
        {
            isize alloced = bytes_allocated();
            assert(alloced == 0 && "tracked size must be zero (alloced size == free size)");
            assert(last_block_from == last_block_to && last_block_from == buffer_from && "all pointers must be set to start of the buffer");
        }
    };

    struct Intrusive_Stack_Resize : Allocator
    {
        Slice<u8> buffer;

        u8* buffer_from = nullptr;
        u8* buffer_to = nullptr;
        u8* last_block_to = nullptr;
        u8* last_block_from = nullptr;

        isize max_alloced = 0;
        isize current_alloced = 0;

        Allocator* parent = nullptr;

        struct Slot
        {
            u32 prev_offset;
            u32 size;
        };

        Intrusive_Stack_Resize(Slice<u8> buffer, Allocator* parent) 
            : parent(parent) 
        {
            using namespace detail;
            Slice<u8> aligned = align_forward(buffer, alignof(Slot));
            this->buffer = aligned;

            buffer_from = aligned.data;
            buffer_to = aligned.data + aligned.size;

            last_block_to = buffer_from;
            last_block_from = buffer_from;
        }

        nodisc virtual 
        Allocation_Result allocate(isize size, isize align) noexcept override 
        {
            using namespace detail;
            assert(is_invariant());
            assert_arg(size >= 0 && align > 0);

            if(align <= sizeof(Slot))
                align = sizeof(Slot);

            //get the adress at which to place Slot header
            u8* available_from = last_block_to + sizeof(Slot);

            u8* aligned_from = align_forward(available_from, align);
            u8* aligned_to = align_forward(aligned_from + size, sizeof(Slot));

            Slot* stub = cast(Slot*) last_block_to;
            Slot* slot = (cast(Slot*) aligned_from) - 1;

            isize slot_size = ptrdiff(aligned_to, aligned_from);
            isize stub_size = ptrdiff(slot, stub) - cast(isize) sizeof(Slot);
            isize slot_offset = stub_size;
            isize stub_offset = ptrdiff(stub, last_block_from);

            u32 reduced_slot_size = cast(u32) (slot_size / SIZE_MULT);
            u32 reduced_stub_size = cast(u32) (stub_size / SIZE_MULT);
            u32 reduced_slot_offset = reduced_stub_size;
            u32 reduced_stub_offset = cast(u32) (stub_offset / SIZE_MULT);

            u8 is_too_big  = reduced_slot_size >= USED_BIT;
            u8 is_past_end = aligned_to > buffer_to;
            if(is_too_big | is_past_end) 
                return parent->allocate(size, align);

            assert(slot_size >= 0 && "slot size should never be negative");
            assert(stub_offset >= 0 && "offset should never be negative");

            assert((slot_offset >= 0 ? true : stub == slot) && "slot offset is only negtaive when stub == slot (will be overriden)");
            assert((stub_size >= 0 ? true : stub == slot) && "stub size is only negtaive when stub == slot (will be overriden)");

            //Stub and slot might be in the same place
            // because of this its necessary we assign the fields in this exact order
            stub->size = reduced_stub_size; //stub is always unused
            slot->size = reduced_slot_size | USED_BIT; 

            slot->prev_offset = reduced_slot_offset; 
            stub->prev_offset = reduced_stub_offset;

            Slice<u8> output = {aligned_from, size};
            last_block_to = aligned_to;
            last_block_from = aligned_from;

            current_alloced += size;
            max_alloced = max(max_alloced, current_alloced);

            assert(check_allocated(output, align));
            return Allocation_Result{Allocator_State::OK, output};
        }

        bool is_invariant() noexcept
        {
            bool is_last_block_aligned = align_forward(last_block_to, sizeof(Slot)) == last_block_to; 
            bool last_pointers_make_range = last_block_to >= last_block_from;
            bool buffer_pointers_make_range = buffer_to >= buffer_from;
            bool last_pointers_within_buffer = buffer_from <= last_block_from && last_block_to <= buffer_to;

            return is_last_block_aligned && last_pointers_make_range && buffer_pointers_make_range && last_pointers_within_buffer;
        }

        bool check_allocated(Slice<u8> allocated, isize align) noexcept
        {
            using namespace detail;

            Slice<u8> used = {buffer_from, buffer_to - buffer_from};
            Slot *slot = (cast(Slot*) allocated.data) - 1;

            bool is_front_in_slice = is_in_slice(allocated.data, used);
            
            u8* back = allocated.data + allocated.size;
            bool is_back_in_slice  = is_in_slice(back, used); 
            bool is_aligned = align_forward(allocated.data, align) == allocated.data; 
            bool is_used = slot->size & USED_BIT;

            u8* aligned = align_forward(allocated.data + allocated.size, sizeof(Slot));
            isize aligned_size = aligned - allocated.data;
            isize slot_size = (slot->size & ~USED_BIT);
            bool sizes_match = slot_size * SIZE_MULT >= aligned_size;
            assert(sizes_match);

            bool ret = is_front_in_slice && is_back_in_slice && is_aligned && sizes_match && is_used;
            assert(ret);
            return ret;
        }

        virtual 
        Allocator_State_Type deallocate(Slice<u8> allocated, isize align) noexcept override 
        {
            using namespace detail;
            assert(is_invariant());

            u8* ptr = allocated.data;

            if(ptr < buffer_from || buffer_to <= ptr) 
                return parent->deallocate(allocated, align);

            assert(check_allocated(allocated, align) 
                && "the allocation must be valid!"
                && "(in used portion, was not yet freed, is_aligned, sizes match)");

            Slot *slot = (cast(Slot*) allocated.data) - 1;
            slot->size = slot->size & ~USED_BIT;

            current_alloced -= allocated.size;

            while (true) 
            {
                Slot* last_slot = (cast(Slot*) last_block_from) - 1;
                if(last_slot->size & USED_BIT)
                    return Allocator_State::OK;

                last_block_from = (cast(u8*) last_slot) - last_slot->prev_offset * SIZE_MULT;
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
            using namespace detail;
            assert(is_invariant());

            u8* ptr = allocated.data;
            if(ptr < buffer_from || buffer_to <= ptr) 
                return parent->resize(allocated, align, new_size);

            assert(check_allocated(allocated, align) 
                && "the allocation must be valid!"
                && "(in used portion, was not yet freed, is_aligned, sizes match)");

            Slot *slot = (cast(Slot*) allocated.data) - 1;
            Slot* current_slot = slot;
            while (true) 
            {
                u32 current_size = current_slot->size & ~USED_BIT; 
                Slot* next_slot = current_slot + current_size + 1;

                //if next is past end simply allocate
                if(cast(u8*) next_slot >= last_block_to)
                {
                    u8* new_end_ptr = allocated.data + new_size;
                    if(new_end_ptr > buffer_to)
                        return Allocation_Result{Allocator_State::OUT_OF_MEM};

                    u8* aligned_end = align_forward(new_end_ptr, sizeof(Slot));

                    isize new_reduced_size = ptrdiff(aligned_end, allocated.data) / SIZE_MULT;
                    slot->size = cast(u32) new_reduced_size | USED_BIT;
                    last_block_to = aligned_end;
                    break;
                }

                //if have enough size (& is not at end)
                if(ptrdiff(next_slot, slot) >= new_size)
                {
                    u8* aligned_end = cast(u8*) next_slot;
                    isize new_reduced_size = ptrdiff(aligned_end, allocated.data) / SIZE_MULT;
                    slot->size = cast(u32) new_reduced_size | USED_BIT;
                    next_slot->prev_offset = cast(u32) new_reduced_size;
                    break;
                }

                if(next_slot->size & USED_BIT)
                    return Allocation_Result{Allocator_State::NOT_RESIZABLE};


                current_slot = next_slot;
            }

            current_alloced += new_size - allocated.size;
            Slice<u8> output = {allocated.data, new_size};
            return Allocation_Result{Allocator_State::OK, output};
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
        ~Intrusive_Stack_Resize() noexcept override
        {
            isize alloced = bytes_allocated();
            assert(alloced == 0 && "tracked size must be zero (alloced size == free size)");
            assert(last_block_from == last_block_to && last_block_from == buffer_from && "all pointers must be set to start of the buffer");
        }
    };

    struct Intrusive_Stack_Scan : Allocator
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
            u32 prev_offset;
            u32 size;
        };

        //static constexpr isize SIZE_MULT = 1;
        static constexpr u32 STUB_BIT = cast(u32) 1 << 31;
        static constexpr isize SIZE_MULT = sizeof(Slot);

        Intrusive_Stack_Scan(Slice<u8> buffer, Allocator* parent) 
            : parent(parent) 
        {
            using namespace detail;
            Slice<u8> aligned = align_forward(buffer, alignof(Slot));

            buffer_from = aligned.data;
            buffer_to = aligned.data + aligned.size;

            last_block_to = buffer_from;
            last_block_from = buffer_from;
        }

        nodisc virtual 
        Allocation_Result allocate(isize size, isize align) noexcept override 
        {
            using namespace detail;
            assert(is_invariant());
            assert_arg(size >= 0 && align > 0);

            if(align <= sizeof(Slot))
                align = sizeof(Slot);

            //get the first adress at which it would be possible to place the slot header
            u8* available_from = last_block_to + sizeof(Slot);

            //align the adresses forward 
            // (aligned_to needs to be aligned for the next allocation
            //  and we can use the aligned size to safely divide by SIZE_MULT;
            //  We can then support 8x bigger allocations using the same header
            //  (from 2 GB to 16 GB))
            // This can be switched of by setting SIZE_MULT to 1 but the performance gain
            //  is statistically insignificant
            u8* aligned_from = align_forward(available_from, align);
            u8* aligned_to = align_forward(aligned_from + size, sizeof(Slot));

            //Get the adresses at which to place the headers
            // The headers might be 2 or 1 depending on if they share the same 
            // address or not.
            // The main header is called slot and stores the size, offset to 
            //   the previous and USED_BIT
            // The stub header is used to track padding added by alignment in case of overalignment
            //   It stores only the size of the alignment and STUB_BIT indicating its stub
            // When the alignment is <= sizeof(Slot) the adress of stub == address of slot
            //  so the stub gets completely overwritten
            Slot* stub = cast(Slot*) last_block_to;
            Slot* slot = (cast(Slot*) aligned_from) - 1;

            isize slot_size = ptrdiff(aligned_to, aligned_from);
            isize stub_size = ptrdiff(slot, stub) - cast(isize) sizeof(Slot); //only the size of data in between headers => - sizeof(Slot)
            isize slot_offset = ptrdiff(slot, last_block_from);

            u32 reduced_slot_size = cast(u32) (slot_size / SIZE_MULT);
            u32 reduced_stub_size = cast(u32) (stub_size / SIZE_MULT);
            u32 reduced_slot_offset = cast(u32) (slot_offset / SIZE_MULT);

            //If is too big we use parent
            u8 is_too_big  = reduced_slot_size >= USED_BIT;
            u8 is_past_end = aligned_to > buffer_to;
            if(is_too_big | is_past_end) 
                return parent->allocate(size, align);

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
            Slice<u8> output = {aligned_from, size};
            last_block_to = aligned_to;
            last_block_from = aligned_from;

            current_alloced += size;
            max_alloced = max(max_alloced, current_alloced);

            assert(check_allocated(output, align));
            return Allocation_Result{Allocator_State::OK, output};
        }


        virtual 
        Allocator_State_Type deallocate(Slice<u8> allocated, isize align) noexcept override 
        {
            using namespace detail;
            assert(is_invariant());

            u8* ptr = allocated.data;

            if(ptr < buffer_from || buffer_to <= ptr) 
                return parent->deallocate(allocated, align);

            assert(check_allocated(allocated, align) 
                && "the allocation must be valid!"
                && "(in used portion, was not yet freed, is_aligned, sizes match)");

            //Mark slot as free
            Slot *slot = (cast(Slot*) allocated.data) - 1;
            slot->size = slot->size & ~USED_BIT;

            current_alloced -= allocated.size;

            //Try to free slots starting at the front and itarting backwards
            // If we hit used slot or we leave the buffer stop
            // Most of the time stops in the first iteration (dealloced != last_block_from)
            while (true) 
            {
                Slot* last_slot = (cast(Slot*) last_block_from) - 1;
                assert((last_slot->prev_offset & STUB_BIT) == false && "must not be stub");

                if(last_slot->size & USED_BIT)
                    return Allocator_State::OK;

                last_block_from = (cast(u8*) last_slot) - last_slot->prev_offset * SIZE_MULT;
                last_block_to = cast(u8*) last_slot;

                //if is past the end or at end collapse all to 0 and end
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
            using namespace detail;
            assert(is_invariant());

            u8* ptr = allocated.data;
            if(ptr < buffer_from || buffer_to <= ptr) 
                return parent->resize(allocated, align, new_size);

            assert(check_allocated(allocated, align) 
                && "the allocation must be valid!"
                && "(in used portion, was not yet freed, is_aligned, sizes match)");

            Slot *slot = (cast(Slot*) allocated.data) - 1;
            Slot* current_slot = slot;

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
                u32 current_size = current_slot->size & ~USED_BIT; 
                Slot* next_slot = offset_ptr(current_slot, current_size * SIZE_MULT) + 1;

                bool is_used = next_slot->size & USED_BIT;
                bool is_stub = next_slot->prev_offset & STUB_BIT;

                //if next is past end simply allocate
                if(cast(u8*) next_slot >= last_block_to)
                {
                    u8* new_end_ptr = allocated.data + new_size;
                    if(new_end_ptr > buffer_to)
                        return Allocation_Result{Allocator_State::OUT_OF_MEM};

                    u8* aligned_end = align_forward(new_end_ptr, sizeof(Slot));

                    isize new_reduced_size = ptrdiff(aligned_end, allocated.data) / SIZE_MULT;
                    slot->size = cast(u32) new_reduced_size | USED_BIT;
                    last_block_to = aligned_end;
                    break;
                }

                //if have enough size & is not a stub (& is not at end)
                if(ptrdiff(next_slot, slot) >= new_size && is_stub == false)
                {
                    u8* aligned_end = cast(u8*) next_slot;
                    isize new_reduced_size = ptrdiff(aligned_end, allocated.data) / SIZE_MULT;
                    slot->size = cast(u32) new_reduced_size | USED_BIT;
                    next_slot->prev_offset = cast(u32) new_reduced_size;
                    break;
                }

                if(is_used)
                    return Allocation_Result{Allocator_State::NOT_RESIZABLE};


                current_slot = next_slot;
            }

            current_alloced += new_size - allocated.size;
            Slice<u8> output = {allocated.data, new_size};
            return Allocation_Result{Allocator_State::OK, output};
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
        bool check_allocated(Slice<u8> allocated, isize align) const noexcept
        {
            using namespace detail;

            Slice<u8> used = {buffer_from, buffer_to - buffer_from};
            Slot *slot = (cast(Slot*) allocated.data) - 1;

            bool is_front_in_slice = is_in_slice(allocated.data, used); 
            bool is_back_in_slice  = is_in_slice(allocated.data + allocated.size, used); 
            bool is_aligned = align_forward(allocated.data, align) == allocated.data; 
            bool is_used = slot->size & USED_BIT;

            u8* aligned = align_forward(allocated.data + allocated.size, sizeof(Slot));
            isize aligned_size = aligned - allocated.data;
            isize slot_size = (slot->size & ~USED_BIT);
            bool sizes_match = slot_size * SIZE_MULT >= aligned_size;
            assert(sizes_match);

            return is_front_in_slice && is_used && is_back_in_slice && is_aligned && sizes_match;
        }

        nodisc virtual 
        Nullable<Allocator*> parent_allocator() const noexcept override 
        {
            return {parent};
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
        ~Intrusive_Stack_Scan() noexcept override
        {
            isize alloced = bytes_allocated();
            assert(alloced == 0 && "tracked size must be zero (alloced size == free size)");
            assert(last_block_from == last_block_to && last_block_from == buffer_from && "all pointers must be set to start of the buffer");
        }
    };
}
#include "undefs.h"