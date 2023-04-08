#pragma once

//@TODO: figure out how to remove this dependency!
//we could strip this one as well and instead use a custom define
#include <new>

#include <stddef.h>
#include <stdint.h>

#include "slice.h"

#define nodisc [[nodiscard]]
#define cast(a) (a)

namespace jot
{
    using u8 = uint8_t;
    using isize = ptrdiff_t;
    using usize = size_t;

    enum class Allocation_State : uint32_t
    {
        OK = 0,
        OUT_OF_MEMORY,
        UNSUPPORTED_ACTION, //the allocator doesnt support this action (resize)
        NOT_RESIZABLE,      //the allocator does support resize but this specific allocation cannot be resized
    };
    
    struct Allocator
    {
        nodisc virtual
        Allocation_State allocate(Slice<u8>* output, isize size, isize align) noexcept = 0; 
        
        virtual 
        Allocation_State deallocate(Slice<u8> allocated, isize align) noexcept = 0; 

        nodisc virtual
        Allocation_State resize(Slice<u8>* output, Slice<u8> allocated, isize new_size, isize align) noexcept = 0; 
        
        nodisc virtual
        isize bytes_allocated() const noexcept = 0;

        nodisc virtual
        isize bytes_used() const noexcept = 0;

        nodisc virtual
        isize max_bytes_allocated() const noexcept = 0;

        nodisc virtual
        isize max_bytes_used() const noexcept = 0;
        
        nodisc virtual
        const char* name() const noexcept = 0;

        virtual
        ~Allocator() noexcept {}

        //is returned from bytes_allocated, bytes_used... etc if the
        // allocator does not track this statistic 
        // (such as the malloc allocator doesnt track bytes
        static constexpr isize SIZE_NOT_TRACKED = -1;
    };
    
    nodisc constexpr 
    bool is_power_of_two(isize num) noexcept 
    {
        usize n = cast(usize) num;
        return (n>0 && ((n & (n-1)) == 0));
    }
    
    //Acts as regular c++ new delete
    struct New_Delete_Allocator : Allocator
    {
        isize total_alloced = 0;
        isize max_alloced = 0;

        nodisc virtual
        Allocation_State allocate(Slice<u8>* output, isize size, isize align) noexcept override
        {
            assert(size >= 0 && is_power_of_two(align));
            output->size = size;
            output->data = cast(u8*) operator new(cast(size_t) size, std::align_val_t{cast(size_t) align}, std::nothrow_t{});
            if(output->data == nullptr)
                return Allocation_State::OUT_OF_MEMORY;

            total_alloced += size;
            if(max_alloced < total_alloced)
                max_alloced = total_alloced;

            return Allocation_State::OK;
        }

        nodisc virtual 
        Allocation_State deallocate(Slice<u8> allocated, isize align) noexcept override
        {
            assert(is_power_of_two(align));
            operator delete(allocated.data, std::align_val_t{cast(size_t) align}, std::nothrow_t{});

            total_alloced -= allocated.size;
            return Allocation_State::OK;
        } 

        nodisc virtual
        Allocation_State resize(Slice<u8>* output, Slice<u8>, isize new_size, isize align) noexcept override
        {
            assert(new_size >= 0 && is_power_of_two(align));
            output->data = nullptr;
            output->size = 0;

            return Allocation_State::UNSUPPORTED_ACTION;
        } 

        nodisc virtual
        isize bytes_allocated() const noexcept override 
        {
            return total_alloced;
        }

        nodisc virtual
        isize bytes_used() const noexcept override 
        {
            return SIZE_NOT_TRACKED;
        }

        nodisc virtual
        isize max_bytes_allocated() const noexcept override 
        {
            return max_alloced;
        }

        nodisc virtual
        isize max_bytes_used() const noexcept override 
        {
            return SIZE_NOT_TRACKED;
        }

        nodisc virtual
        const char* name() const noexcept override
        {
            return "New_Delete_Allocator";
        }

