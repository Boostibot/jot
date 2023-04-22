#pragma once

#include <string.h>
#include "memory.h"

#if 0
#define SLOT_ARRAT_PEDANTIC_CONNECTED
#endif

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
        
        //@NOTE: if kept only data pointer and allocated (sizeof(T) + sizeof(uint32_t))*capacity bytes every
        //  time we could speed this up even more. Doing this would also mean this entire struct would be only
        //  32B which is ideal! I am afraid that that would cost us some additional clarity however 
        //  and right now I am too lazy to implement it.

        Slot_Array(Allocator* allocator = default_allocator()) noexcept
            : _allocator(allocator)
        {}

        ~Slot_Array() noexcept;

        Slot_Array& operator=(Slot_Array && other) noexcept;
        Slot_Array& operator=(Slot_Array const& other);
    };
    
    ///Getters 
    template<class T> const T*   data(Slot_Array<T> const& slot_array) noexcept       { return slot_array._data; }
    template<class T> T*         data(Slot_Array<T>* slot_array) noexcept             { return slot_array->_data; }
    template<class T> isize      size(Slot_Array<T> const& slot_array) noexcept       { return slot_array._size; }
    template<class T> isize      size(Slot_Array<T>* slot_array) noexcept             { return slot_array->_size; }
    template<class T> isize      capacity(Slot_Array<T> const& slot_array) noexcept   { return slot_array._capacity; }
    template<class T> isize      capacity(Slot_Array<T>* slot_array) noexcept         { return slot_array->_capacity; }
    template<class T> Allocator* allocator(Slot_Array<T> const& slot_array) noexcept  { return slot_array._allocator; }
    template<class T> Allocator* allocator(Slot_Array<T>* slot_array) noexcept        { return slot_array->_data; }
    
    ///Iterators
    template<typename T> T*       begin(Slot_Array<T>& slot_array) noexcept           { return slot_array._data; }
    template<typename T> const T* begin(Slot_Array<T> const& slot_array) noexcept     { return slot_array._data; }
    template<typename T> T*       end(Slot_Array<T>& slot_array) noexcept             { return slot_array._data + slot_array._size; }
    template<typename T> const T* end(Slot_Array<T> const& slot_array) noexcept       { return slot_array._data + slot_array._size; }

    ///Returns a slice containing all items of the slot_array
    template<class T> Slice<const T> slice(Slot_Array<T> const& slot_array) noexcept    { return {slot_array._data, slot_array._size}; }
    template<class T> Slice<T>       slice(Slot_Array<T>* slot_array) noexcept          { return {slot_array->_data, slot_array->_size}; }
    
    ///Reserves exactly to_size slots
    template<class T> bool reserve_failing(Slot_Array<T>* slot_array, isize to_size) noexcept;
    template<class T> void reserve(Slot_Array<T>* slot_array, isize to_capacity);
    
    ///Reserves at least to_size slots
    template<class T> void grow(Slot_Array<T>* slot_array, isize to_fit);
    
    ///Inserts an item to the array and returns its id
    template<class T> uint32_t insert(Slot_Array<T>* slot_array, T val);
    
    ///Inserts an item from the array give its id
    template<class T> T remove(Slot_Array<T>* slot_array, uint32_t id) noexcept;

    ///Converts id to element index
    template<class T> isize to_index(Slot_Array<T> const& slot_array, uint32_t id);
    ///Converts element index to id
    template<class T> uint32_t to_id(Slot_Array<T> const& slot_array, isize index);

    ///returns an item given its id
    template<class T> T const& get(Slot_Array<T> const& slot_array, uint32_t id) noexcept;
    template<class T> T* get(Slot_Array<T>* slot_array, uint32_t id) noexcept;

    ///Returns true if the structure is correct which should be always
    template<class T> bool is_invariant(Slot_Array<T> const& slot_array) noexcept;
}

