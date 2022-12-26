#pragma once

#include <memory>
#include "open_enum.h"
#include "option.h"
#include "utils.h"
#include "slice.h"
#include "defines.h"

#define assert_arg(arg) assert(arg)

#define DO_ALLOCATOR_STATS
#define DO_DEALLOC_VALIDTY_CHECKS

namespace jot
{
    namespace Allocator_State
    {
        OPEN_ENUM_DECLARE(OK);
        OPEN_ENUM_ENTRY(OUT_OF_MEM);
        OPEN_ENUM_ENTRY(NOT_RESIZABLE);
        OPEN_ENUM_ENTRY(INVALID_ARGS);
        OPEN_ENUM_ENTRY(INVALID_DEALLOC);
        OPEN_ENUM_ENTRY(INVALID_RESIZE);
        OPEN_ENUM_ENTRY(UNSUPPORTED_ACTION);
    }

    namespace Allocator_Action
    {
        OPEN_ENUM_DECLARE(NO_OP);
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

    template<>
    struct Hasable<Allocator_State_Type>
    {
        static constexpr func perform(Allocator_State_Type state) noexcept -> bool {
            return state == Allocator_State::OK;
        }
    };

    template<>
    struct Hasable<Allocation_Result>
    {
        static constexpr func perform(Allocation_Result result) noexcept -> bool {
            return result.state == Allocator_State::OK;
        }
    };

    struct Allocator
    {
        virtual proc allocate(isize size, isize align) noexcept -> Allocation_Result = 0; 
        
        //even though deallocate and such shouldnt fail and the caller should check
        // we still allow it to fail. This is useful for signaling to backing allocator that the memory wasnt found
        // here and it should perform dealocation there instead.
        virtual auto deallocate(Slice<u8> allocated, isize align) noexcept -> Allocator_State_Type = 0; 

        virtual proc resize(Slice<u8> allocated, isize align, isize new_size) noexcept -> Allocation_Result = 0; 
        
        virtual proc parent_allocator() const noexcept -> Nullable<Allocator*> = 0; 
        
        virtual proc bytes_allocated() const noexcept -> isize = 0;

        virtual proc bytes_used() const noexcept -> isize = 0;

        virtual proc max_bytes_allocated() const noexcept -> isize = 0;

        virtual proc max_bytes_used() const noexcept -> isize = 0;

        virtual proc custom_action(
            Allocator_Action::Type action_type, 
            Nullable<Allocator*> other_alloc, 
            isize new_size, u8 new_align, 
            Slice<u8> allocated, u8 old_align, 
            Nullable<void*> custom_data) noexcept -> Allocation_Result
        {
            assert_arg(new_size >= 0);
            return {Allocator_State::UNSUPPORTED_ACTION};
        }
    };

    //Fails on every allocation/deallocation
    struct Failing_Allocator : Allocator
    {
        virtual proc allocate(isize size, isize align) noexcept -> Allocation_Result override {
            assert_arg(size >= 0 && align >= 0);
            return {Allocator_State::UNSUPPORTED_ACTION};
        }

        virtual proc deallocate(Slice<u8> allocated, isize align) noexcept -> Allocator_State_Type override {
            return Allocator_State::UNSUPPORTED_ACTION;
        }

        virtual proc resize(Slice<u8> allocated, isize align, isize new_size) noexcept -> Allocation_Result override {
            assert_arg(new_size >= 0);
            return {Allocator_State::UNSUPPORTED_ACTION};
        } 

        virtual proc parent_allocator() const noexcept -> Nullable<Allocator*> override {
            return {nullptr};
        }

        virtual proc bytes_allocated() const noexcept -> isize override {
            return 0;
        }

        virtual proc bytes_used() const noexcept -> isize override {
            return 0;
        }

        virtual proc max_bytes_allocated() const noexcept -> isize override {
            return 0;
        }

        virtual proc max_bytes_used() const noexcept -> isize override {
            return 0;
        }
    };

