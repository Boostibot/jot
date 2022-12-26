#pragma once

#include "memory.h"
#include "defines.h"

namespace jot
{
    namespace detail
    {
        template<typename T>
        inline func offset_ptr(T* ptr1, isize by_bytes) -> T*
        {
            u8* address = cast(u8*) cast(void*) ptr1;
            return cast(T*) cast(void*) (address + by_bytes);
        }

        inline func u8_ptr(void* ptr) -> u8*
        {
            return cast(u8*) ptr;
        }

        struct Slot 
        {
            u32 size = 0; 
        };

        static_assert(sizeof(Slot) == sizeof(u32) && alignof(Slot) == alignof(u32), "slot must be transparent");

        static constexpr u32 SLOT_PAD_VALUE = 0xffff'ffff;
        static constexpr u64 USED_BIT = (1ull << 31);
        static constexpr u64 MAX_ALLOWED_SIZE = (SLOT_PAD_VALUE & ~USED_BIT) * sizeof(Slot);

        static func size(Slot* slot) noexcept -> u64 {
            return (slot->size & ~USED_BIT) * sizeof(Slot);
        }

        static func slot(void* ptr) noexcept -> Slot* {
            u32* padding_ptr = cast(u32*) offset_ptr(ptr, -cast(isize) sizeof(Slot));
            while(*padding_ptr == SLOT_PAD_VALUE)
                padding_ptr -= 1;

            return cast(Slot*) cast(void*) padding_ptr;
        }

        static func data(Slot* slot, isize align) noexcept -> Slice<u8> {
            isize slot_size = size(slot); 
            u8* data = align_forward(u8_ptr(slot) + sizeof(Slot), align);

            return Slice<u8>{data, slot_size};
        }

        static proc place_slot(Slot* at, u32 size, void* data_start, bool used)
        {
            at->size = size / sizeof(Slot);
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

        Ring_Allocator(Slice<u8> buffer) {
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
            if (used_from == used_to)
                return false;

            if (used_to > used_from)
                return p >= used_from && p < used_to;

            return p >= used_from || p < used_to;
        }

        virtual proc allocate(isize size, isize align) noexcept -> Allocation_Result override 
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
            if(aligned_to > buffer_to) [[unlikely]]
            {
                //recalc the ptrs
                slot_from = buffer_from;
            available_from = slot_from + sizeof(Slot);
            aligned_from = align_forward(available_from, align);
            aligned_to = align_forward(aligned_from + size, alignof(Slot));
            }   

            isize aligned_size = ptrdiff(aligned_to, available_from);
            if(in_use(aligned_to) == false || aligned_size > MAX_ALLOWED_SIZE) [[unlikely]]
                return Allocation_Result{Allocator_State::OUT_OF_MEM};

            place_slot(cast(Slot*) slot_from, cast(u32) aligned_size, aligned_from, true);

            used_to = aligned_to;
            max_alloced = max(max_alloced, bytes_allocated());
            return Allocation_Result{Allocator_State::OK, Slice<u8>{aligned_from, size}};
        }

        virtual proc deallocate(Slice<u8> allocated, isize align) noexcept -> Allocator_State_Type override 
        {
            using namespace detail;
            u8* ptr = allocated.data;
            if(ptr == nullptr)
                return Allocator_State::OK;

            if(ptr < buffer_from || buffer_to <= ptr) [[unlikely]]
                return Allocator_State::INVALID_DEALLOC;

            // Mark this slot as free
            Slot *s = slot(ptr);
            s->size = s->size & ~USED_BIT;

            // Advance the free pointer past all free slots.
            while (used_from != used_to) {
                Slot* first_slot = cast(Slot*) used_from;
                if(first_slot->size & USED_BIT)
                    break;

                //since USED_BIT is 0 we can directly use the size
                Slot* next_slot = offset_ptr(first_slot + 1, first_slot->size * sizeof(Slot));

                used_from = u8_ptr(next_slot);
                if (used_from >= buffer_to)
                    used_from = buffer_from;
            }

            return Allocator_State::OK;
        } 

        virtual proc resize(Slice<u8> allocated, isize new_size) noexcept -> Allocation_Result override 
        {
            using namespace detail;
            u8* ptr = allocated.data;
            if(new_size < allocated.size || ptr < buffer_from || ptr <= buffer_to) [[unlikely]]
                return Allocation_Result{Allocator_State::INVALID_RESIZE};

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

                Slot* first_slot = cast(Slot*) used_from;

                //Here we could check two times but we can actually just merge the two checks into 
                // one. If the USED_BIT is set the size will be huge and we will probably 
                // not pass the wrap test. If we still do we can still check even the USED_BIT.

                //if((first_slot->size & USED_BIT))
                //return Allocation_Result{Allocator_State::NOT_RESIZABLE};

                Slot* next_slot = offset_ptr(first_slot + 1, first_slot->size * sizeof(Slot));

                //if needs to wrap over or isnt free
                if(u8_ptr(next_slot) >= buffer_to || first_slot->size & USED_BIT)
                    return Allocation_Result{Allocator_State::NOT_RESIZABLE};

                current_slot = next_slot;
            }

            Slot* first_slot = slot(ptr);
            first_slot->size = cast(u32) resized_to_size | USED_BIT;

            return Allocation_Result{Allocator_State::OK, Slice<u8>{ptr, new_size}};
        }

        virtual proc parent_allocator() const noexcept -> Nullable<Allocator*> override {
            return {nullptr};
        }

        virtual proc bytes_allocated() const noexcept -> isize override {

            if (used_to >= used_from)
                return ptrdiff(used_to, used_from);

            return ptrdiff(buffer_to, used_from) + ptrdiff(used_to, buffer_from);
        }

        virtual proc bytes_used() const noexcept -> isize override {
            return buffer_to - buffer_from;
        }

        virtual proc max_bytes_allocated() const noexcept -> isize override {
            return max_alloced;
        }

        virtual proc max_bytes_used() const noexcept -> isize override {
            return bytes_used();
        }

        ~Ring_Allocator()
        {
            isize alloced = bytes_allocated();
            assert(alloced == 0);
        }
    };

}

#include "undefs.h"