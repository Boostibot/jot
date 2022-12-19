#pragma once

#include "allocator.h"
#include "stack.h"
#include "defines.h"

namespace jot 
{
    //no its not possible to inline this - Why? I dont know
    template<tsize alignment_>
    struct Alignment_Holder
    {
        static constexpr tsize alignment = alignment_;
    };

    template<>
    struct Alignment_Holder<-1>
    {
        tsize alignment = 0;
    };

    template<typename T, typename Allocator = Poly_Allocator, tsize alignment_ = DEF_ALIGNMENT<T>>
    struct Owned_Slice : Allocator, Alignment_Holder<alignment_>
    {
        Slice<T> items = {};

        constexpr Owned_Slice() noexcept = default;
        constexpr Owned_Slice(Allocator alloc) noexcept
            : Allocator(move(&alloc)) {};

        constexpr Owned_Slice(Owned_Slice moved other) noexcept;
        constexpr Owned_Slice(Owned_Slice in other) noexcept = delete;
        constexpr ~Owned_Slice() noexcept;
    };

    template<typename T, typename Alloc, tsize align>
    func alloc(Owned_Slice<T, Alloc, align> in owned) noexcept -> Alloc in {
        return cast(Alloc const&) owned;
    }

    template<typename T, typename Alloc, tsize align>
    func alloc(Owned_Slice<T, Alloc, align>* owned) noexcept -> Alloc* {
        return cast(Alloc*) owned;
    }

    template<typename T, typename Alloc, tsize align>
    func slice(Owned_Slice<T, Alloc, align> in owned) noexcept -> Slice<const T> {
        return owned.items;
    }

    template<typename T, typename Alloc, tsize align>
    func slice(Owned_Slice<T, Alloc, align>* owned) noexcept -> Slice<T> {
        //this is where 'to constness' is not exchnagable to constnesss
        return owned->items;
    }

    template<typename T, typename Alloc, tsize align>
    func deallocate_slice(Owned_Slice<T, Alloc, align>* slice) noexcept -> bool
    {
        bool result = true;
        if(slice->items.data != nullptr)
        {
            Comptime_Allocator<T, Alloc> allocator = alloc(slice);
            Alloc_Info info = make_alloc_info<T>(slice->items.size, slice->alignment);
            result = allocator.deallocate(slice->items, info);
            slice->items = Slice<T>{};
        }

        return result;
    }

    template<typename T, typename Alloc, tsize align>
    func release(Owned_Slice<T, Alloc, align>* owned) noexcept -> Slice<T> {
        Slice<T> released = owned.items;
        bool res = deallocate_slice(owned);
        assert(res && "deallocation must succeed");
        return released;
    }

    template<typename T, typename Alloc, tsize align>
    func swap(Owned_Slice<T, Alloc, align>* left, Owned_Slice<T, Alloc, align>* right) noexcept -> void
    {
        swap(alloc(left), alloc(right));
        swap(&left->items, &right->items);
        if constexpr(align != -1)
            swap(&left->alignment, &right->alignment);
    }

    template<typename T, typename Alloc, tsize align>
    constexpr Owned_Slice<T, Alloc, align>::~Owned_Slice() noexcept {
        bool res = deallocate_slice(this);
        assert(res && "deallocation must succeed");
    }

    template<typename T, typename Alloc, tsize align>
    constexpr Owned_Slice<T, Alloc, align>::Owned_Slice(Owned_Slice<T, Alloc, align> moved other) noexcept {
        swap(this, &other);
    }

    template<typename T, typename Alloc, tsize align>
    func allocate_slice_all_args(Owned_Slice<T, Alloc, align>* into, tsize to_size, tsize alignment) noexcept -> Alloc_State
    {
        assert(into->items.data == nullptr && into->items.size == 0 && "slice must be empty!");
        //the interface could have been made in a wayt that doesnt allow missuse but that would require
        // for c++ to be smarter and have proper support for move semnatics. Currently it would result in bad codegen.

        Comptime_Allocator<T, Alloc> allocator = alloc(into);
        const Alloc_Info info = make_alloc_info<T>(to_size, alignment);
        mut res = allocator.allocate(info);
        if(res.state == Error())
            return res.state;

        assert(res.slice.size == to && "allocator must work correctly");
        assert((res.slice.data == nullptr ? to == 0 : true) && "null data is only allowed when size is 0");

        into->items = res.slice;
        into->alignment = alignment;
        return Alloc_State::OK;
    }

    template<typename T, typename Alloc, tsize align>
    func allocate_slice(tsize to_size, tsize alignment, Alloc alloc) noexcept -> Result<Owned_Slice<T, Alloc, align>, Alloc_State>
    {
        Owned_Slice<T, Alloc, align> result = {move(&alloc)};
        Alloc_State res = allocate_slice_all_args(&result, to_size, alignment);
        if(res == Error())
            return Error(res.state);

        return Value(result);
    }

    template<typename T, typename Alloc, tsize align>
    func allocate_slice(Owned_Slice<T, Alloc, align>* into, tsize to_size) noexcept -> Alloc_State
    {
        return allocate_slice_all_args(into, to_size, align);
    }

    template<typename T, typename Alloc>
    func allocate_slice(Owned_Slice<T, Alloc, -1>* into, tsize to_size, tsize alignment) noexcept -> Alloc_State
    {
        return allocate_slice_all_args(into, to_size, alignment);
    }

    template<typename T, typename Alloc>
    func allocate_slice(tsize to_size, tsize alignment, Alloc alloc = Alloc()) noexcept -> Result<Owned_Slice<T, Alloc>, Alloc_State>
    {
        return allocate_slice<T, Alloc, -1>(to_size, alignment, move(&alloc));
    }

    template<typename T, typename Alloc, tsize align>
    func allocate_slice(tsize to_size, Alloc alloc = Alloc()) noexcept -> Result<Owned_Slice<T, Alloc>, Alloc_State>
    {
        return allocate_slice<T, Alloc, align>(to_size, align, move(&alloc));
    }

    template<typename T, typename Alloc, tsize align>
    func set_size(Owned_Slice<T, Alloc, align>* what, tsize to) noexcept -> Alloc_State
    {
        const Alloc_Info new_info = make_alloc_info<T>(to, what->alignment);
        const Alloc_Info prev_info = make_alloc_info<T>(what->items.size, what->alignment);
        Comptime_Allocator<T, Alloc> allocator = alloc(what);

        Generic_Alloc_Result<T> resize_res = allocator.action(
            Alloc_Actions::RESIZE, 
            nullptr, 
            what->items, 
            new_info, prev_info, 
            nullptr);
        if(resize_res.state == Alloc_State::OK)
        {
            assert(resize_res.slice.size == to && "allocator must work correctly");
            assert((resize_res.slice.data == nullptr ? to == 0 : true) && "null data is only allowed when size is 0");
            what->items = resize_res.slice;
        }

        return resize_res.state;
    }
}

#include "undefs.h"