namespace jot
{
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
        isize set_capacity_failing(Slot_Array<T>* slot_array, isize new_capacity) noexcept
        {
            assert(is_invariant(*slot_array));

            const isize SLOT_ALIGNMENT = 8;
            const isize ITEM_BYTES = (isize) sizeof(T);
            const isize SLOT_BYTES = SLOT_SIZE * (isize) sizeof(uint32_t);

            isize old_cap = slot_array->_capacity;
            isize new_cap = new_capacity;
            Allocator* alloc = slot_array->_allocator;
        
            void* new_data = nullptr;
            void* new_slots = nullptr;

            bool s1 = memory_resize_allocate(alloc, &new_data, new_cap*ITEM_BYTES, slot_array->_data, old_cap*ITEM_BYTES, DEF_ALIGNMENT<T>, GET_LINE_INFO());
            bool s2 = memory_resize_allocate(alloc, &new_slots, new_cap*SLOT_BYTES, slot_array->_slots, old_cap*SLOT_BYTES, SLOT_ALIGNMENT, GET_LINE_INFO());

            if(s1 == false || s2 == false)
            {
                memory_resize_undo(alloc, &new_data, new_cap*ITEM_BYTES, slot_array->_data, old_cap*ITEM_BYTES, DEF_ALIGNMENT<T>, GET_LINE_INFO());
                memory_resize_undo(alloc, &new_slots, new_cap*SLOT_BYTES, slot_array->_slots, old_cap*SLOT_BYTES, SLOT_ALIGNMENT, GET_LINE_INFO());

                if(!s1)
                    return new_cap*ITEM_BYTES;
                    
                if(!s2)
                    return new_cap*SLOT_BYTES;
            }

            isize to_size = min(slot_array->_size, new_capacity);
            
            //@NOTE: We assume reallocatble ie. that the object helds in each slice
            // do not depend on their own adress => can be freely moved around in memory
            // => we can simply memove without calling moves and destructors (see slice_ops.h)
            //assert(JOT_IS_REALLOCATABLE(T) && "we assume reallocatble!");
            if(new_data != slot_array->_data)   memcpy(new_data, slot_array->_data, (size_t) (to_size*ITEM_BYTES));
            if(new_slots != slot_array->_slots) memcpy(new_slots, slot_array->_slots, (size_t) (to_size*SLOT_BYTES));

            for(isize i = new_capacity; i < slot_array->_size; i++)
                slot_array->_data[i].~T();
        
            //when growing properly link the added nodes to the free list
            if(old_cap < new_cap)
            {
                for(isize i = old_cap; i < new_cap; i++)
                {
                    uint32_t* new_slot = (uint32_t*) new_slots + i * SLOT_SIZE;
                    new_slot[ITEM] = (uint32_t) -1; 
                    new_slot[OWNER] = (uint32_t) -1; 
                    new_slot[NEXT] = (uint32_t) i + 1; 
                }

                uint32_t* last_new_slot = (uint32_t*) new_slots + (new_cap - 1) * SLOT_SIZE;
                last_new_slot[NEXT] = (uint32_t) slot_array->_free_list;
                slot_array->_free_list = (uint32_t) old_cap;
            }

            memory_resize_deallocate(alloc, &new_data, new_cap*ITEM_BYTES, slot_array->_data, old_cap*ITEM_BYTES, DEF_ALIGNMENT<T>, GET_LINE_INFO());
            memory_resize_deallocate(alloc, &new_slots, new_cap*SLOT_BYTES, slot_array->_slots, old_cap*SLOT_BYTES, SLOT_ALIGNMENT, GET_LINE_INFO());

            slot_array->_size = (uint32_t) to_size;
            slot_array->_data = (T*) new_data;
            slot_array->_slots = (uint32_t*) new_slots;
            slot_array->_capacity = (uint32_t) new_capacity;

            assert(is_invariant(*slot_array));
            return 0;
        }

