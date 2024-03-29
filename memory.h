#pragma once

#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

#ifndef JOT_MALLOC
    #include <stdlib.h>
    #define JOT_MALLOC(size) malloc(size)
    #define JOT_FREE(ptr)    free(ptr)
#endif

using isize = ptrdiff_t;
using usize = size_t;

#define ISIZE_MAX PTRDIFF_MAX
#define USIZE_MAX SIZE_MAX

namespace jot
{
    struct Allocator_Stats;
    struct Line_Info;

    struct Allocator
    {
        ///Allocates size bytes aligned to align
        virtual void* allocate(isize size, isize align, Line_Info callee) noexcept = 0; 
        
        ///Deallocates previously allocated data. Return value signals if iternally the deallocation succeeded and can be ignored.
        virtual bool deallocate(void* allocated, isize old_size, isize align, Line_Info callee) noexcept = 0; 

        ///Attempts to resize in place some previously made allocation. If succeeds return true and the allocation is now considered
        ///to be new_size long. If fails returns false. The return value MUST be checked!
        virtual bool resize(void* allocated, isize old_size, isize new_size, isize align, Line_Info callee) noexcept = 0; 
        
        ///Returns partially filled Allocator_Stats. Not tracked fields are 0
        virtual Allocator_Stats get_stats() const noexcept = 0;

        virtual ~Allocator() noexcept {}
        
        //@NOTE: We also pass line info to each allocation function. This is used to give better error/info messages esentially for free
        //This combined with the fact that this interface operates entirely type erased means we can switch to a 'debug' or 'tracking'
        // allocator during runtime of the program on demand (such as something bugging out)
    };

    inline bool  is_power_of_two(isize num);
    inline void* align_forward(void* ptr, isize align_to);
    inline void* align_backward(void* ptr, isize align_to);

    ///c malloc except allocates aligned
    static void* aligned_malloc(isize byte_size, isize align) noexcept;
    static void  aligned_free(void* aligned_ptr, isize align) noexcept;

    ///These three functions let us easily write custom 'set_capacity' or 'realloc' functions without losing on generality or safety. (see ALLOC_RESIZE_EXAMPLE)
    ///They primarily serve to simplify writing reallocation rutines for SOA structs where we want all of the arrays to have the same capacity.
    /// this means that if one fails all the allocations should be undone (precisely what memory_resize_undo does) and the funtion should fail
    static bool memory_resize_allocate(Allocator* alloc, void** new_allocated, isize new_size, void* old_allocated, isize old_size, isize align, Line_Info callee) noexcept;
    static bool memory_resize_deallocate(Allocator* alloc, void** new_allocated, isize new_size, void* old_allocated, isize old_size, isize align, Line_Info callee) noexcept;
    static bool memory_resize_undo(Allocator* alloc, void** new_allocated, isize new_size, void* old_allocated, isize old_size, isize align, Line_Info callee) noexcept;

    ///Similar to realloc. Attempts to grow the allocation in place if succeeds return the same adress else allocates a new storage and memcopies the data over.
    ///Returns NULL on error
    static void* reallocate(Allocator* alloc, void* old_allocated, isize new_size, isize old_size, isize align, Line_Info callee) noexcept;

    ///Allocates using aligned_malloc/aligned_free
    struct Malloc_Allocator : Allocator
    {
        isize total_alloced = 0;
        isize max_alloced = 0;
        isize allocation_count = 0;
        isize deallocation_count = 0;
        isize resize_count = 0;
        
        virtual void* allocate(isize size, isize align, Line_Info) noexcept override;
        virtual bool deallocate(void* allocated, isize old_size, isize align, Line_Info callee) noexcept override;
        virtual bool resize(void* allocated, isize new_size, isize old_size, isize align, Line_Info callee) noexcept override;
        virtual Allocator_Stats get_stats() const noexcept override;
        virtual ~Malloc_Allocator() noexcept override {}
    };

    namespace memory_constants
    {
        static constexpr int64_t PAGE = 4096;
        static constexpr int64_t KIBI_BYTE = (int64_t) 1 << 10;
        static constexpr int64_t MEBI_BYTE = (int64_t) 1 << 20;
        static constexpr int64_t GIBI_BYTE = (int64_t) 1 << 30;
        static constexpr int64_t TEBI_BYTE = (int64_t) 1 << 40;
    }
    
    struct Allocator_Stats
    {
        Allocator* parent;
        const char* name;
        bool supports_resize;

        isize bytes_used;
        isize bytes_allocated;
            
        isize max_bytes_used;
        isize max_bytes_allocated;

