#pragma once

#include "utils.h"
#include <memory_resource>

#include "defines.h"

namespace jot
{
    namespace Allocator_Actions
    {
        template <typename T>
        struct Result
        {
            bool action_exists = false;
            T* ptr = nullptr;
        };

        enum Action : u32 {};

        constexpr Action DEALLOC_ALL = cast(Action) 1;
        constexpr Action GROW = cast(Action) 2;
        constexpr Action SHRINK = cast(Action) 3;
    }

    template <typename Resource>
    concept allocator = requires(Resource res, 
        Allocator_Actions::Action action_type, 
        void* old_ptr, 
        size_t old_size, size_t new_size, 
        size_t old_align, size_t new_align, 
        void* custom_data)
    {
        { allocate<int>(&res, new_size, new_align) } -> std::convertible_to<int*>;
        deallocate<int>(&res, old_ptr, old_size, old_align);
        { action<int>(&res, action_type, old_ptr, old_size, new_size, old_align, new_align, custom_data) } -> std::convertible_to<typename Allocator_Actions::Result<int>>;
    };

    //global defaults
    template <typename T>
    static constexpr size_t DEF_ALIGNMENT = max(
        alignof(std::max_align_t), 
        alignof(std::conditional_t<are_same_v<T, void>, byte, T>)
    );

    template <typename T, allocator Alloc>
    proc allocate(Alloc* alloc, size_t size) -> T* 
    {
        return allocate<T>(alloc, size, DEF_ALIGNMENT<T>);
    }

    template <typename T, allocator Alloc>
    proc deallocate(Alloc* alloc, T* ptr, size_t size) -> void 
    {
        return deallocate<T>(alloc, ptr, size, DEF_ALIGNMENT<T>);
    }

    template <typename T, typename Alloc>
    proc action(Alloc* alloc, 
        Allocator_Actions::Action action_type, 
        void* old_ptr, 
        size_t old_size, size_t new_size, 
        size_t old_align, size_t new_align, 
        void* custom_data = nullptr) -> Allocator_Actions::Result<T>
    {
        return Allocator_Actions::Result{false, nullptr};
    }

    //STD allocator
    template <typename Alloc>   
    concept std_allocator = requires(Alloc alloc, size_t size)
    {
        { alloc.allocate(size) } -> std::convertible_to<void*>;
        alloc.deallocate(nullptr, size);

        typename Alloc::value_type;
    };

    template <typename To, typename From>
    func maybe_unsafe_ptr_cast(From* from)
    {
        if constexpr (std::convertible_to<From*, To*>)
            return cast(To*) from;
        else
            return cast(To*) cast(void*) from;
    }

    template <typename T, std_allocator Alloc>
    proc allocate(Alloc* alloc, size_t size, size_t align) -> T* 
    {
        using value_type = typename Alloc::value_type;
        let recomputed_size = div_round_up(size * sizeof(T), sizeof(value_type));
        return maybe_unsafe_ptr_cast<T>(alloc->allocate(size));
    }

    template <typename T, std_allocator Alloc>
    proc deallocate(Alloc* alloc, T* ptr, size_t size, size_t align) -> void 
    {
        using value_type = typename Alloc::value_type;
        let recomputed_size = div_round_up(size * sizeof(T), sizeof(value_type));
        return alloc->deallocate(maybe_unsafe_ptr_cast<value_type>(ptr), recomputed_size);
    }
}

#include "undefs.h"