    //Acts as regular c++ new delete
    struct New_Delete_Allocator : Allocator
    {
        isize total_alloced = 0;
        isize max_alloced = 0;

        virtual proc allocate(isize size, isize align) noexcept -> Allocation_Result override {
            assert_arg(size >= 0 && align >= 0);

            void* obtained = operator new(cast(size_t) size, std::align_val_t{cast(size_t) align}, std::nothrow_t{});
            if(obtained == nullptr)
                return {Allocator_State::OUT_OF_MEM};

            #ifdef DO_ALLOCATOR_STATS
                total_alloced += size;
                max_alloced = max(max_alloced, total_alloced);
            #endif // DO_ALLOCATOR_STATS

            return {Allocator_State::OK, {cast(u8*) obtained, size}};
        }

        virtual proc deallocate(Slice<u8> allocated, isize align) noexcept -> Allocator_State_Type override {
            operator delete(allocated.data, std::align_val_t{cast(size_t) align}, std::nothrow_t{});

            #ifdef DO_ALLOCATOR_STATS
                total_alloced -= allocated.size;
            #endif // do_memory_stats

            return Allocator_State::OK;
        } 

        virtual proc resize(Slice<u8> allocated, isize align, isize new_size) noexcept -> Allocation_Result override {
            assert_arg(new_size >= 0);

            return {Allocator_State::UNSUPPORTED_ACTION};
        } 

        virtual proc parent_allocator() const noexcept -> Nullable<Allocator*> override {
            return {nullptr};
        }

        virtual proc bytes_allocated() const noexcept -> isize override {
            #ifdef DO_ALLOCATOR_STATS
                return total_alloced;
            #else
                return -1;
            #endif // DO_ALLOCATOR_STATS
        }

        virtual proc bytes_used() const noexcept -> isize override {
            return -1;
        }

        virtual proc max_bytes_allocated() const noexcept -> isize override {
            return max_alloced;
        }

        virtual proc max_bytes_used() const noexcept -> isize override {
            return -1;
        }

        ~New_Delete_Allocator() noexcept
        {
            assert(total_alloced == 0);
        }
    };

    constexpr func is_power_of_two(isize num) noexcept -> bool {
        usize n = cast(usize) num;
        return (n>0 && ((n & (n-1)) == 0));
    }

    template<typename T>
    inline func is_in_slice(T* ptr, Slice<T> slice) -> bool
    {
        return ptr >= slice.data && ptr < slice.data + slice.size;
    }
    
    inline func ptrdiff(void* ptr1, void* ptr2) -> isize
    {
        return cast(isize) ptr1 - cast(isize) ptr2;
    }

    inline func align_forward(u8* ptr, isize align_to) -> u8*
    {
        assert_arg(is_power_of_two(align_to));

        //this is a little criptic but according to the iternet should be the fastest way of doing this
        // my benchmarks support this. 
        //(its about 50% faster than using div_round_up would be - even if we supply log2 alignment and bitshifts)
        usize mask = cast(usize) (align_to - 1);
        isize ptr_num = cast(isize) ptr;
        ptr_num += (-ptr_num) & mask;

        return cast(u8*) ptr_num;
    }

    inline func align_forward(Slice<u8> space, isize align_to) -> Slice<u8>
    {
        u8* aligned = align_forward(space.data, align_to);
        isize offset = cast(isize) min(space.size, ptrdiff(aligned, space.data));

        return slice(space, offset);
    }

    //Allocate linearry from a buffer. Once the buffer is filled no more
    // allocations are possible. Deallocation is possible only of the most recent allocation
    //Doesnt insert any extra data into the provided buffer
    struct Stack_Allocator : Allocator
    {
        Slice<u8> buffer = {};
        isize filled_to = 0;
        isize last_alloc = 0;
        isize alloced = 0;
        isize max_alloced = 0;

        Allocator* parent = nullptr;

        Stack_Allocator(Slice<u8> buffer, Allocator* parent) noexcept 
            : buffer(buffer), parent(parent) {}