        isize allocation_count;
        isize deallocation_count;
        isize resize_count;
    };
    
    struct Line_Info
    {
        const char* file;
        const char* func;
        isize line;
    };

    #ifndef _MSC_VER
        #define __FUNCTION__ __func__
    #endif
    #define GET_LINE_INFO() ::jot::Line_Info{__FILE__, __FUNCTION__, __LINE__}

    namespace memory_globals
    {
        using Out_Of_Memory_handler_Function = void(*)(Line_Info callee, const char* cformat, isize requested_size, Allocator* requested_from, ...);

        static void default_out_of_memory_handler(Line_Info callee, const char* cformat_string, isize requested_size, Allocator* allocator, ...);


        inline Out_Of_Memory_handler_Function* out_of_memory_hadler_ptr()
        {
            thread_local static Out_Of_Memory_handler_Function handler = default_out_of_memory_handler;
            return &handler;
        }

        inline Out_Of_Memory_handler_Function out_of_memory_hadler()
        {
            return *out_of_memory_hadler_ptr();
        }

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
            thread_local static Allocator* scratch = malloc_allocator();
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
    
    //@TODO: remove
    constexpr isize max(isize a, isize b) { return a > b ? a : b; }
    constexpr isize min(isize a, isize b) { return a < b ? a : b; }
    constexpr isize clamp(isize val, isize lo, isize hi)            { return max(lo, min(val, hi)); }
    constexpr isize div_round_up(isize value, isize to_multiple_of) { return (value + to_multiple_of - 1) / to_multiple_of; }

    template <typename T> constexpr 
    T && move(T* val) noexcept 
    { 
        return (T &&) *val; 
    };

    //@TODO: place somewhere else or btter yet remove entirely
    template <typename T> constexpr 
    void swap(T* a, T* b) noexcept 
    { 
        T copy = (T&&) *a;
        *a = (T&&) *b;
        *b = (T&&) copy;
    };
    
    //Used to stop infering of arguments. Is useful for example with get(Array<T> arr, isize index, Id<T*> if_not_found)
    // if we tried to call get(arr, 2, nullptr) it wouldnt compile without Id because the T is either nullptr or whatever arr is...
    template<class _T>
    struct _Id {using T = _T;};

    template<class T>
    using Id = typename _Id<T>::T;

    #ifndef SLICE_DEFINED
        #define SLICE_DEFINED
        template<typename T>
        struct Slice
        {
            T* data = nullptr;
            isize size = 0;

            constexpr T const& operator[](isize index) const noexcept  
            { 
                assert(0 <= index && index < size && "index out of range"); return data[index]; 
            }

            constexpr T& operator[](isize index) noexcept 
            { 
                assert(0 <= index && index < size && "index out of range"); return data[index]; 
            }
        
            constexpr operator Slice<const T>() const noexcept 
            { 
                return Slice<const T>{data, size}; 
            }
        };
    #endif 
}

//this is necessary because c++...
inline void* operator new(size_t, void* ptr) noexcept;

//if you are getting linker errors on this put this somewhere in your code or insclude <new>
// 
//   inline void* operator new(size_t, void* ptr) noexcept {return ptr;}

#if defined(_MSC_VER) && defined(_DEBUG)
   #include <crtdbg.h>
   #include <stdarg.h>
#endif

namespace jot
{   
    inline bool is_power_of_two(isize num) 
    {
        usize n = (usize) num;
        return (n>0 && ((n & (n-1)) == 0));
    }

    inline void* align_forward(void* ptr, isize align_to)
    {
        assert(is_power_of_two(align_to));

        //this is a little criptic but according to the iternet should be the fastest way of doing this
        // my benchmarks support this. 
        //(its about 50% faster than using div_round_up would be - even if we supply log2 alignment and bitshifts)
        usize mask = (usize) (align_to - 1);
        isize ptr_num = (isize) ptr;
        ptr_num += (-ptr_num) & mask;

        return (void*) ptr_num;
    }

    inline void* align_backward(void* ptr, isize align_to)
    {
        assert(is_power_of_two(align_to));

        usize ualign = (usize) align_to;
        usize mask = ~(ualign - 1);
        usize ptr_num = (usize) ptr;
        ptr_num = ptr_num & mask;

        return (void*) ptr_num;
    }
    
    
    struct Aligned_Malloc_Header
    {
        uint32_t align_padding;
        
        #ifndef NDEBUG
        uint32_t magic_number;
        #endif
    };

