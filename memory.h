#pragma once

#include <new>

#include "open_enum.h"
#include "utils.h"
#include "slice.h"
#include "standards.h"
#include "types.h"
#include "defines.h"

#define DO_ALLOCATOR_STATS
#define DO_DEALLOC_VALIDTY_CHECKS

namespace jot
{
    open_enum Allocator_State
    {
        OPEN_STATE_DECLARE("jot::Allocator_State");
        OPEN_ENUM_ENTRY(OUT_OF_MEM);
        OPEN_ENUM_ENTRY(NOT_RESIZABLE);
        OPEN_ENUM_ENTRY(INVALID_ARGS);
        OPEN_ENUM_ENTRY(INVALID_DEALLOC);
        OPEN_ENUM_ENTRY(INVALID_RESIZE);
        OPEN_ENUM_ENTRY(UNSUPPORTED_ACTION);
    }

    open_enum Allocator_Action
    {
        OPEN_ENUM_DECLARE("jot::Allocator_Action");
        OPEN_ENUM_ENTRY(ALLOCATE);
        OPEN_ENUM_ENTRY(DEALLOCATE);
        OPEN_ENUM_ENTRY(RESIZE);
        OPEN_ENUM_ENTRY(RESET);
        OPEN_ENUM_ENTRY(RELEASE_EXTRA_MEMORY);
    }

    using Allocator_State_Type = Allocator_State::Type;
    struct Allocation_Result
    {
        Allocator_State_Type state = Allocator_State::OK;
        Slice<u8> items;
    };

    template <typename T>
    struct Nullable
    {
        T value;
    };
    
    nodisc inline constexpr 
    bool is_power_of_two(isize num) noexcept 
    {
        usize n = cast(usize) num;
        return (n>0 && ((n & (n-1)) == 0));
    }

    struct Allocator
    {
        static constexpr isize SIZE_NOT_TRACKED = -1;

        nodisc virtual
        Allocation_Result allocate(isize size, isize align) noexcept = 0; 
        
        //even though deallocate and such shouldnt fail and the caller should check
        // we still allow it to fail. This is useful for signaling to backing allocator that the memory wasnt found
        // here and it should perform dealocation there instead.
        virtual 
        Allocator_State_Type deallocate(Slice<u8> allocated, isize align) noexcept = 0; 

        nodisc virtual
        Allocation_Result resize(Slice<u8> allocated, isize align, isize new_size) noexcept = 0; 
        
        nodisc virtual
        Nullable<Allocator*> parent_allocator() const noexcept = 0; 
        
        nodisc virtual
        isize bytes_allocated() const noexcept = 0;

        nodisc virtual
        isize bytes_used() const noexcept = 0;

        nodisc virtual
        isize max_bytes_allocated() const noexcept = 0;

        nodisc virtual
        isize max_bytes_used() const noexcept = 0;

        nodisc virtual
        Allocation_Result custom_action(
            Allocator_Action::Type action_type, 
            Nullable<Allocator*> other_alloc, 
            isize new_size, isize new_align, 
            Slice<u8> allocated, isize old_align, 
            Nullable<void*> custom_data) noexcept 
        {
            cast(void) action_type;
            cast(void) other_alloc;
            cast(void) new_size;
            cast(void) new_align;
            cast(void) allocated;
            cast(void) old_align;
            cast(void) custom_data;

            assert(is_power_of_two(new_align));
            assert(is_power_of_two(old_align));
            assert(new_size >= 0);
            return {Allocator_State::UNSUPPORTED_ACTION};
        }

        virtual
        ~Allocator() noexcept {}
    };

    //Fails on every allocation/deallocation
    struct Failing_Allocator : Allocator
    {
        nodisc virtual
        Allocation_Result allocate(isize size, isize align) noexcept override
        {
            assert(size >= 0 && is_power_of_two(align));
            return {Allocator_State::UNSUPPORTED_ACTION};
        }

        nodisc virtual 
        Allocator_State_Type deallocate(Slice<u8> allocated, isize align) noexcept override
        {
            assert(allocated.data != nullptr && is_power_of_two(align));
            return Allocator_State::UNSUPPORTED_ACTION;
        }

        nodisc virtual
        Allocation_Result resize(Slice<u8> allocated, isize align, isize new_size) noexcept override
        {
            cast(void) allocated; cast(void) align; cast(void) new_size; 
            assert(new_size >= 0 && is_power_of_two(align));
            return {Allocator_State::UNSUPPORTED_ACTION};
        } 

        nodisc virtual
        Nullable<Allocator*> parent_allocator() const noexcept  override 
        {
            return {nullptr};
        }

        nodisc virtual
        isize bytes_allocated() const noexcept override 
        {
            return 0;
        }

        nodisc virtual
        isize bytes_used() const noexcept override 
        {
            return 0;
        }

        nodisc virtual
        isize max_bytes_allocated() const noexcept override 
        {
            return 0;
        }

        nodisc virtual
        isize max_bytes_used() const noexcept override 
        {
            return 0;
        }
        
        virtual
        ~Failing_Allocator() noexcept override {}
    };

    //Acts as regular c++ new delete
    struct New_Delete_Allocator : Allocator
    {
        isize total_alloced = 0;
        isize max_alloced = 0;

        nodisc virtual
        Allocation_Result allocate(isize size, isize align) noexcept override
        {
            assert(size >= 0 && is_power_of_two(align));

            void* obtained = operator new(cast(size_t) size, std::align_val_t{cast(size_t) align}, std::nothrow_t{});
            if(obtained == nullptr)
                return {Allocator_State::OUT_OF_MEM};

            total_alloced += size;
            max_alloced = max(max_alloced, total_alloced);
            return {Allocator_State::OK, {cast(u8*) obtained, size}};
        }