        virtual
        ~New_Delete_Allocator() noexcept override
        {}
    };

    namespace memory_globals
    {
        inline static New_Delete_Allocator NEW_DELETE_ALLOCATOR;
        namespace hidden
        {
            thread_local inline static Allocator* DEFAULT_ALLOCATOR = &NEW_DELETE_ALLOCATOR;
            thread_local inline static Allocator* SCRATCH_ALLOCATOR = &NEW_DELETE_ALLOCATOR;
        }

        nodisc inline Allocator* default_allocator() noexcept 
        {
            return hidden::DEFAULT_ALLOCATOR;
        }

        nodisc inline Allocator* scratch_allocator() noexcept 
        {
            return hidden::SCRATCH_ALLOCATOR;
        }
        
        nodisc constexpr 
        Allocator** default_allocator_ptr() noexcept 
        {
            return &hidden::DEFAULT_ALLOCATOR;
        }

        nodisc constexpr 
        Allocator** scratch_allocator_ptr() noexcept 
        {
            return &hidden::SCRATCH_ALLOCATOR;
        }

        //Upon construction exchnages the DEFAULT_ALLOCATOR to the provided allocator
        // and upon destruction restores original value of DEFAULT_ALLOCATOR
        //Does safely compose
        struct Allocator_Swap
        {
            Allocator* new_allocator;
            Allocator* old_allocator;
            Allocator** resource;

            Allocator_Swap(Allocator* new_allocator, Allocator** resource = default_allocator_ptr()) 
                : new_allocator(new_allocator), old_allocator(*resource), resource(resource) 
            {
                *resource = new_allocator;
            }

            ~Allocator_Swap() {
                *resource = old_allocator;
            }
        };
    }
    
    using memory_globals::default_allocator;
    using memory_globals::scratch_allocator;

    template<typename T> nodisc constexpr
    bool is_in_slice(T* ptr, Slice<T> slice)
    {
        return ptr >= slice.data && ptr <= slice.data + slice.size;
    }
   
    nodisc inline
    isize ptrdiff(void* ptr1, void* ptr2)
    {
        return cast(isize) ptr1 - cast(isize) ptr2;
    }

    nodisc inline
    u8* align_forward(u8* ptr, isize align_to)
    {
        assert(is_power_of_two(align_to));

        //this is a little criptic but according to the iternet should be the fastest way of doing this
        // my benchmarks support this. 
        //(its about 50% faster than using div_round_up would be - even if we supply log2 alignment and bitshifts)
        usize mask = cast(usize) (align_to - 1);
        isize ptr_num = cast(isize) ptr;
        ptr_num += (-ptr_num) & mask;

        return cast(u8*) ptr_num;
    }

    nodisc inline
    u8* align_backward(u8* ptr, isize align_to)
    {
        assert(is_power_of_two(align_to));

        usize ualign = cast(usize) align_to;
        usize mask = ~(ualign - 1);
        usize ptr_num = cast(usize) ptr;
        ptr_num = ptr_num & mask;

        return cast(u8*) ptr_num;
    }
    
    nodisc inline
    Slice<u8> align_forward(Slice<u8> space, isize align_to)
    {
        u8* aligned = align_forward(space.data, align_to);
        isize offset = ptrdiff(aligned, space.data);
        if(offset < 0)
            offset = 0;

        return tail(space, offset);
    }

    template <typename T>
    static constexpr isize DEF_ALIGNMENT = cast(isize) (alignof(T) > 8 ? alignof(T) : 8);
    
    namespace memory_constants
    {
        static constexpr int64_t PAGE = 4096;
        static constexpr int64_t KIBI_BYTE = cast(int64_t) 1 << 10;
        static constexpr int64_t MEBI_BYTE = cast(int64_t) 1 << 20;
        static constexpr int64_t GIBI_BYTE = cast(int64_t) 1 << 30;
        static constexpr int64_t TEBI_BYTE = cast(int64_t) 1 << 40;
    }
}

#undef nodisc
#undef cast