#include "utils.h"
#include <memory_resource>

#include "defines.h"

namespace jot
{


    namespace Allocator_Actions
    {
        struct Result
        {
            bool action_exists = false;
            void* ptr = nullptr;
        };

        enum Action : u32 {};

        constexpr Action DEALLOC_ALL = cast(Action) 1;
        constexpr Action GROW = cast(Action) 2;
        constexpr Action SHRINK = cast(Action) 3;
    }

    template <typename Alloc>
    proc action(Alloc* alloc, 
        Allocator_Actions::Action action_type, 
        void* old_ptr, 
        size_t old_size, size_t new_size, 
        size_t old_align, size_t new_align, 
        void* custom_data = nullptr) -> Allocator_Actions::Result
    {
        return Allocator_Actions::Result{false, nullptr};
    }   


    template <typename Resource>
    concept allocator_resource = requires(Resource res, size_t size, size_t align)
    {
        { allocate(&res, size, align) } -> std::convertible_to<void*>;
        deallocate(&res, size, align);
    };

    template <typename Alloc>
    concept allocator = requires(Alloc alloc, size_t size, size_t align)
    {
        typename Alloc::value_type;
        { allocate(&alloc, size, align) } -> std::convertible_to<typename Alloc::value_type*>;
        deallocate(&alloc, size, align);
    };

    struct Allocator_Resource
    {
        virtual func allocate(size_t size, size_t align) -> void* = 0;
        virtual func deallocate(size_t size, size_t align) -> void* = 0;
        virtual func action(
            Allocator_Actions::Action action_type, 
            void* old_ptr, 
            size_t old_size, size_t new_size, 
            size_t old_align, size_t new_align, 
            void* custom_data = nullptr
       ) -> Allocator_Actions::Result = 0;
    };

    template <typename T>
    struct Allocator
    {
        Allocator_Resource* resource;

        using value_type = T;
    };

    template <typename Alloc>   
    concept std_allocator = requires(Alloc alloc, size_t size)
    {
        { alloc.allocate(size) } -> std::convertible_to<void*>;
        alloc.deallocate(size, nullptr);

        typename Alloc::value_type;
    };

    template <typename Alloc>
    concept simple_allocator = requires(Alloc alloc)
    {
        *alloc.resource;
        alloc.resource = nullptr;

        typename Alloc::value_type;
    };


    template <typename T>
    func get_standard_alignment() -> size_t
    {
        return max(alignof(std::max_align_t), alignof(T)));
    }

    template <allocator Alloc, typename T = Alloc::value_type>
    func allocate(Alloc* alloc, size_t size) -> T* 
    {
        return allocate(alloc, size, get_standard_alignment<T>();
    }

    template <allocator Alloc, typename T = Alloc::value_type>
    func deallocate(Alloc* alloc, size_t size) -> T* 
    {
        return deallocate(alloc, size, get_standard_alignment<T>();
    }

    //STD allocator
    template <std_allocator Alloc, typename T = Alloc::value_type>
    func allocate(Alloc* alloc, size_t size, size_t align) -> T* 
    {
        return alloc->allocate(size);
    }

    template <std_allocator Alloc, typename T = Alloc::value_type>
    proc deallocate(Alloc* alloc, T* ptr, size_t size, size_t align) -> void
    {
        return alloc->deallocate(ptr, size);
    }

    //simple allocator
    proc action(Allocator* alloc, 
        Allocator_Actions::Action action_type, 
        void* old_ptr, 
        size_t old_size, size_t new_size, 
        size_t old_align, size_t new_align, 
        void* custom_data = nullptr) -> Allocator_Actions::Result
    {
        return action(&alloc->resource, action_type, old_ptr, old_size, new_size, old_align, new_align, custom_data);
    }   
}

#include "undefs.h"