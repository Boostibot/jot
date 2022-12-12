#pragma once

#include <concepts>
#include <memory>
#include "types.h"
#include "slice.h"
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

    //@TODO: Owned slice - basis for vectors
    //@TODO: Maybe - with nullable for ptrs
    //@TODO: Failing allocs for stack
    //@TODO: less shity format

    template <typename T>
    struct Nullable
    {
        T* data = nullptr;
        constexpr Nullable(T* ptr) : data(ptr) {}
    };

    template <typename T>
    func has(Nullable<T> ptr) -> bool 
    {
        return ptr.data != nullptr;
    }

    template <typename T>
    func unwrap(Nullable<T> ptr) -> T* 
    {
        assert(ptr.data != nullptr);
        return ptr.data;
    }

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
        Nullable<Alloc> other_alloc, 
        Slice<u8> prev, 
        Alloc_Info new_, 
        Alloc_Info old_, 
        Nullable<void> custom_data)
    {
        //{ allocate(alloc, new_) } -> std::convertible_to<Alloc_Result>;
        //{ deallocate(alloc, prev, old_) } -> std::convertible_to<bool>;
        //{ is_alloc_equal(*alloc, *alloc) } -> std::convertible_to<bool>;
        //{ action(action_type, alloc, other_alloc, prev, new_, old_, custom_data) } -> std::convertible_to<Alloc_Result>;
        true;
    };

    template <typename T>
    static constexpr tsize DEF_ALIGNMENT = max(
        alignof(std::max_align_t), 
        alignof(std::conditional_t<same<T, void>, u8, T>)
    );

    struct Allocator_Resource
    {
        virtual proc do_allocate(Alloc_Info info) -> Alloc_Result = 0; 
        virtual proc do_deallocate(Slice<u8> old_res, Alloc_Info old_info) -> bool = 0; 
        virtual proc do_is_alloc_equal(Allocator_Resource const& other) const -> bool = 0; 
        virtual proc do_parent_resource() const -> Allocator_Resource* = 0; 
        virtual proc do_action(
            Alloc_Action action_type, 
            Nullable<Allocator_Resource> other_alloc, 
            Slice<u8> prev, 
            Alloc_Info new_, 
            Alloc_Info old_, 
            Nullable<void> custom_data) -> Alloc_Result
        {
            return {Alloc_State::UNSUPPORTED_ACTION};
        }
    };

    template<typename T>
    func make_def_alloc_info(tsize element_size) -> Alloc_Info
    {
        return Alloc_Info{element_size * cast(tsize) sizeof(T), DEF_ALIGNMENT<T>};
    }

    func is_power_of_two(tsize num) noexcept -> bool
    {
        usize n = cast(usize) num;
        return (n>0 && ((n & (n-1)) == 0));
    }


    func allocate(Allocator_Resource* resource, Alloc_Info info) -> Alloc_Result { 
        return resource->do_allocate(info);
    }

    func deallocate(Allocator_Resource* resource, Slice<u8> old_res, Alloc_Info old_info) -> bool {
        return resource->do_deallocate(old_res, old_info);
    }

    func is_alloc_equal(Allocator_Resource const& alloc, Allocator_Resource const& other) -> bool {
        return alloc.do_is_alloc_equal(other);
    }

    func parent_resource(Allocator_Resource const& resource) -> Allocator_Resource* {
        return resource.do_parent_resource();
    }

    func action(Alloc_Action action_type,
        Allocator_Resource* resource,  
        Nullable<Allocator_Resource> other_alloc, 
        Slice<u8> prev, 
        Alloc_Info new_, 
        Alloc_Info old_, 
        Nullable<void> custom_data) -> Alloc_Result
    {
        return resource->do_action(action_type, other_alloc, prev, new_, old_, custom_data);
    }


    //Fails on every allocation/deallocation
    struct Failing_Resource : Allocator_Resource
    {
        runtime_proc do_allocate(Alloc_Info info) -> Alloc_Result override {
            return {Alloc_State::ERROR};
        }
        runtime_proc do_deallocate(Slice<u8> old_res, Alloc_Info old_info) -> bool override {
            return false;
        };
        runtime_proc do_is_alloc_equal(Allocator_Resource const& other) const -> bool override {
            return dynamic_cast<const Failing_Resource*>(&other) != nullptr;
        };
        runtime_proc do_parent_resource() const -> Allocator_Resource* override {
            return nullptr;
        }; 
    };

    //Acts as regular c++ new delete
    struct New_Delete_Resource : Allocator_Resource
    {
        runtime_proc do_allocate(Alloc_Info info) -> Alloc_Result override {

            try {
                let align = std::align_val_t{cast(size_t) info.align};
                u8* ptr = cast(u8*) ::operator new (info.byte_size, align);
                return {Alloc_State::OK, {ptr, info.byte_size}};
            }
            catch(...) {
                return {Alloc_State::ERROR};
            }
        }
        runtime_proc do_deallocate(Slice<u8> old_res, Alloc_Info old_info) -> bool override {

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
        runtime_proc do_is_alloc_equal(Allocator_Resource const& other) const -> bool override {
            return dynamic_cast<const New_Delete_Resource*>(&other) != nullptr;
        };
        runtime_proc do_parent_resource() const -> Allocator_Resource* override {
            return nullptr;
        }; 
    };

    static_assert(allocator<Allocator_Resource>);
    static_assert(allocator<Failing_Resource>);
    static_assert(allocator<New_Delete_Resource>);

    static New_Delete_Resource GLOBAL_NEW_DELETE_RESOURCE;
    static Failing_Resource    GLOBAL_FAILING_RESOURCE;
    static Allocator_Resource* DEFAULT_RESOURCE = &GLOBAL_NEW_DELETE_RESOURCE;

    struct Poly_Allocator
    {
        Allocator_Resource* resource = DEFAULT_RESOURCE;

        constexpr bool operator ==(Poly_Allocator const& other) const { return resource->do_is_alloc_equal(*other.resource); }
        constexpr bool operator !=(Poly_Allocator const& other) const = default;
    };

    func allocate(Poly_Allocator* alloc, Alloc_Info info) -> Alloc_Result { 
        return alloc->resource->do_allocate(info);
    }

    func deallocate(Poly_Allocator* alloc, Slice<u8> old_res, Alloc_Info old_info) -> bool {
        return alloc->resource->do_deallocate(old_res, old_info);
    }

    func is_alloc_equal(Poly_Allocator const& alloc, Poly_Allocator const& other) -> bool {
        return alloc == other;
    }

    func action(Alloc_Action action_type,
        Poly_Allocator* alloc,  
        Nullable<Poly_Allocator> other_alloc, 
        Slice<u8> prev, 
        Alloc_Info new_, 
        Alloc_Info old_, 
        Nullable<void> custom_data) -> Alloc_Result
    {
        assert(alloc != nullptr);
        assert(alloc->resource != nullptr);
        
        Nullable<Allocator_Resource> other_resource = has(other_alloc) ? unwrap(other_alloc)->resource : nullptr;
        return alloc->resource->do_action(action_type, other_resource, prev, new_, old_, custom_data);
    }

    static_assert(allocator<Poly_Allocator>);

    struct Failing_Allocator {};

    func allocate(Failing_Allocator* alloc, Alloc_Info info) -> Alloc_Result { 
        return {Alloc_State::ERROR};
    }

    func deallocate(Failing_Allocator* alloc, Slice<u8> old_res, Alloc_Info old_info) -> bool {
        return false;
    }

    func is_alloc_equal(Failing_Allocator const& alloc, Failing_Allocator const& other) -> bool {
        return true;
    }

    func action(Alloc_Action action_type,
        Failing_Allocator* alloc,  
        Nullable<Failing_Allocator> other_alloc, 
        Slice<u8> prev, 
        Alloc_Info new_, 
        Alloc_Info old_, 
        Nullable<void> custom_data) -> Alloc_Result
    {
        return {Alloc_State::UNSUPPORTED_ACTION};;
    }

    static_assert(allocator<Failing_Allocator>);

    template <typename To, typename From>
    runtime_func cast_alloc_result(Generic_Alloc_Result<From> const& from) -> Generic_Alloc_Result<To>
    {
        mut out = Generic_Alloc_Result<To>{};
        out.slice = cast_slice<To>(from.slice);
        out.state = from.state;

        return out;
    }

    template <typename T, typename Alloc>
    func comptime_allocate(Alloc* alloc, Alloc_Info info) -> Generic_Alloc_Result<T> 
    { 
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
            let res = allocate(alloc, info);
            return cast_alloc_result<T>(res);
        }
    }

    template <typename T, typename Alloc>
    func comptime_deallocate(Alloc* alloc, Slice<T> old_res, Alloc_Info old_info) -> bool {

        if(is_const_eval())
        {
            std::allocator<T> comp_alloc;
            comp_alloc.deallocate(old_res.data, old_res.size);
            return true;
        }
        else
        {
            let cast_old_res = cast_slice<u8>(old_res);
            return deallocate(alloc, cast_old_res, old_info);
        }
    }

    template <typename Alloc>
    func comptime_is_alloc_equal(Alloc const& alloc, Alloc const& other) -> bool {
        if(is_const_eval())
            return true;
        else
            return is_alloc_equal(alloc, other);
    }

    template <typename T, typename Alloc>
    func comptime_action(
        Alloc_Action action_type,
        Alloc* alloc,  
        Nullable<Alloc> other_alloc, 
        Slice<T> prev, 
        Alloc_Info new_, 
        Alloc_Info old_, 
        Nullable<void> custom_data) -> Generic_Alloc_Result<T>
    {
        if(is_const_eval())
            return {Alloc_State::UNSUPPORTED_ACTION};
        else
        {
            let cast_prev = cast_slice<u8>(prev);
            let res = action(action_type, alloc, other_alloc, cast_prev, new_, old_, custom_data);
            return cast_alloc_result<T>(res);
        }
    }

    
    static func new_delete_resource() noexcept -> Allocator_Resource*      { return &GLOBAL_NEW_DELETE_RESOURCE; }
    static func failing_resource() noexcept -> Allocator_Resource*         { return &GLOBAL_FAILING_RESOURCE; }

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
}


#include "undefs.h"