#pragma once

#include <memory_resource>
#include "allocator.h"
#include "defines.h"

namespace jot
{
    struct Allocator_Resource : std::pmr::memory_resource
    {
        using Result = Allocator_Actions::Result<void>;

        virtual proc do_allocate(size_t bytes, size_t alignment) -> void*  = 0;
        virtual proc do_deallocate(void* old_ptr, size_t bytes, size_t alignment) -> void = 0;
        virtual func do_is_equal(std::pmr::memory_resource const& other) const noexcept -> bool = 0;
        virtual proc do_upstream_resource() const noexcept -> Allocator_Resource* = 0;
        virtual proc do_action(
            Allocator_Actions::Action action_type, 
            void* old_ptr, 
            size_t old_size, size_t new_size, 
            size_t old_align, size_t new_align, 
            void* custom_data) -> Result 
        {
            return Result{false, nullptr};
        }
    };

    template <class Resource>
    concept allocator_resource = std::derived_from<Resource, Allocator_Resource>;

    func is_power_of_two(std::integral auto n) noexcept -> bool
    {
        return (n>0 && ((n & (n-1)) == 0));
    }

    template <typename T>
    func allocate(Allocator_Resource* resource, size_t byte_size, size_t align) -> T*
    {
        if(std::is_constant_evaluated())
        {
            if constexpr(same<T, void>)
                throw std::exception("void cannot be constexpr allocated");
            else
            {
                std::allocator<T> alloc;
                return cast(T*) alloc.allocate(div_round_up(byte_size, sizeof(T)));
            }
        }
        else
        {
            assert(is_power_of_two(align));
            return cast(T*) resource->do_allocate(byte_size, align);
        }
    }

    template <typename T>
    func deallocate(Allocator_Resource* resource, T* old_ptr, size_t old_size, size_t align) -> void 
    {
        if(std::is_constant_evaluated())
        {
            if constexpr (same<T, void>)
                throw std::exception("void cannot be constexpr deallocated");
            else
            {
                std::allocator<T> alloc;
                return alloc.deallocate( old_ptr, div_round_up(old_size, sizeof(T)));
            }
        }
        else
        {
            assert(is_power_of_two(align));
            return resource->do_deallocate(old_ptr, old_size, align);
        }
    }

    template <typename T>
    func action(Allocator_Resource* resource, 
        Allocator_Actions::Action action_type, 
        void* old_ptr, 
        size_t old_size, size_t new_size, 
        size_t old_align, size_t new_align, 
        void* custom_data) -> Allocator_Actions::Result<T>
    {
        if(std::is_constant_evaluated())
            return Allocator_Actions::Result<T>{false, nullptr};
        else
        {
            assert(is_power_of_two(old_align) && is_power_of_two(new_align));
            let res = resource->do_action(action_type, old_ptr, old_size, new_size, old_align, new_align, custom_data);
            return Allocator_Actions::Result<T>{res.action_exists, cast(T*) res.ptr};
        }
    }

    func upstream_allocator(Allocator_Resource const& resource) -> Allocator_Resource*
    {
        return resource.do_upstream_resource();
    }

    //Allocator_Resource that throws std::bad_alloc on each allocation
    struct Null_Resource : Allocator_Resource
    {
        constexpr Null_Resource() = default;

        runtime_proc do_allocate(size_t byte_size, size_t align) -> void* override                      { throw std::bad_alloc(); }
        runtime_proc do_deallocate(void* old_ptr, size_t byte_size, size_t align) -> void override      { throw std::bad_alloc(); }

        //has no upstream and no sate
        func do_upstream_resource() const noexcept -> Allocator_Resource* override                      { return nullptr;}
        func do_is_equal(std::pmr::memory_resource const& other) const noexcept -> bool override        { return true; }

        constexpr bool operator ==(Null_Resource const& other) const { return this->do_is_equal(* cast(std::pmr::memory_resource*) &other); }
        constexpr bool operator !=(Null_Resource const& other) const = default;
    };

    //Allocator_Resource that acts as regular c++ new delete
    struct New_Delete_Resource : Allocator_Resource
    {
        constexpr New_Delete_Resource() = default;

