#pragma once

#include <concepts>
#include <memory>
#include "open_enum.h"
#include "option.h"
#include "utils.h"
#include "slice.h"
#include "defines.h"

#define DO_MEMORY_STATS
#define assert_arg(arg) assert(arg)

namespace jot
{
    namespace Allocator_State
    {
        OPEN_ENUM_DECLARE(OK);
        OPEN_ENUM_ENTRY(OUT_OF_MEM);
        OPEN_ENUM_ENTRY(NOT_RESIZABLE);
        OPEN_ENUM_ENTRY(INVALID_ARGS);
        OPEN_ENUM_ENTRY(INVALID_DEALLOC);
        OPEN_ENUM_ENTRY(UNSUPPORTED_ACTION);
    }

    using Allocator_State_Type = Allocator_State::Type;

    template<>
    struct Hasable<Allocator_State_Type>
    {
        static constexpr func perform(Allocator_State_Type state) noexcept -> bool {
            return state == Allocator_State::OK;
        }
    };

    struct Allocator_Result
    {
        Allocator_State_Type state = Allocator_State::OK;
        Slice<u8> items;
    };

    namespace Allocator_Action
    {
        OPEN_ENUM_DECLARE(NO_OP);
        OPEN_ENUM_ENTRY(ALLOCATE);
        OPEN_ENUM_ENTRY(DEALLOCATE);
        OPEN_ENUM_ENTRY(RESIZE);
        OPEN_ENUM_ENTRY(RESET);
        OPEN_ENUM_ENTRY(RELEASE_EXTRA_MEMORY);
    }

    template <typename T>
    static constexpr isize DEF_ALIGNMENT = max(alignof(T), alignof(std::max_align_t));

    struct Allocator
    {
        virtual proc allocate(isize size, isize align) noexcept -> Allocator_Result = 0; 
        
        virtual proc deallocate(Slice<u8> allocated, isize align) noexcept -> Allocator_State_Type = 0; 

        virtual proc resize(Slice<u8> allocated, isize new_size) noexcept -> Allocator_Result = 0; 
        
        virtual proc parent_allocator() const noexcept -> Nullable<Allocator*> = 0; 
        
        virtual proc bytes_allocated() const noexcept -> isize = 0;

        virtual proc bytes_used() const noexcept -> isize = 0;

        virtual proc custom_action(
            Allocator_Action::Type action_type, 
            Nullable<Allocator*> other_alloc, 
            isize new_size, u8 new_align, 
            Slice<u8> allocated, u8 old_align, 
            Nullable<void*> custom_data) noexcept -> Allocator_Result
        {
            assert_arg(new_size >= 0);
            return {Allocator_State::UNSUPPORTED_ACTION};
        }
    };


    //Fails on every allocation/deallocation
    struct Failing_Allocator : Allocator
    {
        virtual proc allocate(isize size, isize align) noexcept -> Allocator_Result override {
            assert_arg(size >= 0 && align >= 0);
            return {Allocator_State::UNSUPPORTED_ACTION};
        }

        virtual proc deallocate(Slice<u8> allocated, isize align) noexcept -> Allocator_State_Type override {
            return {Allocator_State::UNSUPPORTED_ACTION};;
        }

        virtual proc resize(Slice<u8> allocated, isize new_size) noexcept -> Allocator_Result override {
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
    };

    //Acts as regular c++ new delete
    struct New_Delete_Allocator : Allocator
    {
        isize total_alloced = 0;
        isize max_alloced = 0;

        virtual proc allocate(isize size, isize align) noexcept -> Allocator_Result override {
            assert_arg(size >= 0 && align >= 0);

            //void* obtained = operator new(cast(size_t) size, std::align_val_t{cast(size_t) align}, std::nothrow_t{});
            void* obtained = operator new(cast(size_t) size, std::align_val_t{cast(size_t) align});
            if(obtained == nullptr)
                return {Allocator_State::OUT_OF_MEM};

            #ifdef DO_MEMORY_STATS
                total_alloced += size;
                max_alloced = max(max_alloced, total_alloced);
            #endif // DO_MEMORY_STATS

            return {Allocator_State::OK, {cast(u8*) obtained, size}};
        }

        virtual proc deallocate(Slice<u8> allocated, isize align) noexcept -> Allocator_State_Type override {
            //operator delete(allocated.data, std::align_val_t{cast(size_t) align}, std::nothrow_t{});
            operator delete(allocated.data, std::align_val_t{cast(size_t) align});

            #ifdef DO_MEMORY_STATS
                total_alloced -= allocated.size;
                return total_alloced >= 0 ? Allocator_State::OK : Allocator_State::INVALID_DEALLOC;
            #else
                return Alloc_State::OK;
            #endif // DO_MEMORY_STATS
        } 

        virtual proc resize(Slice<u8> allocated, isize new_size) noexcept -> Allocator_Result override {
            assert_arg(new_size >= 0);

            return {Allocator_State::UNSUPPORTED_ACTION};
        } 

        virtual proc parent_allocator() const noexcept -> Nullable<Allocator*> override {
            return {nullptr};
        }

        virtual proc bytes_allocated() const noexcept -> isize override {
            #ifdef DO_MEMORY_STATS
                return total_alloced;
            #else
                return -1;
            #endif // DO_MEMORY_STATS
        }

