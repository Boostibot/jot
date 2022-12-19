#pragma once

#include <concepts>
//#include <memory>
#include "utils.h"
#include "defines.h"

namespace jot
{
    enum class Alloc_State
    {
        OK,
        ERROR,
        OUT_OF_MEM,
        INVALID_ARGS,
        UNSUPPORTED_ACTION,
        UNINIT,
    };

    func has(Alloc_State state) noexcept -> bool {
        return state == Alloc_State::OK;
    }

    template <typename T>
    struct Generic_Alloc_Result
    {
        Alloc_State state = Alloc_State::UNINIT;
        Slice<T> slice;
    };

    using Alloc_Result = Generic_Alloc_Result<u8>;

    struct Alloc_Info
    {
        tsize byte_size;
        tsize align;
    };

    enum class Alloc_Action : u32 {};
    namespace Alloc_Actions
    {
        constexpr Alloc_Action ALLOCATE = cast(Alloc_Action) 0;
        constexpr Alloc_Action DEALLOCATE = cast(Alloc_Action) 1;
        constexpr Alloc_Action IS_EQUAL = cast(Alloc_Action) 2;
        constexpr Alloc_Action RESIZE = cast(Alloc_Action) 4;
        constexpr Alloc_Action DEALLOCATE_ALL = cast(Alloc_Action) 5;
        constexpr Alloc_Action RELEASE_EXTRA_MEMORY = cast(Alloc_Action) 6;
    }

    template <typename Alloc>
    concept allocator = requires(Alloc_Action action_type, 
        Alloc* alloc, 
        Option<Alloc*> other_alloc, 
        Slice<u8> prev, 
        Alloc_Info new_, 
        Alloc_Info old_, 
        Option<void*> custom_data)
    {
        { alloc->allocate(new_) } -> std::convertible_to<Alloc_Result>;
        { alloc->deallocate(prev, old_) } -> std::convertible_to<bool>;
        { alloc->is_alloc_equal(*alloc) } -> std::convertible_to<bool>;
        //{ alloc->action(action_type, other_alloc, prev, new_, old_, custom_data) } -> std::convertible_to<Alloc_Result>;
        true;
    };

    template <typename T>
    static constexpr tsize DEF_ALIGNMENT = max(
        alignof(std::max_align_t), 
        alignof(std::conditional_t<same<T, void>, u8, T>)
    );

    struct Allocator_Resource
    {
        virtual proc do_allocate(Alloc_Info info) noexcept -> Alloc_Result = 0; 
        virtual proc do_deallocate(Slice<u8> old_res, Alloc_Info old_info) noexcept -> bool = 0; 
        virtual proc do_is_alloc_equal(Allocator_Resource const& other) const noexcept -> bool = 0; 
        virtual proc do_parent_resource() const noexcept -> Allocator_Resource* = 0; 
        virtual proc do_action(
            Alloc_Action action_type, 
            Option<Allocator_Resource*> other_alloc, 
            Slice<u8> prev, 
            Alloc_Info new_, 
            Alloc_Info old_, 
            Option<void*> custom_data) noexcept -> Alloc_Result
        {
            return {Alloc_State::UNSUPPORTED_ACTION};
        }
    };

    template<typename T>
    func make_alloc_info(tsize element_size, tsize align = DEF_ALIGNMENT<T>) -> Alloc_Info {
        return Alloc_Info{element_size * cast(tsize) sizeof(T), align};
    }

    func is_power_of_two(tsize num) noexcept -> bool {
        usize n = cast(usize) num;
        return (n>0 && ((n & (n-1)) == 0));
    }

    //Fails on every allocation/deallocation
    struct Failing_Resource : Allocator_Resource
    {
        proc do_allocate(Alloc_Info info) noexcept -> Alloc_Result override {
            return {Alloc_State::ERROR};
        }
        proc do_deallocate(Slice<u8> old_res, Alloc_Info old_info) noexcept -> bool override {
            return false;
        }
        proc do_is_alloc_equal(Allocator_Resource const& other) const noexcept -> bool override {
            return dynamic_cast<const Failing_Resource*>(&other) != nullptr;
        }
        proc do_parent_resource() const noexcept -> Allocator_Resource* override {
            return nullptr;
        }
    };

    //Acts as regular c++ new delete
    struct New_Delete_Resource : Allocator_Resource
    {
        runtime_proc do_allocate(Alloc_Info info) noexcept -> Alloc_Result override 
        {
            try {
                let align = std::align_val_t{cast(size_t) info.align};
                u8* ptr = cast(u8*) ::operator new (info.byte_size, align);
                return {Alloc_State::OK, {ptr, info.byte_size}};
            }
            catch(...) {
                return {Alloc_State::ERROR};
            }
        }
        runtime_proc do_deallocate(Slice<u8> old_res, Alloc_Info old_info) noexcept -> bool override 
        {
            let mem = old_res;
            let align = std::align_val_t{cast(size_t) old_info.align};

            try {
                ::operator delete(mem.data, mem.size, align);
                return true;
            }
            catch(...) {
                return false;
            }
        };
        runtime_proc do_is_alloc_equal(Allocator_Resource const& other) const noexcept -> bool override {
            return dynamic_cast<const New_Delete_Resource*>(&other) != nullptr;
        };
        proc do_parent_resource() const noexcept -> Allocator_Resource* override {
            return nullptr;
        }; 
    };

