#pragma once
#include "memory.h"

namespace jot
{
    struct Failing_Allocator : Allocator
    {
        virtual
        Allocation_State allocate(Slice<u8>* output, isize size, isize align) noexcept override
        {
            output->data = nullptr;
            output->size = 0;
            assert(size >= 0 && is_power_of_two(align));
            return Allocation_State::UNSUPPORTED_ACTION;
        }

        virtual 
        Allocation_State deallocate(Slice<u8>, isize align) noexcept override
        {
            assert(is_power_of_two(align));
            return Allocation_State::UNSUPPORTED_ACTION;
        } 

        virtual
        Allocation_State resize(Slice<u8>* output, Slice<u8>, isize new_size, isize align) noexcept override
        {
            output->data = nullptr;
            output->size = 0;
            assert(new_size >= 0 && is_power_of_two(align));
            return Allocation_State::UNSUPPORTED_ACTION;
        } 

        virtual
        isize bytes_allocated() const noexcept override 
        {
            return SIZE_NOT_TRACKED;
        }

        virtual
        isize bytes_used() const noexcept override 
        {
            return SIZE_NOT_TRACKED;
        }

        virtual
        isize max_bytes_allocated() const noexcept override 
        {
            return SIZE_NOT_TRACKED;
        }

        virtual
        isize max_bytes_used() const noexcept override 
        {
            return SIZE_NOT_TRACKED;
        }

        virtual
        const char* name() const noexcept override
        {
            return "Failing_Allocator";
        }

        virtual
        ~Failing_Allocator() noexcept override
        {}
    };
}