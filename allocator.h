#pragma once

#include <concepts>
#include <memory>
#include "open_enum.h"
#include "option.h"
#include "utils.h"
#include "slice.h"
#include "defines.h"

#define assert_arg(arg) assert(arg)

#define DO_ALLOCATOR_STATS
#define DO_DEALLOC_VALIDTY_CHECKS
#define DO_SNAPSHOT_VALIDTY_CHECKS

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
    enum class Allocator_Snapshot : isize {};

    template<>
    struct Hasable<Allocator_State_Type>
    {
        static constexpr func perform(Allocator_State_Type state) noexcept -> bool {
            return state == Allocator_State::OK;
        }
    };

    struct Allocation_Result
    {
        Allocator_State_Type state = Allocator_State::OK;
        Slice<u8> items;
    };

    struct Snapshot_Result
    {
        Allocator_State_Type state = Allocator_State::OK;
        Allocator_Snapshot snapshot;
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

    //todo: think about which guarantees we need: 
    //  namely should deallocate be allowed to fail - If we want to check validity we should check it within the allocator not the calling code!

    template <typename T>
    static constexpr isize DEF_ALIGNMENT = max(alignof(T), alignof(std::max_align_t));

    struct Allocator
    {
        virtual proc allocate(isize size, isize align) noexcept -> Allocation_Result = 0; 
        
        virtual proc deallocate(Slice<u8> allocated, isize align) noexcept -> void = 0; 

        virtual proc resize(Slice<u8> allocated, isize new_size) noexcept -> Allocation_Result = 0; 
        
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


    struct Scratch_Allocator : Allocator
    {
        virtual proc snapshot() noexcept -> Snapshot_Result = 0;

        virtual proc reset(Allocator_Snapshot snapshot) noexcept -> void = 0;
    };

    //Fails on every allocation/deallocation
    struct Failing_Allocator : Scratch_Allocator
    {
        virtual proc allocate(isize size, isize align) noexcept -> Allocation_Result override {
            assert_arg(size >= 0 && align >= 0);
            return {Allocator_State::UNSUPPORTED_ACTION};
        }

        virtual proc deallocate(Slice<u8> allocated, isize align) noexcept -> void override {
        }

        virtual proc resize(Slice<u8> allocated, isize new_size) noexcept -> Allocation_Result override {
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

        virtual proc snapshot() noexcept -> Snapshot_Result override {
            return Snapshot_Result{Allocator_State::OK, {}};
        }

        virtual proc reset(Allocator_Snapshot) noexcept -> void override {};
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

        virtual proc deallocate(Slice<u8> allocated, isize align) noexcept -> void override {
            operator delete(allocated.data, std::align_val_t{cast(size_t) align}, std::nothrow_t{});

            #ifdef DO_ALLOCATOR_STATS
                total_alloced -= allocated.size;
            #endif // do_memory_stats
        } 

        virtual proc resize(Slice<u8> allocated, isize new_size) noexcept -> Allocation_Result override {
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
    constexpr func is_in_slice(T* ptr, Slice<T> slice) -> bool
    {
        return ptr >= slice.data && ptr < slice.data + slice.size;
    }

    constexpr func align_forward(usize ptr_num, isize align_to) -> usize
    {
        assert_arg(is_power_of_two(align_to));

        usize mask = (align_to - 1);
        usize mod = ptr_num & mask;
        if(mod == 0)
            return ptr_num;

        return ptr_num + (align_to - mod);
    }
    
    constexpr func align_forward(u8* ptr, isize align_to) -> u8*
    {
        return cast(u8*) align_forward(cast(usize) ptr, align_to);
    }
    
    constexpr func ptrdiff(void* ptr1, void* ptr2) -> isize
    {
        return cast(isize) ptr1 - cast(isize) ptr2;
    }

    constexpr func align_forward(Slice<u8> space, isize align_to) -> Slice<u8>
    {
        usize ptr_num = cast(usize) space.data;
        usize aligned_num = align_forward(ptr_num, align_to);
        isize offset = cast(isize) min(aligned_num - ptr_num, space.size);

        return slice(space, offset);
    }

    //internal align function that might result in illegal negative sized slice
    constexpr func _align_forward_negative(Slice<u8> space, isize align_to) -> Slice<u8>
    {
        usize ptr_num = cast(usize) space.data;
        usize aligned_num = align_forward(ptr_num, align_to);
        
        return Slice<u8>{
            cast(u8*) cast(void*) aligned_num, 
            space.size - cast(isize) ptr_num + cast(isize) aligned_num
        };
    }

    struct Stack_Allocator : Scratch_Allocator
    {
        Slice<u8> buffer;
        isize filled_to = 0;
        isize last_alloc = 0;
        isize max_used = 0;
        isize alloced = 0;
        isize max_alloced = 0;

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

        virtual proc allocate(isize size, isize align) noexcept -> Allocation_Result override {
            assert(filled_to >= 0 && last_alloc >= 0);
            assert_arg(size >= 0 && align >= 0);

            Slice<u8> available = available_slice();
            Slice<u8> aligned = _align_forward_negative(available, align);

            if(aligned.size < size)
                return Allocation_Result{Allocator_State::OUT_OF_MEM};

            Slice<u8> returned_slice = trim(aligned, size);
            last_alloc = filled_to;

            isize total_alloced_bytes = returned_slice.data + returned_slice.size - available.data;
            filled_to += total_alloced_bytes;

            #ifdef DO_ALLOCATOR_STATS
            max_used = max(filled_to, max_used);
            alloced += size;
            max_alloced += max(max_alloced, alloced);
            #endif

            return Allocation_Result{Allocator_State::OK, returned_slice};
        }

        virtual proc deallocate(Slice<u8> allocated, isize align) noexcept -> void override {
            if(allocated == last_alloced_slice())  
                filled_to = last_alloc;
        } 

        virtual proc resize(Slice<u8> allocated, isize new_size) noexcept -> Allocation_Result override {
            
            Slice<u8> last_slice = last_alloced_slice();
            if(allocated != last_slice)
            {
                if(new_size < allocated.size)
                    return Allocation_Result{Allocator_State::OK, trim(allocated, new_size)};

                return Allocation_Result{Allocator_State::NOT_RESIZABLE};
            }

            isize new_filled_to = last_alloc + new_size;
            if(new_filled_to > buffer.size)
                return Allocation_Result{Allocator_State::OUT_OF_MEM};

            filled_to = new_filled_to;
            return Allocation_Result{Allocator_State::OK, last_slice};
        } 

        virtual proc parent_allocator() const noexcept -> Nullable<Allocator*> override {
            return {nullptr};
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

        virtual proc snapshot() noexcept -> Snapshot_Result override {
            return Snapshot_Result{Allocator_State::OK, Allocator_Snapshot{filled_to}};
        }

        virtual proc reset(Allocator_Snapshot snapshot) noexcept -> void override {
            filled_to = cast(usize) snapshot;
        };
    };

    namespace allocator_globals
    {
        static New_Delete_Allocator NEW_DELETE_ALLOCATOR;
        static Failing_Allocator    FAILING_ALLOCATOR;

        thread_local static Allocator* DEFAULT = &NEW_DELETE_ALLOCATOR;
        thread_local static Scratch_Allocator* SCRATCH = &FAILING_ALLOCATOR;

        inline Allocator* get_default() noexcept {
            return DEFAULT;
        }

        inline Scratch_Allocator* get_scratch() noexcept {
            return SCRATCH;
        }

        inline void set_default(Allocator* alloc) noexcept {
            DEFAULT = alloc;
        }

        inline void set_scratch(Scratch_Allocator* alloc) noexcept {
            SCRATCH = alloc;
        }

        //Upon construction exchnages the DEFAULT to the provided allocator
        // and upon destruction restores original value of DEFAULT
        //Does safely compose
        struct Default_Swap
        {
            Allocator* new_allocator;
            Allocator* old_allocator;

            Default_Swap(Allocator* resource, Allocator* old = allocator_globals::get_default()) : new_allocator(resource), old_allocator(old) {
                set_default(new_allocator);
            }

            ~Default_Swap() {
                set_default(old_allocator);
            }
        };

        struct Scratch_Swap
        {
            Scratch_Allocator* new_allocator;
            Scratch_Allocator* old_allocator;

            Scratch_Swap(Scratch_Allocator* resource, Scratch_Allocator* old = allocator_globals::get_scratch()) : new_allocator(resource), old_allocator(old) {
                set_scratch(new_allocator);
            }

            ~Scratch_Swap() {
                set_scratch(old_allocator);
            }
        };
    }
}

#include "undefs.h"