    static New_Delete_Resource GLOBAL_NEW_DELETE_RESOURCE;
    static Failing_Resource    GLOBAL_FAILING_RESOURCE;
    static Allocator_Resource* DEFAULT_RESOURCE = &GLOBAL_NEW_DELETE_RESOURCE;

    struct Poly_Allocator
    {
        Allocator_Resource* resource = DEFAULT_RESOURCE;

        constexpr bool operator ==(Poly_Allocator const& other) const { return resource->do_is_alloc_equal(*other.resource); }
        constexpr bool operator !=(Poly_Allocator const& other) const = default;

        func allocate(Alloc_Info info) noexcept -> Alloc_Result { 
            assert(this->resource != nullptr);
            return this->resource->do_allocate(info);
        }

        func deallocate(Slice<u8> old_res, Alloc_Info old_info) noexcept -> bool {
            assert(this->resource != nullptr);
            return this->resource->do_deallocate(old_res, old_info);
        }

        func is_alloc_equal(Poly_Allocator const& other) const noexcept -> bool {
            assert(this->resource != nullptr);
            return this->resource->do_is_alloc_equal(*other.resource);
        }

        func action(Alloc_Action action_type,
            Option<Poly_Allocator*> other_alloc, 
            Slice<u8> prev, 
            Alloc_Info new_, 
            Alloc_Info old_, 
            Option<void*> custom_data) noexcept -> Alloc_Result
        {
            assert(this->resource != nullptr);

            Option<Allocator_Resource*> other_resource = has(other_alloc) ? value(other_alloc)->resource : nullptr;
            return this->resource->do_action(action_type, other_resource, prev, new_, old_, custom_data);
        }
    };

    static_assert(allocator<Poly_Allocator>);

    struct Failing_Allocator 
    {
        func allocate(Alloc_Info info) noexcept -> Alloc_Result { 
            return {Alloc_State::ERROR};
        }

        func deallocate(Slice<u8> old_res, Alloc_Info old_info) noexcept -> bool {
            return false;
        }

        func is_alloc_equal(Failing_Allocator const& other) const noexcept -> bool {
            return true;
        }

        func action(Alloc_Action action_type,
            Option<Failing_Allocator*> other_alloc, 
            Slice<u8> prev, 
            Alloc_Info new_, 
            Alloc_Info old_, 
            Option<void*> custom_data) noexcept -> Alloc_Result
        {
            return {Alloc_State::UNSUPPORTED_ACTION};;
        }
    };

    static_assert(allocator<Failing_Allocator>);

    template <typename To, typename From>
    runtime_func cast_alloc_result(Generic_Alloc_Result<From> const& from) -> Generic_Alloc_Result<To>
    {
        mut out = Generic_Alloc_Result<To>{};
        out.slice = cast_slice<To>(from.slice);
        out.state = from.state;

        return out;
    }

    template <typename T, typename Allocator>
    struct Comptime_Allocator
    {
        Allocator* alloc;
        constexpr Comptime_Allocator(Allocator* alloc)
            : alloc(alloc) {}
        
        func allocate(Alloc_Info info) noexcept -> Generic_Alloc_Result<T> { 
            if(is_const_eval())
            {
                std::allocator<T> comp_alloc;
                if(info.byte_size % sizeof(T) != 0)
                    return {Alloc_State::ERROR};

                tsize alloced_size = info.byte_size / sizeof(T);
                mut ptr = comp_alloc.allocate(alloced_size);
                return {Alloc_State::OK, Slice<T>{ptr, alloced_size}};
            }
            else
            {
                let res = alloc->allocate(info);
                return cast_alloc_result<T>(res);
            }
        }

        func deallocate(Slice<T> old_res, Alloc_Info old_info) noexcept -> bool {
            if(is_const_eval())
            {
                std::allocator<T> comp_alloc;
                comp_alloc.deallocate(old_res.data, old_res.size);
                return true;
            }
            else
            {
                let cast_old_res = cast_slice<u8>(old_res);
                return alloc->deallocate(cast_old_res, old_info);
            }
        }

        func is_alloc_equal(Allocator const& other) const noexcept -> bool {
            if(is_const_eval())
                return true;
            else
                return alloc->is_alloc_equal(other);
        }

        func action(Alloc_Action action_type,
            Option<Allocator*> other_alloc, 
            Slice<T> prev, 
            Alloc_Info new_, 
            Alloc_Info old_, 
            Option<void*> custom_data) noexcept -> Generic_Alloc_Result<T>
        {
            if(is_const_eval())
                return {Alloc_State::UNSUPPORTED_ACTION};
            else
            {
                let cast_prev = cast_slice<u8>(prev);
                let res = alloc->action(action_type, other_alloc, cast_prev, new_, old_, custom_data);
                return cast_alloc_result<T>(res);
            }
        }
    };

