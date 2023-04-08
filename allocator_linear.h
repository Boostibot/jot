#pragma once

#include "memory.h"

#define nodisc [[nodiscard]]
#define cast(a) (a)

namespace jot
{
    //Allocate linearry from a buffer. Once the buffer is filled no more
    // allocations are possible. Deallocation is possible only of the most recent allocation
    //Doesnt insert any extra data into the provided buffer
    struct Linear_Allocator : Allocator
    {
        Slice<u8> buffer = {};
        isize filled_to = 0;
        isize last_alloc = 0;
        isize alloced = 0;
        isize max_alloced = 0;

        Allocator* parent = nullptr;

        Linear_Allocator(Slice<u8> buffer, Allocator* parent = default_allocator()) noexcept 
            : buffer(buffer), parent(parent) {}

        Slice<u8> available_slice() const 
        {
            return tail(buffer, filled_to);
        }

        Slice<u8> used_slice() const 
        {
            return head(buffer, filled_to);
        }

        Slice<u8> last_alloced_slice() const 
        {
            return slice_range(buffer, last_alloc, filled_to);
        }

        nodisc virtual
        Allocation_State allocate(Slice<u8>* output, isize size, isize align) noexcept override
        {
            assert(filled_to >= 0 && last_alloc >= 0);
            assert(size >= 0 && is_power_of_two(align));

            Slice<u8> available = available_slice();
            Slice<u8> aligned = _align_forward_negative(available, align);

            if(aligned.size < size)
                return parent->allocate(output, size, align);

            *output = head(aligned, size);
            last_alloc = filled_to;

            isize total_alloced_bytes = output->data + output->size - available.data;
            filled_to += total_alloced_bytes;

            alloced += size;
            if(max_alloced < alloced)
                max_alloced = alloced;

            return Allocation_State::OK;
        }

        nodisc virtual 
        Allocation_State deallocate(Slice<u8> allocated, isize align) noexcept override
        {
            if(is_in_slice(allocated.data, buffer) == false)
                return parent->deallocate(allocated, align);

            Slice<u8> last = last_alloced_slice();
            if(allocated.data == last.data && allocated.size == last.size)  
                filled_to = last_alloc;
            
            return Allocation_State::OK;
        } 

        nodisc virtual
        Allocation_State resize(Slice<u8>* output, Slice<u8> allocated, isize used_align, isize new_size) noexcept override 
        {
            assert(is_power_of_two(used_align));
            Slice<u8> last_slice = last_alloced_slice();
            if(is_in_slice(allocated.data, buffer) == false)
                return parent->resize(output, allocated, used_align, new_size);

            if(allocated.data != last_slice.data || allocated.size != last_slice.size)  
            {
                *output = Slice<u8>{};
                return Allocation_State::NOT_RESIZABLE;
            }

            isize new_filled_to = last_alloc + new_size;
            if(new_filled_to > buffer.size)
            {
                *output = Slice<u8>{};
                return Allocation_State::OUT_OF_MEMORY;
            }

            filled_to = new_filled_to;
            *output = last_slice;
            return Allocation_State::OK;
        } 

        nodisc virtual
        isize bytes_allocated() const noexcept override 
        {
            return alloced;
        }

        nodisc virtual
        isize bytes_used() const noexcept override 
        {
            return buffer.size;
        }

        nodisc virtual
        isize max_bytes_allocated() const noexcept override 
        {
            return max_alloced;
        }

        nodisc virtual
        isize max_bytes_used() const noexcept override 
        {
            return buffer.size;
        }

        nodisc static 
        Slice<u8> _align_forward_negative(Slice<u8> space, isize align_to)
        {
            u8* aligned = align_forward(space.data, align_to);
            return Slice<u8>{aligned, space.size - ptrdiff(aligned, space.data)};
        }

        void reset() noexcept 
        {
            filled_to = 0;
            last_alloc = 0;
            alloced = 0;
        };

        virtual
        ~Linear_Allocator() noexcept override {}
    };
}

#undef nodisc
#undef cast