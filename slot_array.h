#pragma once

#include "memory.h"
#include "slice.h"
#include "panic.h"

namespace jot
{
    template<typename T>
    struct Slot_Array
    {
        T* _data = nullptr;
        uint32_t* _slots = nullptr;
        Allocator* _allocator = default_allocator();
        uint32_t _size = 0;
        uint32_t _capacity = 0;
        uint32_t _free_list = (uint32_t) -1;
        uint32_t _max_slot = 0;
        
        Slot_Array(Allocator* allocator = default_allocator()) noexcept
            : _allocator(allocator)
        {}

        ~Slot_Array() noexcept;

        Slot_Array& operator=(Slot_Array && other) noexcept;
        Slot_Array& operator=(Slot_Array const& other);
    };
    
    ///Getters 
    template<class T> auto data(Slot_Array<T> const& array) noexcept -> const T*          { return array._data; }
    template<class T> auto data(Slot_Array<T>* array) noexcept -> T*                      { return array->_data; }
    template<class T> auto size(Slot_Array<T> const& array) noexcept -> isize             { return array._size; }
    template<class T> auto size(Slot_Array<T>* array) noexcept -> isize                   { return array->_size; }
    template<class T> auto capacity(Slot_Array<T> const& array) noexcept -> isize         { return array._capacity; }
    template<class T> auto capacity(Slot_Array<T>* array) noexcept -> isize               { return array->_capacity; }
    template<class T> auto allocator(Slot_Array<T> const& array) noexcept -> Allocator*   { return array._allocator; }
    template<class T> auto allocator(Slot_Array<T>* array) noexcept -> Allocator*         { return array->_data; }
    
    ///Iterators
    template<typename T> constexpr T*       begin(Slot_Array<T>& array) noexcept          { return array._data; }
    template<typename T> constexpr const T* begin(Slot_Array<T> const& array) noexcept    { return array._data; }
    template<typename T> constexpr T*       end(Slot_Array<T>& array) noexcept            { return array._data + array._size; }
    template<typename T> constexpr const T* end(Slot_Array<T> const& array) noexcept      { return array._data + array._size; }

    ///Returns a slice containing all items of the array
    template<class T> auto slice(Slot_Array<T> const& array) noexcept -> Slice<const T>   { return {array._data, array._size}; }
    template<class T> auto slice(Slot_Array<T>* array) noexcept -> Slice<T>               { return {array->_data, array->_size}; }

    template<class T>
    bool is_invariant(Slot_Array<T> const& array) noexcept
    {
        bool is_size_big_enough = array._capacity >= array._size;
        bool has_properly_alloced_data = (array._capacity == 0) == (array._data == nullptr);
        bool has_properly_alloced_slots = (array._capacity == 0) == (array._slots == nullptr);

        return is_size_big_enough && has_properly_alloced_data && has_properly_alloced_slots;
    }

    namespace slot_array_internal
    {
        enum 
        {
            ITEM = 0,
            OWNER = 1,
            NEXT = 2,
            
            SLOT_SIZE = 3,
        };

        template<class T>
        bool set_capacity_failing(Slot_Array<T>* array, isize new_capacity) noexcept
        {
            assert(is_invariant(*array));

            const isize SLOT_ALIGNMENT = 8;
            const isize ITEM_BYTES = (isize) sizeof(T);
            const isize SLOT_BYTES = SLOT_SIZE * (isize) sizeof(uint32_t);

            isize old_cap = array->_capacity;
            isize new_cap = new_capacity;
            Allocator* alloc = array->_allocator;
        
            T* new_data = nullptr;
            uint32_t* new_slots = nullptr;

            bool s1 = memory_resize_allocate(alloc, &new_data, new_cap*ITEM_BYTES, array->_data, old_cap*ITEM_BYTES, DEF_ALIGNMENT<T>, GET_LINE_INFO());
            bool s2 = memory_resize_allocate(alloc, &new_slots, new_cap*SLOT_BYTES, array->_slots, old_cap*SLOT_BYTES, SLOT_ALIGNMENT, GET_LINE_INFO());

            if(s1 == false || s2 == false)
            {
                memory_resize_undo(alloc, &new_data, new_cap*ITEM_BYTES, array->_data, old_cap*ITEM_BYTES, DEF_ALIGNMENT<T>, GET_LINE_INFO());
                memory_resize_undo(alloc, &new_slots, new_cap*SLOT_BYTES, array->_slots, old_cap*SLOT_BYTES, SLOT_ALIGNMENT, GET_LINE_INFO());
            }

            isize to_size = min(array->_size, new_capacity);

            assert(JOT_IS_REALLOCATABLE(T) && "we assume reallocatable!");
            if(new_data != array->_data)   memcpy(new_data, array->_data, to_size * ITEM_BYTES);
            if(new_slots != array->_slots) memcpy(new_slots, array->_slots, to_size * SLOT_BYTES);

            if(JOT_IS_TRIVIALLY_DESTRUCTIBLE(T) == false)
            {
                for(isize i = new_capacity; i < array->_size); i++)
                    array->_data[i].~T();
            }
        