    func new_delete_resource() noexcept -> Allocator_Resource*      { return &GLOBAL_NEW_DELETE_RESOURCE; }
    func failing_resource() noexcept -> Allocator_Resource*         { return &GLOBAL_FAILING_RESOURCE; }

    //Upon construction exchnages the DEFAULT_RESOURCE to the provided resource
    // and upon destruction restores original value of DEFAULT_RESOURCE
    //Does safely compose
    struct Resource_Swap
    {
        Allocator_Resource* new_resource;
        Allocator_Resource* old_resource;

        Resource_Swap(Allocator_Resource* resource) : new_resource(resource), old_resource(DEFAULT_RESOURCE) {
            DEFAULT_RESOURCE = new_resource;
        }

        ~Resource_Swap() {
            DEFAULT_RESOURCE = old_resource;
        }
    };


    template<typename T>
    runtime_func is_in_slice(T* ptr, Slice<T> slice) -> bool
    {
        return ptr >= slice.data && ptr < slice.data + slice.size;
    }

    func align_forward(uintptr_t ptr_num, tsize align_to) -> uintptr_t
    {
        uintptr_t align_to_ = align_to;
        return div_round_up(ptr_num, align_to_) * align_to_;
    }

    func align_forward(Slice<u8> space, tsize align_to) -> Slice<u8>
    {
        uintptr_t ptr_num = cast(uintptr_t) space.data;
        uintptr_t aligned_num = align_forward(ptr_num, align_to);
        uintptr_t offset = min(aligned_num - ptr_num, space.size);

        return slice(space, offset);
    }

    struct Arena_Resource : Allocator_Resource
    {
        Slice<u8> buffer;
        tsize filled_to = 0;
        tsize last_alloc = 0;
        tsize max_used = 0;
        tsize max_single_alloc = 0;

        func available_slice() const -> Slice<u8> {
            return slice(buffer, filled_to);
        }

        func used_slice() const -> Slice<u8> {
            return trim(buffer, filled_to);
        }

        func last_alloced_slice() const -> Slice<u8> {
            return slice_range(buffer, last_alloc, filled_to);
        }

        proc do_allocate(Alloc_Info info) noexcept -> Alloc_Result override {
            assert(is_power_of_two(info.align));
            assert(filled_to >= 0 && last_alloc >= 0);
            Slice<u8> available = available_slice();
            Slice<u8> aligned = align_forward(available, info.align);

            if(aligned.size < info.byte_size)
                return Alloc_Result{Alloc_State::OUT_OF_MEM};

            Slice<u8> alloced = trim(aligned, info.byte_size);
            last_alloc = filled_to;
             
            tsize total_alloced_bytes = alloced.data + alloced.size - available.data;
            filled_to += total_alloced_bytes;

            #ifndef SKIP_ALLOCATOR_STATS
            max_used = max(filled_to, max_used);
            max_single_alloc = max(total_alloced_bytes, max_single_alloc);
            #endif

            return Alloc_Result{Alloc_State::OK, alloced};
        }

        proc do_deallocate(Slice<u8> old, Alloc_Info info) noexcept -> bool override {
            if(old == last_alloced_slice())  
                filled_to = last_alloc;

            return true;
        }

        constexpr bool operator ==(Arena_Resource const&) const noexcept = default;

        proc do_is_alloc_equal(Allocator_Resource const& other) const noexcept -> bool override {
            const Arena_Resource* casted = dynamic_cast<const Arena_Resource*>(&other);
            if(casted == nullptr)
                return false;

            return this->buffer == casted->buffer
                && this->filled_to == casted->filled_to 
                && this->last_alloc == casted->last_alloc;
        }

        proc do_parent_resource() const noexcept -> Allocator_Resource* override {
            return nullptr;
        }; 

        func do_action(
            Alloc_Action action_type, 
            Option<Allocator_Resource*> other_alloc, 
            Slice<u8> prev, 
            Alloc_Info new_, 
            Alloc_Info old_, 
            Option<void*> custom_data) noexcept -> Alloc_Result override
        {
            using namespace Alloc_Actions;
            if(action_type == RESIZE)
            {
                Slice<u8> last_slice = last_alloced_slice();
                if(prev != last_slice 
                    || new_.align != old_.align
                    || new_.byte_size < 0)
                    return Alloc_Result{Alloc_State::INVALID_ARGS};

                tsize new_filled_to = last_alloc + new_.byte_size;
                if(new_filled_to > buffer.size)
                    return Alloc_Result{Alloc_State::OUT_OF_MEM};

                filled_to = new_filled_to;
                return Alloc_Result{Alloc_State::OK, last_alloced_slice()};
            }

            if(action_type == DEALLOCATE_ALL)
            {
                filled_to = 0;
                last_alloc = 0;
                return Alloc_Result{Alloc_State::OK, {}};
            }

            return Alloc_Result{Alloc_State::UNSUPPORTED_ACTION};
        }
    };

}

#include "undefs.h"