        nodisc virtual 
        Allocator_State_Type deallocate(Slice<u8> allocated, isize align) noexcept override
        {
            assert(is_power_of_two(align));
            operator delete(allocated.data, std::align_val_t{cast(size_t) align}, std::nothrow_t{});

            total_alloced -= allocated.size;
            return Allocator_State::OK;
        } 

        nodisc virtual
        Allocation_Result resize(Slice<u8> allocated, isize align, isize new_size) noexcept override
        {
            assert(new_size >= 0 && is_power_of_two(align));
            cast(void) align; cast(void) allocated; cast(void) new_size;

            return {Allocator_State::UNSUPPORTED_ACTION};
        } 

        nodisc virtual
        Nullable<Allocator*> parent_allocator() const noexcept override 
        {
            return {nullptr};
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

        virtual
        ~New_Delete_Allocator() noexcept override
        {
        }
    };

    namespace memory_globals
    {
        inline static New_Delete_Allocator NEW_DELETE_ALLOCATOR;
        inline static Failing_Allocator    FAILING_ALLOCATOR;
        
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

        //Upon construction exchnages the DEFAULT_ALLOCATOR to the provided allocator
        // and upon destruction restores original value of DEFAULT_ALLOCATOR
        //Does safely compose
        struct Default_Swap
        {
            Allocator* new_allocator;
            Allocator* old_allocator;

            Default_Swap(Allocator* resource, Allocator* old = memory_globals::default_allocator()) : new_allocator(resource), old_allocator(old) {
                hidden::DEFAULT_ALLOCATOR = new_allocator;
            }

            ~Default_Swap() {
                hidden::DEFAULT_ALLOCATOR = old_allocator;
            }
        };

        struct Scratch_Swap
        {
            Allocator* new_allocator;
            Allocator* old_allocator;

            Scratch_Swap(Allocator* resource, Allocator* old = memory_globals::scratch_allocator()) : new_allocator(resource), old_allocator(old) {
                hidden::SCRATCH_ALLOCATOR = new_allocator;
            }

            ~Scratch_Swap() {
                hidden::SCRATCH_ALLOCATOR = old_allocator;
            }
        };


        //Since Allocator* alloc = default_allocator() is not valid constant expression it cannot be used
        // as default argument. This means that we have to create two versions of every function that would
        // ideally have default allocator argument. These two classes solve this for tme most common cases
        struct Default_Alloc
        {
            Allocator* val = default_allocator();
            Default_Alloc() = default;
            Default_Alloc(Allocator* alloc) : val(alloc) {} 
        };

        struct Scratch_Alloc
        {
            Allocator* val = scratch_allocator();
            Scratch_Alloc() = default;
            Scratch_Alloc(Allocator* alloc) : val(alloc) {} 
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
        isize offset = cast(isize) min(space.size, ptrdiff(aligned, space.data));

        return tail(space, offset);
    }

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

        Linear_Allocator(Slice<u8> buffer, Allocator* parent) noexcept 
            : buffer(buffer), parent(parent) {}

        Linear_Allocator(Slice<u8> buffer) noexcept 
            : buffer(buffer), parent(memory_globals::default_allocator()) {}

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
        Allocation_Result allocate(isize size, isize align) noexcept override
        {
            assert(filled_to >= 0 && last_alloc >= 0);
            assert(size >= 0 && is_power_of_two(align));

            Slice<u8> available = available_slice();
            Slice<u8> aligned = _align_forward_negative(available, align);

            if(aligned.size < size)
                return parent->allocate(size, align);

            Slice<u8> returned_slice = head(aligned, size);
            last_alloc = filled_to;

            isize total_alloced_bytes = returned_slice.data + returned_slice.size - available.data;
            filled_to += total_alloced_bytes;

            #ifdef DO_ALLOCATOR_STATS
            alloced += size;
            max_alloced += max(max_alloced, alloced);
            #endif

            return Allocation_Result{Allocator_State::OK, returned_slice};
        }

        nodisc virtual 
        Allocator_State_Type deallocate(Slice<u8> allocated, isize align) noexcept override
        {
            if(is_in_slice(allocated.data, buffer) == false)
                return parent->deallocate(allocated, align);

            Slice<u8> last = last_alloced_slice();
            if(allocated.data == last.data && allocated.size == last.size)  
                filled_to = last_alloc;
            
            return Allocator_State::OK;
        } 

        nodisc virtual
        Allocation_Result resize(Slice<u8> allocated, isize used_align, isize new_size) noexcept override 
        {
            assert(is_power_of_two(used_align));
            Slice<u8> last_slice = last_alloced_slice();
            if(is_in_slice(allocated.data, buffer) == false)
                return parent->resize(allocated, used_align, new_size);

            if(allocated.data != last_slice.data || allocated.size != last_slice.size)  
                return Allocation_Result{Allocator_State::NOT_RESIZABLE};

            isize new_filled_to = last_alloc + new_size;
            if(new_filled_to > buffer.size)
                return Allocation_Result{Allocator_State::OUT_OF_MEM};

            filled_to = new_filled_to;
            return Allocation_Result{Allocator_State::OK, last_slice};
        } 

        nodisc virtual
        Nullable<Allocator*> parent_allocator() const noexcept override 
        {
            return {parent};
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

        static 
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

    template <typename T>
    static constexpr isize DEF_ALIGNMENT = cast(isize) max(alignof(T), 4);

    namespace memory_constants
    {
        static constexpr i64 PAGE = 4096;
        static constexpr i64 KIBI_BYTE = 1ull << 10;
        static constexpr i64 MEBI_BYTE = 1ull << 20;
        static constexpr i64 GIBI_BYTE = 1ull << 30;
        static constexpr i64 TEBI_BYTE = 1ull << 40;
    }
}

#include "undefs.h"