#pragma once

#include <cstdint>
#include <concepts>
#include <memory>
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

        enum class Action : std::uint32_t {};

        constexpr Action DEALLOC_ALL = cast(Action) 1;
        constexpr Action RESIZE = cast(Action) 2;
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

    template <typename T, std_allocator Alloc>
    proc action(Alloc* alloc, 
        Allocator_Actions::Action action_type, 
        void* old_ptr, 
        size_t old_size, size_t new_size, 
        size_t old_align, size_t new_align, 
        void* custom_data = nullptr) -> Allocator_Actions::Result<T>
    {
        return Allocator_Actions::Result<T>{false, nullptr};
    }

    template <typename Resource>
    concept allocator = requires(Resource res, 
        Allocator_Actions::Action action_type, 
        void* old_ptr, 
        size_t old_size, size_t new_size, 
        size_t old_align, size_t new_align, 
        void* custom_data)
    {
        //true; //@NOTE: MSVC is buggy and syntax highlighting breaks so we dont have all checks on per default
        { allocate<int>(&res, new_size, new_align) } -> std::convertible_to<int*>;
        //deallocate<void>(&res, old_ptr, old_size, old_align);
        //{ action<void>(&res, action_type, old_ptr, old_size, new_size, old_align, new_align, custom_data) } -> std::convertible_to<Allocator_Actions::Result<void>>;
    };

    //global defaults
    template <typename T>
    static constexpr size_t DEF_ALIGNMENT = max(
        alignof(std::max_align_t), 
        alignof(std::conditional_t<same<T, void>, std::uint8_t, T>)
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

    template <typename T, allocator Alloc>
    proc action(Alloc* alloc, 
        Allocator_Actions::Action action_type, 
        void* old_ptr, 
        size_t old_size, size_t new_size, 
        size_t old_align, size_t new_align, 
        void* custom_data = nullptr) -> Allocator_Actions::Result<T>
    {
        return Allocator_Actions::Result<T>{false, nullptr};
    }

    template <typename T, allocator Alloc>
    proc action(Alloc* alloc, 
        Allocator_Actions::Action action_type, 
        void* old_ptr, 
        size_t old_size, size_t new_size) -> Allocator_Actions::Result<T>
    {
        return action<T, Alloc>(alloc, action_type, old_ptr, old_size, new_size, DEF_ALIGNMENT<T>, DEF_ALIGNMENT<T>, nullptr);
    }


    static_assert(allocator<std::allocator<int>>);
}

#include "undefs.h"