        runtime_proc do_allocate(size_t byte_size, size_t align) -> void* override
        { return ::operator new (byte_size, std::align_val_t{align});}

        runtime_proc do_deallocate(void* old_ptr, size_t byte_size, size_t align) -> void override
        { ::operator delete(old_ptr, byte_size, std::align_val_t{align});}

        //has no upstream and no sate
        func do_upstream_resource() const noexcept -> Allocator_Resource* override
        { return nullptr;}

        func do_is_equal(std::pmr::memory_resource const& other) const noexcept -> bool override
        { return true; }

        constexpr bool operator ==(New_Delete_Resource const& other) const { return this->do_is_equal(* cast(std::pmr::memory_resource*) &other); }
        constexpr bool operator !=(New_Delete_Resource const& other) const = default;
    };


    static New_Delete_Resource GLOBAL_NEW_DELETE_RESOURCE;
    static Null_Resource       GLOBAL_NULL_RESOURCE;
    static Allocator_Resource* DEFAULT_RESOURCE = &GLOBAL_NEW_DELETE_RESOURCE;
    

    template <typename T>
    struct Allocator_
    {
        Allocator_Resource* resource = DEFAULT_RESOURCE;
        using value_type = T;

        constexpr Allocator_() noexcept = default;
        constexpr Allocator_(Allocator_Resource* resource) 
            : resource(resource) {};

        func allocate(size_t size, size_t align = STANDARD_ALIGNMENT<T>) -> T*
            { return allocate(resource, size, align); }

        proc deallocate(T* old_ptr, size_t old_size, size_t align = STANDARD_ALIGNMENT<T>) -> void 
            { return deallocate(resource, old_ptr, old_size, align); }

        constexpr bool operator ==(Allocator_ const& other) const 
            { return resource->do_is_equal(* cast(std::pmr::memory_resource*) &other); }
        constexpr bool operator !=(Allocator_ const& other) const = default;

    };

    using Allocator = Allocator_<byte>;

    template <typename T, typename Def>
    func allocate(Allocator_<Def>* alloc, size_t size, size_t align) -> T*
        { return allocate<T>(alloc->resource, size, align);}

    template <typename T, typename Def>
    func deallocate(Allocator_<Def>* alloc, T* old_ptr, size_t old_size, size_t align) -> void
        { return deallocate<T>(alloc->resource, old_ptr, old_size, align);}

    template <typename T, typename Def>
    func action(Allocator_<Def>* alloc, 
        Allocator_Actions::Action action_type, 
        void* old_ptr, 
        size_t old_size, size_t new_size, 
        size_t old_align, size_t new_align, 
        void* custom_data) -> Allocator_Actions::Result<T>
    {
        return action<T>(alloc->resource, action_type, old_ptr, old_size, new_size, old_align, new_align, custom_data);
    }

    template<typename Def>
    func upstream_allocator(Allocator_<Def> const& alloc) -> Allocator_<Def>
    {
        return Allocator_<Def>{upstream_allocator(alloc.resource)};
    }

    static func new_delete_resource() noexcept -> Allocator_Resource*   { return &GLOBAL_NEW_DELETE_RESOURCE; }
    static func null_resource() noexcept -> Allocator_Resource*         { return &GLOBAL_NULL_RESOURCE; }

    static func new_delete_allocator() noexcept -> Allocator            { return Allocator{&GLOBAL_NEW_DELETE_RESOURCE}; }
    static func null_allocator() noexcept -> Allocator                  { return Allocator{&GLOBAL_NULL_RESOURCE}; }


    //Upon construction exchnages the DEFAULT_RESOURCE to the provided resource
    // and upon destruction restores original value of DEFAULT_RESOURCE
    //Does safely compose
    struct Resource_Swap
    {
        Allocator_Resource* new_resource;
        Allocator_Resource* old_resource;

        template <allocator_resource Resource>
        Resource_Swap(Resource* resource)
            : new_resource(cast(Allocator_Resource*) resource), old_resource(DEFAULT_RESOURCE)
        {
            DEFAULT_RESOURCE = new_resource;
        }

        ~Resource_Swap()
        {
            DEFAULT_RESOURCE = old_resource;
        }
    };
}

#include "undefs.h"