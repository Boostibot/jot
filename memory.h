#pragma once

#include <stddef.h>
#include <stdint.h>

//Set this to use c++ new delete instead of c malloc
#ifdef JOT_USE_NEW_DELETE

    #include <new>

    #define JOT_MALLOC(size, align) operator new((size_t) size, std::align_val_t{(size_t) align}, std::nothrow_t{})
    #define JOT_FREE(ptr, size, align) operator delete(ptr, std::align_val_t{(size_t) align}, std::nothrow_t{})
    #define JOT_DEFAULT_MALLOC_ALIGN -1
    #define JOT_USE_CUSTOM_ALLOCATOR

#endif

//We give option to use external default allocators 
//(such as the c++ aligned new delete)
#ifndef JOT_USE_CUSTOM_ALLOCATOR

    #include <stdlib.h>
    //even though we only use some arguments we give option to 
    // use all so that proper align sensitive routines can be
    // provided
    #define JOT_MALLOC(size, align) malloc(size)
    #define JOT_FREE(ptr, size, align) free(ptr)

    //Sets the default alignment of the function given to JOT_MALLOC
    //if your function implements alignment set this to -1 and no
    //alignment overhead will ever happen
    #define JOT_DEFAULT_MALLOC_ALIGN 8

#endif

#ifndef NODISCARD
    #define NODISCARD [[nodiscard]]
#endif

#include "slice.h"


namespace jot
{
    enum class Allocation_State : uint32_t
    {
        OK = 0,
        OUT_OF_MEMORY,
        UNSUPPORTED_ACTION, //the allocator doesnt support this action (resize)
        NOT_RESIZABLE,      //the allocator does support resize but this specific allocation cannot be resized
    };
    
    struct Allocator
    {
        NODISCARD virtual
        Allocation_State allocate(Slice<uint8_t>* output, isize size, isize align) noexcept = 0; 
        
        virtual 
        Allocation_State deallocate(Slice<uint8_t> allocated, isize align) noexcept = 0; 

        NODISCARD virtual
        Allocation_State resize(Slice<uint8_t>* output, Slice<uint8_t> allocated, isize new_size, isize align) noexcept = 0; 
        
        virtual
        isize bytes_allocated() const noexcept = 0;

        virtual
        isize bytes_used() const noexcept = 0;

        virtual
        isize max_bytes_allocated() const noexcept = 0;

        virtual
        isize max_bytes_used() const noexcept = 0;
        
        virtual
        const char* name() const noexcept = 0;

        virtual
        ~Allocator() noexcept {}

        //is returned from bytes_allocated, bytes_used... etc if the
        // allocator does not track this statistic 
        // (such as the malloc allocator doesnt track bytes
        static constexpr isize SIZE_NOT_TRACKED = -1;
    };
    
    
    template<typename T> constexpr
    inline bool is_in_slice(T* ptr, Slice<T> slice);
    inline bool is_power_of_two(isize num);
    inline ptrdiff_t ptrdiff(void* ptr1, void* ptr2);
    inline uint8_t* align_forward(uint8_t* ptr, isize align_to);
    inline uint8_t* align_backward(uint8_t* ptr, isize align_to);
    inline Slice<uint8_t> align_forward(Slice<uint8_t> space, isize align_to);
    inline void* malloc_aligned(size_t byte_size, size_t align);
    inline void free_aligned(void* aligned_ptr, size_t byte_size, size_t align);

    //Acts as regular maloc
    struct Default_Allocator : Allocator
    {
        isize total_alloced = 0;
        isize max_alloced = 0;

        NODISCARD virtual
        Allocation_State allocate(Slice<uint8_t>* output, isize size, isize align) noexcept override
        {
            output->size = size;
            output->data = (uint8_t*) malloc_aligned(size, align);
            if(output->data == nullptr)
                return Allocation_State::OUT_OF_MEMORY;

            total_alloced += size;
            if(max_alloced < total_alloced)
                max_alloced = total_alloced;

            return Allocation_State::OK;
        }

        virtual 
        Allocation_State deallocate(Slice<uint8_t> allocated, isize align) noexcept override
        {
            free_aligned(allocated.data, allocated.size, align);

            total_alloced -= allocated.size;
            return Allocation_State::OK;
        } 

        NODISCARD virtual
        Allocation_State resize(Slice<uint8_t>* output, Slice<uint8_t>, isize new_size, isize align) noexcept override
        {
            assert(new_size >= 0 && is_power_of_two(align));
            output->data = nullptr;
            output->size = 0;

            return Allocation_State::UNSUPPORTED_ACTION;
        } 

        virtual
        isize bytes_allocated() const noexcept override 
        {
            return total_alloced;
        }

        virtual
        isize bytes_used() const noexcept override 
        {
            return SIZE_NOT_TRACKED;
        }

        virtual
        isize max_bytes_allocated() const noexcept override 
        {
            return max_alloced;
        }

