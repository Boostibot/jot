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
    //@TODO: remove!
    inline Slice<uint8_t> align_forward(Slice<uint8_t> space, isize align_to)
    {
        uint8_t* aligned = align_forward(space.data, align_to);
        isize offset = ptrdiff(aligned, space.data);
        if(offset < 0)
            offset = 0;

        return tail(space, offset);
    }


    struct Line_Info
    {
        const char* file = "";
        const char* func = "";
        int line = -1;
    };
    
    #define GET_LINE_INFO() ::jot::Line_Info{__FILE__, __FUNCTION__, __LINE__}

    struct Allocator
    {
        struct Stats
        {
            //if a returned stats struct contains 
            //default values it mean that the speicific
            //statistic is not tracked

            isize bytes_used = -1;
            isize bytes_allocated = -1;
            isize max_bytes_used = -1;
            isize max_bytes_allocated = -1;
            bool supports_resize = false;
            const char* name = nullptr;
            Allocator* parent = nullptr;
        };

        //We also pass line info to each allocation function. This is used to give better error/info messages esentially for free
        //This combined with the fact that this interface operates entirely type erased means we can switch to a 'debug' or 'tracking'
        // allocator during runtime of the program on demand (such as something bugging out)

        NODISCARD virtual
        void* allocate(isize size, isize align, Line_Info callee) noexcept = 0; 
        
        virtual 
        bool deallocate(void* allocated, isize old_size, isize align, Line_Info callee) noexcept = 0; 

        NODISCARD virtual
        bool resize(void* allocated, isize new_size, isize old_size, isize align, Line_Info callee) noexcept = 0; 
        
        virtual
        Stats get_stats() const noexcept = 0;

        virtual
        ~Allocator() noexcept {}
    };

    inline bool is_power_of_two(isize num);
    inline ptrdiff_t ptrdiff(void* ptr1, void* ptr2);
    inline uint8_t* align_forward(uint8_t* ptr, isize align_to);
    inline uint8_t* align_backward(uint8_t* ptr, isize align_to);
    inline void* malloc_aligned(size_t byte_size, size_t align);
    inline void free_aligned(void* aligned_ptr, size_t byte_size, size_t align);
    
    //These three functions let us easily write custom 'set_capacity' or 'realloc' functions without losing on generality or safety. (see ALLOC_RESIZE_EXAMPLE)
    //They primarily serve to simplify writing reallocation rutines for SOA structs where we want all of the arrays to have the same capacity.
    // this means that if one fails all the allocations should be undone (precisely what resize_undo does) and the funtion should fail
    NODISCARD 
    static bool resize_allocate(Allocator* alloc, void** new_allocated, isize new_size, void* old_allocated, isize old_size, isize align, Line_Info callee) noexcept;
    static bool resize_deallocate(Allocator* alloc, void** new_allocated, isize new_size, void* old_allocated, isize old_size, isize align, Line_Info callee) noexcept;
    static bool resize_undo(Allocator* alloc, void** new_allocated, isize new_size, void* old_allocated, isize old_size, isize align, Line_Info callee) noexcept;

    //Acts as regular malloc
    struct Malloc_Allocator : Allocator
    {
        isize total_alloced = 0;
        isize max_alloced = 0;
        
        NODISCARD virtual
        void* allocate(isize size, isize align, Line_Info) noexcept
        {
            assert(size >= 0 && is_power_of_two(align));
            void* out = malloc_aligned(size, align);
            if(out == nullptr)
                return out;

            total_alloced += size;
            if(max_alloced < total_alloced)
                max_alloced = total_alloced;

            return out;
        } 
        
        virtual 
        bool deallocate(void* allocated, isize old_size, isize align, Line_Info callee) noexcept
        {
            assert(old_size > 0 && is_power_of_two(align));
            free_aligned(allocated, old_size, align);
            total_alloced -= old_size;
            return true;
        }

        NODISCARD virtual
        bool resize(void* allocated, isize new_size, isize old_size, isize align, Line_Info callee) noexcept
        {
            assert(old_size > 0 && new_size >= 0 && is_power_of_two(align));
            return false;
        }

        virtual
        Stats get_stats() const noexcept override
        {
            Stats stats = {};
            stats.name = "Malloc_Allocator";
            stats.supports_resize = false;
            stats.bytes_allocated = total_alloced;
            stats.max_bytes_allocated = max_alloced;
            return stats;
        }

        virtual
        ~Malloc_Allocator() noexcept override {}
    };

    namespace memory_globals
    {
        inline Malloc_Allocator* malloc_allocator() noexcept 
        {
            thread_local static Malloc_Allocator alloc;
            return &alloc;
        }
        
        inline Allocator** default_allocator_ptr() noexcept 
        {
            thread_local static Allocator* alloc = malloc_allocator();
            return &alloc;
        }

        inline Allocator** scratch_allocator_ptr() noexcept 
        {
            thread_local inline static Allocator* scratch = malloc_allocator();
            return &scratch;
        }

        inline Allocator* default_allocator() noexcept 
        {
            return *default_allocator_ptr();
        }

        inline Allocator* scratch_allocator() noexcept 
        {
            return *scratch_allocator_ptr();
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

            ~Allocator_Swap() 
            {
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
    
    static bool resize_allocate(Allocator* alloc, void** new_allocated, isize new_size, void* old_allocated, isize old_size, isize align, Line_Info callee) noexcept
    {
        assert(alloc != nullptr && new_allocated != nullptr);
        assert(old_size >= 0 && new_size >= 0 && is_power_of_two(align));

        if(new_size == 0)
        {
            *new_allocated = nullptr;
            return true;
        }

        if(old_allocated != nullptr)
        {
            if(alloc->resize(old_allocated, new_size, old_size, align, callee))
            {
                *new_allocated = old_allocated;
                return true;
            }
        }

        *new_allocated = alloc->allocate(new_size, align, callee);
        return new_allocated = nullptr;
    }
    
    static bool resize_deallocate(Allocator* alloc, void** new_allocated, isize new_size, void* old_allocated, isize old_size, isize align, Line_Info callee) noexcept
    {
        assert(alloc != nullptr && new_allocated != nullptr);
        assert(old_size >= 0 && new_size >= 0 && is_power_of_two(align));

        if(old_size == 0)
            return true;

        if(new_size == 0 || *new_allocated != old_allocated)
            return alloc->deallocate(*new_allocated, old_size, align, callee);
    }
    
    static bool resize_undo(Allocator* alloc, void** new_allocated, isize new_size, void* old_allocated, isize old_size, isize align, Line_Info callee) noexcept
    {
        assert(alloc != nullptr && new_allocated != nullptr);
        assert(old_size >= 0 && new_size >= 0 && is_power_of_two(align));

        //if nothing happened do nothing
        if(*new_allocated == nullptr)
            return true;

        //if resized resize back down
        if(*new_allocated == old_allocated)
            return alloc->resize(*new_allocated, old_size, new_size, align, callee);
        
        //else deallocate newly allocated
        return alloc->deallocate(*new_allocated, new_size, align, callee);
    }

    
    #ifndef ALLOC_RESIZE_EXAMPLE
    static void destroy_extra(Slice<uint8_t> slice, isize from_size);
    
    static bool set_capacity(Allocator* alloc, isize new_size)
    {
        isize align = 16;

        //suppose these are filled with some data, or they are null if the struct hasnt yet been allocated
        // both will work just fine
        Slice<uint8_t> keys = {};
        Slice<uint8_t> values = {};
        Slice<uint8_t> linker = {};

        //destroy extra values - resize down should never fail 
        // nor deallocation so this is fine
        destroy_extra(keys, new_size);
        destroy_extra(values, new_size);
        destroy_extra(linker, new_size);

        void* new_keys  = nullptr;
        void* new_values = nullptr;
        void* new_linker = nullptr;

        //Do allocation part of resize
        bool s1 = resize_allocate(alloc, &new_keys, new_size, keys.data, keys.size, align, GET_LINE_INFO());
        bool s2 = resize_allocate(alloc, &new_values, new_size, values.data, values.size, align, GET_LINE_INFO());
        bool s3 = resize_allocate(alloc, &new_linker, new_size, linker.data, linker.size, align, GET_LINE_INFO());

        //If error occured undo the allocations made and return false (only undos if there is something to undo)
        if(!s1 || !s2 || !s3)
        {
            resize_undo(alloc, &new_keys, new_size, keys.data, keys.size, align, GET_LINE_INFO());
            resize_undo(alloc, &new_values, new_size, values.data, values.size, align, GET_LINE_INFO());
            resize_undo(alloc, &new_linker, new_size, linker.data, linker.size, align, GET_LINE_INFO());

            return false;
        }
        
        //If adress change move data over
        if(new_keys != keys.data)     memcmp(new_keys, keys.data, keys.size);
        if(new_values != values.data) memcmp(new_values, values.data, values.size);
        if(new_linker != linker.data) memcmp(new_linker, linker.data, linker.size);
        
        //After we no longer need the old arrays deallocated them (only deallocates if the ptr is not null)
        resize_deallocate(alloc, &new_keys, new_size, keys.data, keys.size, align, GET_LINE_INFO());
        resize_deallocate(alloc, &new_values, new_size, values.data, values.size, align, GET_LINE_INFO());
        resize_deallocate(alloc, &new_linker, new_size, linker.data, linker.size, align, GET_LINE_INFO());

        //set the result
        keys = Slice<uint8_t>{(uint8_t*) new_keys, new_size};
        values = Slice<uint8_t>{(uint8_t*) new_values, new_size};
        linker = Slice<uint8_t>{(uint8_t*) new_linker, new_size};
        return true;
    }
    #endif
}