        func available_slice() const -> Slice<u8> {
            return slice(buffer, filled_to);
        }

        func used_slice() const -> Slice<u8> {
            return trim(buffer, filled_to);
        }

        func last_alloced_slice() const -> Slice<u8> {
            return slice_range(buffer, last_alloc, filled_to);
        }

        virtual proc allocate(isize size, isize align) noexcept -> Allocation_Result override {
            assert(filled_to >= 0 && last_alloc >= 0);
            assert_arg(size >= 0 && align >= 0);

            Slice<u8> available = available_slice();
            Slice<u8> aligned = _align_forward_negative(available, align);

            if(aligned.size < size)
                return parent->allocate(size, align);

            Slice<u8> returned_slice = trim(aligned, size);
            last_alloc = filled_to;

            isize total_alloced_bytes = returned_slice.data + returned_slice.size - available.data;
            filled_to += total_alloced_bytes;

            #ifdef DO_ALLOCATOR_STATS
            alloced += size;
            max_alloced += max(max_alloced, alloced);
            #endif

            return Allocation_Result{Allocator_State::OK, returned_slice};
        }

        virtual proc deallocate(Slice<u8> allocated, isize align) noexcept -> Allocator_State_Type override {
            if(is_in_slice(allocated.data, buffer) == false)
                return parent->deallocate(allocated, align);

            if(allocated == last_alloced_slice())  
                filled_to = last_alloc;
            
            return Allocator_State::OK;
        } 

        virtual proc resize(Slice<u8> allocated, isize used_align, isize new_size) noexcept -> Allocation_Result override {
            
            Slice<u8> last_slice = last_alloced_slice();
            if(is_in_slice(allocated.data, buffer) == false)
                return parent->resize(allocated, used_align, new_size);

            if(allocated != last_slice)
                return Allocation_Result{Allocator_State::NOT_RESIZABLE};

            isize new_filled_to = last_alloc + new_size;
            if(new_filled_to > buffer.size)
                return Allocation_Result{Allocator_State::OUT_OF_MEM};

            filled_to = new_filled_to;
            return Allocation_Result{Allocator_State::OK, last_slice};
        } 

        virtual proc parent_allocator() const noexcept -> Nullable<Allocator*> override {
            return {parent};
        }

        virtual proc bytes_allocated() const noexcept -> isize override {
            return alloced;
        }

        virtual proc bytes_used() const noexcept -> isize override {
            return buffer.size;
        }

        virtual proc max_bytes_allocated() const noexcept -> isize override {
            return max_alloced;
        }

        virtual proc max_bytes_used() const noexcept -> isize override {
            return buffer.size;
        }

        static func _align_forward_negative(Slice<u8> space, isize align_to) -> Slice<u8>
        {
            u8* aligned = align_forward(space.data, align_to);
            return Slice<u8>{aligned, space.size - ptrdiff(aligned, space.data)};
        }
    };

    namespace memory_globals
    {
        static New_Delete_Allocator NEW_DELETE_ALLOCATOR;
        static Failing_Allocator    FAILING_ALLOCATOR;

        namespace hidden
        {
            thread_local static Allocator* DEFAULT_ALLOCATOR = &NEW_DELETE_ALLOCATOR;
            thread_local static Allocator* SCRATCH_ALLOCATOR = &NEW_DELETE_ALLOCATOR;
        }

        inline Allocator* default_allocator() noexcept {
            return hidden::DEFAULT_ALLOCATOR;
        }

        inline Allocator* scratch_allocator() noexcept {
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
    }

    template <typename T>
    static constexpr isize DEF_ALIGNMENT = max(alignof(T), 4);

    namespace memory_constants
    {
        static constexpr isize PAGE = 4096;
        static constexpr isize KIBI_BYTE = 1ull << 10;
        static constexpr isize MEBI_BYTE = 1ull << 20;
        static constexpr isize GIBI_BYTE = 1ull << 30;
        static constexpr isize TEBI_BYTE = 1ull << 40;
    }
}

#include "undefs.h"