            //when growing properly link the added nodes to the free list
            if(old_cap < new_cap)
            {
                for(isize i = old_cap; i < new_cap; i++)
                {
                    uint32_t* new_slot = new_slots + i * SLOT_SIZE;
                    new_slot[ITEM] = (uint32_t) -1; 
                    new_slot[OWNER] = (uint32_t) -1; 
                    new_slot[NEXT] = i + 1; 
                    new_slots + index
                }

                uint32_t* last_new_slot = new_slots + (new_cap - 1) * SLOT_SIZE;
                last_new_slot[NEXT] = (uint32_t) array->_free_list;
                array->_free_list = old_cap;
            }

            memory_resize_deallocate(alloc, &new_data, new_cap*ITEM_BYTES, array->_data, old_cap*ITEM_BYTES, DEF_ALIGNMENT<T>, GET_LINE_INFO());
            memory_resize_deallocate(alloc, &new_slots, new_cap*SLOT_BYTES, array->_slots, old_cap*SLOT_BYTES, SLOT_ALIGNMENT, GET_LINE_INFO());

            array->_size = (uint32_t) to_size;
            array->_data = new_data;
            array->_slots = new_slots;
            array->_capacity = (uint32_t) new_capacity;

            assert(is_invariant(*array));
            return true;
        }

        template<class T>
        void set_capacity(Slot_Array<T>* array, isize new_capacity)
        {
            if(set_capacity_failing(array, new_capacity) == false)
            {
                const char* alloc_name = array->_allocator->get_stats().name;
                PANIC(panic_cformat, "Slot_Array<T> allocation failed! "
                    "Attempted to allocated %t bytes from allocator %s"
                    "Slot_Array: {size: %t, capacity: %t} sizeof(T): %z",
                    new_capacity*sizeof(T), alloc_name ? alloc_name : "<No alloc name>", 
                    array->_size, array->_capacity, sizeof(T));
            }
        }
        
    }

    template<class T>
    bool reserve_failing(Slot_Array<T>* array, isize to_size) noexcept
    {
        if (array->_capacity >= to_size)
            return true;
            
        return slot_array_internal::set_capacity_failing(array, to_size);
    }

    template<class T>
    void reserve(Slot_Array<T>* array, isize to_capacity)
    {
        if (array->_capacity < to_capacity)
            slot_array_internal::set_capacity(array, to_capacity);
    }

    template<class T>
    void grow(Slot_Array<T>* array, isize to_fit)
    {
        if (array->_capacity >= to_fit)
            return;
        
        isize new_capacity = (isize) array->_size*3/2 + 8;
        while(new_capacity < to_fit)
            new_capacity *= 2;

        slot_array_internal::set_capacity(array, new_capacity);
    }
    
    template<class T>
    Slot_Array<T>::~Slot_Array() noexcept
    {
        slot_array_internal::set_capacity(this, 0);
    }

    template<class T>
    uint32_t add(Slot_Array<T>* array, T val)
    {
        assert(is_invariant(*array));
        grow(array, array->_size + 1);
        
        using namespace slot_array_internal;
        uint32_t added_i = array->_size;
        uint32_t added_slot_i = array->_first_free_slot;

        new (&array->_data[added_i]) T((T&&) val);
        
        uint32_t* added_slot = array->_slots + SLOT_SIZE*added_slot_i;
        uint32_t* owned_slot = array->_slots + SLOT_SIZE*added_i;

        array->_first_free_slot = added_slot[NEXT];

        added_slot[ITEM] = added_i;
        added_slot[NEXT] = (uint32_t) -1; //is not free
        owned_slot[OWNER] = added_i; //set owned by the newly added

        if(array->_max_slot < added_slot)
            array->_max_slot = added_slot;

        array->_size++;
        return added_slot_i;
        assert(is_invariant(*array));
    }

    template<class T>
    T remove(Slot_Array<T>* array, uint32_t id)
    {
        assert(is_invariant(*array));
        
        using namespace slot_array_internal;
        uint32_t* removed_slot = array->_slots + SLOT_SIZE*id;
        uint32_t* look_up_slot = array->_slots + SLOT_SIZE*(array->_size - 1);
        uint32_t* excange_slot = array->_slots + SLOT_SIZE*look_up_slot[OWNER];

        assert(id < array->_capacity);
        assert(look_up_slot[OWNER] < array->_capacity);

        uint32_t removed_i = removed_slot[ITEM];
        uint32_t excange_i = array->_size - 1;

        assert(excange_i == excange_slot[ITEM]);

        uint32_t* owned_slot = array->_slots + SLOT_SIZE*added_i;

        T removed = (T&&) array->_data[removed_i];
        array->_data[removed_i] = (T&&) array->_data[excange_i];
        array->_data[excange_i].~T();
        array->_first_free_slot = id;
        array->_size--;
        assert(is_invariant(*array));

        return removed;
    }
    
    template<class T>
    isize to_index(Slot_Array<T> const& array, uint32_t id)
    {
        assert(id < array->_capacity && "id out of bounds!");
        using namespace slot_array_internal;
        uint32_t* slot = array->_slots + SLOT_SIZE*id;

        return (isize) slot[ITEM];
    }
    
    template<class T>
    uint32_t to_id(Slot_Array<T> const& array, isize index)
    {
        assert(index < array->_size && "index out of bounds!");
        using namespace slot_array_internal;
        uint32_t* slot = array->_slots + SLOT_SIZE*index;

        return slot[OWNER];
    }
}