        template<class T>
        void set_capacity(Slot_Array<T>* slot_array, isize new_capacity)
        {
            isize failed_bytes_requested = set_capacity_failing(slot_array, new_capacity);
            if(failed_bytes_requested != 0)
            {
                memory_globals::out_of_memory_hadler()(GET_LINE_INFO(),"Slot_Array<T> allocation failed! "
                    "Attempted to allocated %t bytes from allocator %p "
                    "Slot_Array: {size: %t, capacity: %t} sizeof(T): %z",
                    failed_bytes_requested, slot_array->_allocator, 
                    slot_array->_size, slot_array->_capacity, sizeof(T));
            }
        }
    }

    template<class T>
    bool is_invariant(Slot_Array<T> const& slot_array) noexcept
    {
        using namespace slot_array_internal;

        bool is_size_big_enough = slot_array._capacity >= slot_array._size;
        bool has_properly_alloced_data = (slot_array._capacity == 0) == (slot_array._data == nullptr);
        bool has_properly_alloced_slots = (slot_array._capacity == 0) == (slot_array._slots == nullptr);

        #ifdef SLOT_ARRAT_PEDANTIC_CONNECTED
        bool links_match = true;
        for(isize i = 0; i < slot_array._size; i++)
        {
            uint32_t* curr = slot_array._slots + i*SLOT_SIZE;
            uint32_t owner_i = curr[OWNER];
            assert(owner_i < slot_array._capacity);

            uint32_t* owner = slot_array._slots + owner_i*SLOT_SIZE;
            assert(i == owner[ITEM]);
        }
        #endif
        return is_size_big_enough && has_properly_alloced_data && has_properly_alloced_slots;
    }
       
    template<class T>
    bool reserve_failing(Slot_Array<T>* slot_array, isize to_size) noexcept
    {
        if (slot_array->_capacity >= to_size)
            return true;
            
        return slot_array_internal::set_capacity_failing(slot_array, to_size) == 0;
    }

    template<class T>
    void reserve(Slot_Array<T>* slot_array, isize to_capacity)
    {
        if (slot_array->_capacity < to_capacity)
            slot_array_internal::set_capacity(slot_array, to_capacity);
    }

    template<class T>
    void grow(Slot_Array<T>* slot_array, isize to_fit)
    {
        if (slot_array->_capacity >= to_fit)
            return;
        
        isize new_capacity = (isize) slot_array->_size*3/2 + 8;
        while(new_capacity < to_fit)
            new_capacity *= 2;

        slot_array_internal::set_capacity(slot_array, new_capacity);
    }
    
    template<class T>
    Slot_Array<T>::~Slot_Array() noexcept
    {
        slot_array_internal::set_capacity(this, 0);
    }

    template<class T>
    uint32_t insert(Slot_Array<T>* slot_array, T val)
    {
        assert(is_invariant(*slot_array));
        grow(slot_array, slot_array->_size + 1);
        
        using namespace slot_array_internal;
        uint32_t added_item_i = slot_array->_size;
        uint32_t added_slot_i = slot_array->_free_list;
        assert(added_slot_i != (uint32_t) -1 && "should not be empty!");

        new (&slot_array->_data[added_item_i]) T((T&&) val);
        
        uint32_t* added_slot = slot_array->_slots + SLOT_SIZE*added_slot_i;
        uint32_t* owned_slot = slot_array->_slots + SLOT_SIZE*added_item_i;

        slot_array->_free_list = added_slot[NEXT];

        added_slot[ITEM] = added_item_i;
        added_slot[NEXT] = (uint32_t) -1; //is not free
        owned_slot[OWNER] = added_slot_i; //set owned by the newly added
        if(slot_array->_max_slot < added_slot_i)
            slot_array->_max_slot = added_slot_i;

        slot_array->_size++;
        assert(is_invariant(*slot_array));
        return added_slot_i;
    }
    

    template<class T>
    T remove(Slot_Array<T>* slot_array, uint32_t id) noexcept
    {
        assert(is_invariant(*slot_array));
        #define slot_at(i) (slot_array->_slots + 3*i)
        
        using namespace slot_array_internal;
        uint32_t last_item_i = slot_array->_size - 1;
        uint32_t excange_slot_i = slot_at(last_item_i)[OWNER];

        assert(id < slot_array->_capacity);
        assert(excange_slot_i < slot_array->_capacity);

        uint32_t removed_item_i = slot_at(id)[ITEM];

        assert(last_item_i == slot_at(excange_slot_i)[ITEM]);
        slot_at(excange_slot_i)[ITEM] = removed_item_i;
        slot_at(removed_item_i)[OWNER] = excange_slot_i;
        slot_at(id)[ITEM] = (uint32_t) -1;
        slot_at(id)[NEXT] = slot_array->_free_list;

        T removed = (T&&) slot_array->_data[removed_item_i];
        slot_array->_data[removed_item_i] = (T&&) slot_array->_data[last_item_i];
        slot_array->_data[last_item_i].~T();
        slot_array->_free_list = id;
        slot_array->_size--;

        assert(is_invariant(*slot_array));

        #undef slot_at 
        return removed;
    }
    
    
    template<class T>
    isize to_index(Slot_Array<T> const& slot_array, uint32_t id)
    {
        using namespace slot_array_internal;
        assert(id < slot_array._capacity && "id out of bounds!");
        uint32_t* slot = slot_array._slots + SLOT_SIZE*id;

        return (isize) slot[ITEM];
    }
    
    template<class T>
    uint32_t to_id(Slot_Array<T> const& slot_array, isize index)
    {
        using namespace slot_array_internal;
        assert(index < slot_array->_size && "index out of bounds!");
        uint32_t* slot = slot_array->_slots + SLOT_SIZE*index;

        return slot[OWNER];
    }
    
    template<class T>
    T const& get(Slot_Array<T> const& slot_array, uint32_t id) noexcept
    {
        isize index = to_index(slot_array, id);
        assert(index < slot_array._size && "invlaid id!");
        return slot_array._data[index];
    }
    
    template<class T>
    T* get(Slot_Array<T>* slot_array, uint32_t id) noexcept
    {
        isize index = to_index(*slot_array, id);
        assert(index < slot_array->_size && "invlaid id!");
        return &slot_array->_data[index];
    }
}