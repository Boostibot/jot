#pragma once
#include "memory.h"

namespace jot
{
    ///Every operation results in error. Useful for testing.
    struct Failing_Allocator : Allocator
    {
        isize allocation_count = 0;
        isize deallocation_count = 0;
        isize resize_count = 0;
        
        virtual void* allocate(isize, isize, Line_Info) noexcept override
        {
            allocation_count++;
            return nullptr;
        } 
        
        virtual bool deallocate(void*, isize, isize, Line_Info) noexcept override
        {
            deallocation_count++;
            return false;
        }

        virtual bool resize(void*, isize, isize, isize, Line_Info) noexcept override
        {
            resize_count++;
            return false;
        }

        virtual Allocator_Stats get_stats() const noexcept override
        {
            Allocator_Stats stats = {};
            stats.name = "Failing_Allocator";
            stats.supports_resize = false;
            
            stats.allocation_count = allocation_count;
            stats.deallocation_count = deallocation_count;
            stats.resize_count = resize_count;
            
            return stats;
        }

        virtual ~Failing_Allocator() noexcept override {}
    };
}