    static void* aligned_malloc(isize byte_size, isize align) noexcept
    {
        assert(byte_size >= 0 && is_power_of_two(align));
        //For the vast majority of cases the default align will suffice
        if(align <= sizeof(size_t))
            return JOT_MALLOC((size_t) byte_size);

        //Else we allocate extra size for alignemnt and uint32_t marker
        uint8_t* original_ptr = (uint8_t*) JOT_MALLOC(byte_size + align + sizeof(Aligned_Malloc_Header));
        uint8_t* original_end = original_ptr + byte_size + align + sizeof(Aligned_Malloc_Header);
        if(original_ptr == nullptr)
            return nullptr;

        Aligned_Malloc_Header* aligned_ptr = (Aligned_Malloc_Header*) align_forward(original_ptr + sizeof(Aligned_Malloc_Header), align);
        Aligned_Malloc_Header* header = aligned_ptr - 1;

        (void) original_end;
        assert(original_end - (uint8_t*) aligned_ptr >= byte_size);

        //set the marker thats just before the return adress 
        #ifndef NDEBUG
        header->magic_number = 0xABCDABCD;
        #endif
        header->align_padding = (uint32_t) ((uint8_t*) aligned_ptr - original_ptr);
        return aligned_ptr;
    }
    
    static void aligned_free(void* aligned_ptr, isize align) noexcept
    {
        if(aligned_ptr == nullptr || align <= sizeof(size_t))
            return JOT_FREE(aligned_ptr);
        
        Aligned_Malloc_Header* header = (Aligned_Malloc_Header*) aligned_ptr - 1;
        assert(header->magic_number == 0xABCDABCD && "Heap coruption detetected");

        uint8_t* original_ptr = (uint8_t*) aligned_ptr - header->align_padding;
        JOT_FREE(original_ptr);
    }
    
    void* Malloc_Allocator::allocate(isize size, isize align, Line_Info) noexcept
    {
        assert(size >= 0 && is_power_of_two(align));
        void* out = aligned_malloc(size, align);
        if(out == nullptr)
            return out;

        total_alloced += size;
        if(max_alloced < total_alloced)
            max_alloced = total_alloced;

        allocation_count++;
        return out;
    } 
        
    bool Malloc_Allocator::deallocate(void* allocated, isize old_size, isize align, Line_Info callee) noexcept
    {
        assert(old_size > 0 && is_power_of_two(align));
        (void) old_size; (void) align; (void) callee;
        aligned_free(allocated, align);
        total_alloced -= old_size;
        deallocation_count ++;
        return true;
    }

    bool Malloc_Allocator::resize(void* allocated, isize new_size, isize old_size, isize align, Line_Info callee) noexcept
    {
        assert(old_size > 0 && new_size >= 0 && is_power_of_two(align));
        (void) allocated; (void) new_size; (void) old_size; (void) align; (void) callee;
        resize_count++;
        return false;
    }

    Allocator_Stats Malloc_Allocator::get_stats() const noexcept
    {
        Allocator_Stats stats = {};
        stats.name = "Malloc_Allocator";
        stats.supports_resize = false;
        stats.bytes_allocated = total_alloced;
        stats.max_bytes_allocated = max_alloced;
            
        stats.allocation_count = allocation_count;
        stats.deallocation_count = deallocation_count;
        stats.resize_count = resize_count;
            
        return stats;
    }

    static bool memory_resize_allocate(Allocator* alloc, void** new_allocated, isize new_size, void* old_allocated, isize old_size, isize align, Line_Info callee) noexcept
    {
        assert(alloc != nullptr && new_allocated != nullptr);
        assert(old_size >= 0 && new_size >= 0 && is_power_of_two(align));

        if(new_size == 0)
        {
            *new_allocated = nullptr;
            return true;
        }

        if(old_allocated != nullptr && old_size != 0)
        {
            if(alloc->resize(old_allocated, old_size, new_size, align, callee))
            {
                *new_allocated = old_allocated;
                return true;
            }
        }

        *new_allocated = alloc->allocate(new_size, align, callee);
        return *new_allocated != nullptr;
    }
    
    static bool memory_resize_deallocate(Allocator* alloc, void** new_allocated, isize new_size, void* old_allocated, isize old_size, isize align, Line_Info callee) noexcept
    {
        assert(alloc != nullptr && new_allocated != nullptr);
        assert(old_size >= 0 && new_size >= 0 && is_power_of_two(align));

        if(old_size == 0)
            return true;

        if(new_size == 0 || *new_allocated != old_allocated)
            return alloc->deallocate(old_allocated, old_size, align, callee);

        return true;
    }
    