        virtual
        isize max_bytes_used() const noexcept override 
        {
            return SIZE_NOT_TRACKED;
        }

        virtual
        const char* name() const noexcept override
        {
            return "Default_Allocator";
        }

        virtual
        ~Default_Allocator() noexcept override {}
    };

    namespace memory_globals
    {
        inline static Default_Allocator NEW_DELETE_ALLOCATOR;
        namespace hidden
        {
            thread_local inline static Allocator* DEFAULT_ALLOCATOR = &NEW_DELETE_ALLOCATOR;
            thread_local inline static Allocator* SCRATCH_ALLOCATOR = &NEW_DELETE_ALLOCATOR;
        }

        inline Allocator* default_allocator() noexcept 
        {
            return hidden::DEFAULT_ALLOCATOR;
        }

        inline Allocator* scratch_allocator() noexcept 
        {
            return hidden::SCRATCH_ALLOCATOR;
        }
        
        inline Allocator** default_allocator_ptr() noexcept 
        {
            return &hidden::DEFAULT_ALLOCATOR;
        }

        inline Allocator** scratch_allocator_ptr() noexcept 
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


    template <typename T>
    static constexpr isize DEF_ALIGNMENT = (isize) (alignof(T) > 8 ? alignof(T) : 8);
    
    namespace memory_constants
    {
        static constexpr int64_t PAGE = 4096;
        static constexpr int64_t KIBI_BYTE = (int64_t) 1 << 10;
        static constexpr int64_t MEBI_BYTE = (int64_t) 1 << 20;
        static constexpr int64_t GIBI_BYTE = (int64_t) 1 << 30;
        static constexpr int64_t TEBI_BYTE = (int64_t) 1 << 40;
    }
}

namespace jot
{
    
    template<typename T> constexpr
    inline bool is_in_slice(T* ptr, Slice<T> slice)
    {
        return ptr >= slice.data && ptr <= slice.data + slice.size;
    }

    inline bool is_power_of_two(isize num) 
    {
        usize n = (usize) num;
        return (n>0 && ((n & (n-1)) == 0));
    }
   
    inline isize ptrdiff(void* ptr1, void* ptr2)
    {
        return (isize) ptr1 - (isize) ptr2;
    }

    inline uint8_t* align_forward(uint8_t* ptr, isize align_to)
    {
        assert(is_power_of_two(align_to));

        //this is a little criptic but according to the iternet should be the fastest way of doing this
        // my benchmarks support this. 
        //(its about 50% faster than using div_round_up would be - even if we supply log2 alignment and bitshifts)
        usize mask = (usize) (align_to - 1);
        isize ptr_num = (isize) ptr;
        ptr_num += (-ptr_num) & mask;

        return (uint8_t*) ptr_num;
    }

    inline uint8_t* align_backward(uint8_t* ptr, isize align_to)
    {
        assert(is_power_of_two(align_to));

        usize ualign = (usize) align_to;
        usize mask = ~(ualign - 1);
        usize ptr_num = (usize) ptr;
        ptr_num = ptr_num & mask;

        return (uint8_t*) ptr_num;
    }
    
    inline Slice<uint8_t> align_forward(Slice<uint8_t> space, isize align_to)
    {
        uint8_t* aligned = align_forward(space.data, align_to);
        isize offset = ptrdiff(aligned, space.data);
        if(offset < 0)
            offset = 0;

        return tail(space, offset);
    }

    inline void* malloc_aligned(size_t byte_size, size_t align)
    {
        assert(byte_size >= 0 && is_power_of_two(align));
        //For the vast majority of cases the default align will suffice
        if(align <= (size_t) JOT_DEFAULT_MALLOC_ALIGN)
            return JOT_MALLOC(byte_size, align);

        //Else we allocate extra size for alignemnt and uint32_t marker
        uint8_t* original_ptr = (uint8_t*) JOT_MALLOC(byte_size + align + sizeof(uint32_t), align);
        if(original_ptr == nullptr)
            return nullptr;

        uint32_t* aligned_ptr = (uint32_t*) align_forward(original_ptr + sizeof(uint32_t), align);

        //set the marker thats just before the return adress 
        *(aligned_ptr - 1) = (uint32_t) ptrdiff(original_ptr, aligned_ptr);
        return aligned_ptr;
    }
    
    inline void free_aligned(void* aligned_ptr, size_t byte_size, size_t align)
    {
        (void) byte_size;
        if(aligned_ptr == nullptr || align <= (size_t) JOT_DEFAULT_MALLOC_ALIGN)
            return JOT_FREE(aligned_ptr, byte_size, align);

        uint32_t offset = *((uint32_t*) aligned_ptr - 1);
        uint8_t* original_ptr = (uint8_t*) aligned_ptr - offset;
        JOT_FREE((void*) original_ptr, byte_size, align);
    }
}