        virtual proc bytes_used() const noexcept -> isize override {
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
    constexpr func is_in_slice(T* ptr, Slice<T> slice) -> bool
    {
        return ptr >= slice.data && ptr < slice.data + slice.size;
    }

    constexpr func align_forward(uintptr_t ptr_num, isize align_to) -> uintptr_t
    {
        uintptr_t align_to_ = align_to;
        return div_round_up(ptr_num, align_to_) * align_to_;
    }
    
    constexpr func align_forward(u8* ptr, isize align_to) -> u8*
    {
        return cast(u8*) align_forward(cast(uintptr_t) ptr, align_to);
    }

    
    constexpr func ptrdiff(void* ptr1, void* ptr2) -> isize
    {
        return cast(isize) (cast(ptrdiff_t) ptr1 - cast(ptrdiff_t) ptr2);

    }

    constexpr func align_forward(Slice<u8> space, isize align_to) -> Slice<u8>
    {
        uintptr_t ptr_num = cast(uintptr_t) space.data;
        uintptr_t aligned_num = align_forward(ptr_num, align_to);
        uintptr_t offset = min(aligned_num - ptr_num, space.size);

        return slice(space, offset);
    }

    //internal align function that might result in illegal negative sized slice
    constexpr func _align_forward_negative(Slice<u8> space, isize align_to) -> Slice<u8>
    {
        uintptr_t ptr_num = cast(uintptr_t) space.data;
        uintptr_t aligned_num = align_forward(ptr_num, align_to);
        
        return Slice<u8>{
            cast(u8*) cast(void*) aligned_num, 
            space.size - cast(isize) ptr_num + cast(isize) aligned_num
        };
    }

    struct Stack_Allocator : Allocator
    {
        Slice<u8> buffer;
        isize filled_to = 0;
        isize last_alloc = 0;
        isize max_used = 0;
        isize max_single_alloc = 0;

        Stack_Allocator(Slice<u8> buffer) noexcept : buffer(buffer) {}

        func available_slice() const -> Slice<u8> {
            return slice(buffer, filled_to);
        }

        func used_slice() const -> Slice<u8> {
            return trim(buffer, filled_to);
        }

        func last_alloced_slice() const -> Slice<u8> {
            return slice_range(buffer, last_alloc, filled_to);
        }

        virtual proc allocate(isize size, isize align) noexcept -> Allocator_Result override {
            assert(filled_to >= 0 && last_alloc >= 0);
            assert_arg(size >= 0 && align >= 0);

            Slice<u8> available = available_slice();
            Slice<u8> aligned = _align_forward_negative(available, align);

            if(aligned.size < size)
                return Allocator_Result{Allocator_State::OUT_OF_MEM};

            Slice<u8> alloced = trim(aligned, size);
            last_alloc = filled_to;

            isize total_alloced_bytes = alloced.data + alloced.size - available.data;
            filled_to += total_alloced_bytes;

            #ifdef DO_MEMORY_STATS
            max_used = max(filled_to, max_used);
            max_single_alloc = max(total_alloced_bytes, max_single_alloc);
            #endif

            return Allocator_Result{Allocator_State::OK, alloced};
        }

        virtual proc deallocate(Slice<u8> allocated, isize align) noexcept -> Allocator_State_Type override {
            if(allocated == last_alloced_slice())  
                filled_to = last_alloc;

            return Allocator_State::OK;
        } 

        virtual proc resize(Slice<u8> allocated, isize new_size) noexcept -> Allocator_Result override {
            Slice<u8> last_slice = last_alloced_slice();
            if(allocated != last_slice)
                return Allocator_Result{Allocator_State::NOT_RESIZABLE};

            isize new_filled_to = last_alloc + new_size;
            if(new_filled_to > buffer.size)
                return Allocator_Result{Allocator_State::OUT_OF_MEM};

            filled_to = new_filled_to;
            return Allocator_Result{Allocator_State::OK, last_slice};
        } 

        virtual proc parent_allocator() const noexcept -> Nullable<Allocator*> override {
            return {nullptr};
        }

        virtual proc bytes_allocated() const noexcept -> isize override {
            return filled_to;
        }

        virtual proc bytes_used() const noexcept -> isize override {
            return buffer.size;
        }

        proc reset() -> void {
            filled_to = 0;
            last_alloc = 0;
        }

        virtual proc custom_action(
            Allocator_Action::Type action_type, 
            Nullable<Allocator*> other_alloc, 
            isize new_size, u8 new_align, 
            Slice<u8> allocated, u8 old_align, 
            Nullable<void*> custom_data) noexcept -> Allocator_Result override
        {
            if(action_type == Allocator_Action::RESET)
            {  
                reset();
                return Allocator_Result{Allocator_State::OK};
            }

            return Allocator_Result{Allocator_State::UNSUPPORTED_ACTION};
        }
    };

    namespace allocator_globals
    {
        static New_Delete_Allocator NEW_DELETE_ALLOCATOR;
        static Failing_Allocator    FAILING_ALLOCATOR;

        static Allocator* DEFAULT = &NEW_DELETE_ALLOCATOR;
        static Allocator* SCRATCH = &NEW_DELETE_ALLOCATOR;
    }

    //Upon construction exchnages the DEFAULT to the provided allocator
    // and upon destruction restores original value of DEFAULT
    //Does safely compose
    struct Default_Allocator_Swap
    {
        Allocator* new_allocator;
        Allocator* old_allocator;

        Default_Allocator_Swap(Allocator* resource, Allocator* old = allocator_globals::DEFAULT) : new_allocator(resource), old_allocator(old) {
            allocator_globals::DEFAULT = new_allocator;
        }

        ~Default_Allocator_Swap() {
            allocator_globals::DEFAULT = old_allocator;
        }
    };

    struct Scratch_Allocator_Swap
    {
        Allocator* new_allocator;
        Allocator* old_allocator;

        Scratch_Allocator_Swap(Allocator* resource, Allocator* old = allocator_globals::SCRATCH) : new_allocator(resource), old_allocator(old) {
            allocator_globals::SCRATCH = new_allocator;
        }

        ~Scratch_Allocator_Swap() {
            allocator_globals::SCRATCH = old_allocator;
        }
    };

}

#include "undefs.h"