    static bool memory_resize_undo(Allocator* alloc, void** new_allocated, isize new_size, void* old_allocated, isize old_size, isize align, Line_Info callee) noexcept
    {
        assert(alloc != nullptr && new_allocated != nullptr);
        assert(old_size >= 0 && new_size >= 0 && is_power_of_two(align));

        //if nothing happened do nothing
        if(*new_allocated == nullptr)
            return true;

        //if resized resize back down
        if(*new_allocated == old_allocated)
            return alloc->resize(*new_allocated, new_size, old_size, align, callee);
        
        //else deallocate newly allocated
        return alloc->deallocate(*new_allocated, new_size, align, callee);
    }
    
    static void* reallocate(Allocator* alloc, void* old_allocated, isize new_size, isize old_size, isize align, Line_Info callee) noexcept
    {
        void* out = nullptr;
        if(memory_resize_allocate(alloc, &out, new_size, old_allocated, old_size, align, callee))
        {
            isize min_size = new_size;
            if(min_size > old_size)
                min_size = old_size;

            if(out != old_allocated)
                memcpy(out, old_allocated, (size_t) min_size);

            memory_resize_deallocate(alloc, &out, new_size, old_allocated, old_size, align, callee);
        }

        return out;
    }
    
    namespace memory_globals
    {
        static void default_out_of_memory_handler(Line_Info callee, const char*, isize requested_size, Allocator* allocator, ...)
        {
            #if defined(_MSC_VER) && defined(_DEBUG)
                using lli =  long long int;
                Allocator_Stats stats = {0};
                if(allocator != nullptr)
                    stats = allocator->get_stats();

                if(stats.name == nullptr)
                   stats.name = "<no name>";
               
                _CrtDbgReport(_CRT_ERROR, callee.file, (int) callee.line, callee.file,
                    "Allocator %s ran out of memory\n"
                    "Requested %lld B\n"
                    "Allocator_Stats:\n"
                    "  allocated: %lld max allocated: %lld\n"
                    "  num allocs: %lld num frees: %lld", stats.name, (lli) requested_size, 
                    (lli) stats.bytes_allocated, (lli) stats.max_bytes_allocated,
                    (lli) stats.allocation_count, (lli) stats.deallocation_count);
            #else
                assert("Out of memory!");
                *(const char* volatile*) 0 = "JOT_PANIC triggered! ";
            #endif

            (void) callee; (void) requested_size; (void) allocator; 
        }
    }

    #ifdef ALLOC_RESIZE_EXAMPLE
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
        bool s1 = memory_resize_allocate(alloc, &new_keys,   new_size, keys.data,   keys.size,   align, GET_LINE_INFO());
        bool s2 = memory_resize_allocate(alloc, &new_values, new_size, values.data, values.size, align, GET_LINE_INFO());
        bool s3 = memory_resize_allocate(alloc, &new_linker, new_size, linker.data, linker.size, align, GET_LINE_INFO());

        //If error occured undo the allocations made and return false (only undos if there is something to undo)
        if(!s1 || !s2 || !s3)
        {
            memory_resize_undo(alloc, &new_keys,   new_size, keys.data,   keys.size,   align, GET_LINE_INFO());
            memory_resize_undo(alloc, &new_values, new_size, values.data, values.size, align, GET_LINE_INFO());
            memory_resize_undo(alloc, &new_linker, new_size, linker.data, linker.size, align, GET_LINE_INFO());

            return false;
        }
        
        //If adress changed move data over
        if(new_keys != keys.data)     memcpy(new_keys,   keys.data,   min(keys.size, new_size));
        if(new_values != values.data) memcpy(new_values, values.data, min(values.size, new_size));
        if(new_linker != linker.data) memcpy(new_linker, linker.data, min(linker.size, new_size));
        
        //After we no longer need the old arrays deallocated them (only deallocates if the ptr is not null)
        memory_resize_deallocate(alloc, &new_keys,   new_size, keys.data,   keys.size,   align, GET_LINE_INFO());
        memory_resize_deallocate(alloc, &new_values, new_size, values.data, values.size, align, GET_LINE_INFO());
        memory_resize_deallocate(alloc, &new_linker, new_size, linker.data, linker.size, align, GET_LINE_INFO());

        //set the result
        keys = Slice<uint8_t>{(uint8_t*) new_keys, new_size};
        values = Slice<uint8_t>{(uint8_t*) new_values, new_size};
        linker = Slice<uint8_t>{(uint8_t*) new_linker, new_size};
        return true;
    }

    #endif
}
