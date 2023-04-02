#pragma once
#include "slice.h"
#include "memory.h"
#include "slice_ops.h"
#include "utils.h"

#include "defines.h"

namespace jot
{
    template<class T>
    struct Set_Capacity_Result
    {
        Allocator_State_Type state = Allocator_State::OK;
        Slice<T> items;
        bool adress_changed = true;
    };

    // Can be used for arbitrary growing/shrinking of data
    // When old_slice is null only allocates the data
    // When new_capacity is 0 only deallocates the data
    // Destroys elements when shrinking but does not construct new ones when growing 
    //  (because doesnt know how to)
    template<class T> nodisc
    Set_Capacity_Result<T> set_capacity(Allocator* allocator, Slice<T>* old_slice, isize filled_to, isize align, isize new_capacity, bool try_resize) noexcept;

    //Returns new size of a stack which is guaranteed to be greater than to_fit
    constexpr inline
    isize calculate_stack_growth(isize curr_size, isize to_fit, isize growth_num = 3, isize growth_den = 2, isize grow_lin = 8)
    {
        //with default values for small sizes grows fatser than classic factor of 2 for big slower
        isize size = curr_size;
        while(size < to_fit)
            size = size * growth_num / growth_den + grow_lin; 

        return size;
    }

    template<class T> nodisc
    Set_Capacity_Result<T> set_capacity_allocation_stage(Allocator* allocator, Slice<T>* old_slice, isize align, isize new_capacity, bool try_resize) noexcept
    {
        Set_Capacity_Result<T> result;
        if(new_capacity == 0)
        {
            result.state = Allocator_State::OK;
            result.items = Slice<T>();
            result.adress_changed = false;
            return result;
        }

        if(old_slice->size != 0 && try_resize)
        {
            assert(old_slice->data != nullptr);
            Slice<u8> old_data = cast_slice<u8>(*old_slice);
            Allocation_Result resize_res = allocator->resize(old_data, align, new_capacity * cast(isize) sizeof(T));
            if(resize_res.state == Allocator_State::OK)
            {
                result.state = Allocator_State::OK;
                result.items = cast_slice<T>(resize_res.items);
                result.adress_changed = false;

                assert(result.items.size >= new_capacity);
                assert(result.items.data == nullptr ? new_capacity == 0 : true);

                return result;
            }
        }
            
        Allocation_Result allocation_res = allocator->allocate(new_capacity * cast(isize) sizeof(T), align);
        result.state = allocation_res.state;
        result.items = cast_slice<T>(allocation_res.items);
        result.adress_changed = true;

        return result;
    }
        
    template<class T>
    void set_capacity_deallocation_stage(Allocator* allocator, Slice<T>* old_slice, isize filled_to, isize align, Set_Capacity_Result<T>* result) noexcept
    {
        assert(result->state == Allocator_State::OK && "must be okay!");

        T* RESTRICT new_data = result->items.data;
        T* RESTRICT old_data = old_slice->data;
            
        isize new_capacity = result->items.size;
        if(result->adress_changed)
        {
            assert((are_aliasing<T>(*old_slice, result->items) == false));

            isize copy_to = min(filled_to, new_capacity);
            if constexpr(is_byte_copyable<T>)
            {
                std::memcpy(new_data, old_data, copy_to * sizeof(T));
            }
            else
            {
                for (isize i = 0; i < copy_to; i++)
                    construct_at(new_data + i, move(old_data + i));
            }
            
            if constexpr(std::is_trivially_destructible_v<T> == false)
                for(isize i = 0; i < filled_to; i++)
                    old_data[i].~T();
        }
        else
        {
            if constexpr(std::is_trivially_destructible_v<T> == false)
                for(isize i = new_capacity; i < filled_to; i++)
                    old_data[i].~T();
        }

        if(old_slice->size != 0)
        {
            assert(old_slice->data != nullptr);
            allocator->deallocate(cast_slice<u8>(*old_slice), align);
        }
    }

    template<class T> nodisc
    Set_Capacity_Result<T> set_capacity(Allocator* allocator, Slice<T>* old_slice, isize filled_to, isize align, isize new_capacity, bool try_resize) noexcept
    {
        Set_Capacity_Result<T> result = set_capacity_allocation_stage(allocator, old_slice, align, new_capacity, try_resize);
        if(result.state != Allocator_State::OK)
            return result;

        set_capacity_deallocation_stage(allocator, old_slice, filled_to, align, &result);
        return result;
    }
    
    //fast path for destroying of stacks (instead of set_capacity which is longer)
    template<class T>
    Allocator_State_Type destroy_and_deallocate(Allocator* allocator, Slice<T>* old_slice, isize filled_to, isize align) noexcept
    {
        if constexpr(std::is_trivially_destructible_v<T> == false)
            for(isize i = 0; i < filled_to; i++)
                (*old_slice)[i].~T();

        if(old_slice->size != 0)
        {
            assert(old_slice->data != nullptr);
            return allocator->deallocate(cast_slice<u8>(*old_slice), align);
        }

        return Allocator_State::OK;
    }
}

#include "undefs.h"