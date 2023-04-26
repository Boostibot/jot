#pragma once

#include <string.h>
#include "memory.h"

namespace jot
{
    #ifndef JOT_HANDLE
        #define JOT_HANDLE
        struct Handle { uint32_t index; };
    #endif
    
    namespace slot_array_internal
    {
        struct Slot;
    }

    ///Dansely packet array of items accessible through stable handle or unstable array index
    template<typename T>
    struct Slot_Array
    {
        T* _data = nullptr;
        slot_array_internal::Slot* _slots = nullptr;
        Allocator* _allocator = default_allocator();
        uint32_t _size = 0;
        uint32_t _capacity = 0;
        uint32_t _free_list = (uint32_t) -1;

        //a number that gets substracted from / added to every handle passed in.
        // This lets for example set up two slot arrays whose handles never overlap and we can thus easily detect
        // bugs in access.
        uint32_t _handle_offset = 0; 
        
        //@NOTE: if kept only data pointer and allocated (sizeof(T) + sizeof(uint32_t))*capacity bytes every
        //  time we could speed this up even more. Doing this would also mean this entire struct would be only
        //  32B which is ideal! I am afraid that that would cost us some additional clarity however 
        //  and right now I am too lazy to implement it.

        Slot_Array(Allocator* allocator = default_allocator(), uint32_t handle_offset = 0) noexcept
            : _allocator(allocator), _handle_offset(handle_offset)
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
    
    ///Reserves at least to_size slots using geometric sequence
    template<class T> void grow(Slot_Array<T>* slot_array, isize to_fit);
    
    ///Inserts an item to the array and returns its handle
    template<class T> Handle insert(Slot_Array<T>* slot_array, T val);
    
    ///Inserts an item from the array give its handle
    template<class T> T remove(Slot_Array<T>* slot_array, Handle handle) noexcept;

    ///Converts handle to element index
    template<class T> isize to_index(Slot_Array<T> const& slot_array, Handle slot) noexcept;
    ///Converts element index to handle
    template<class T> Handle to_slot(Slot_Array<T> const& slot_array, isize index) noexcept;

    ///returns an item given its handle
    template<class T> T const& get(Slot_Array<T> const& slot_array, Handle handle) noexcept;
    template<class T> T* get(Slot_Array<T>* slot_array, Handle handle) noexcept;

    ///Returns true if the structure is correct which should be always
    template<class T> bool is_invariant(Slot_Array<T> const& slot_array) noexcept;
}

namespace jot
{
    namespace slot_array_internal
    {
        //This structure is rather confusing but incredibly simple and performant. 
        // It assigns a unique stable handle to each item while still being densely packed. 
        // To do this we keep 3 extra arrays of links.
        //
        // Firstly consider and array of item indicies. We use ~~~ as index -1 considered null
        //   indeces: [0,  1, ~~~,  2, ~~~, ~~~, ~~~, 3]
        //             |   |        |                 |
        //             |   o----o   o--o     o--------o
        //             V        V      V     V
        //    items: [item1, item2, item3, item4 ]
        //
        // If we want to add a new item to this structure we:
        //   1) push new item to items array
        //   2) find empty index in the indicies array and set it to point to the newly added item
        //
        // If we want to be able to perform this in constant time we keep an additional free list array to accelerate the search
        //  
        //                      0   1  2    3   4    5     6   7
        //       next (free): [~~, ~~, 4, ~~~,  5,   6,  ~~~, ~~]
        //  
        //         free_list: 2
        // free_list (graph):    -----o o------o o--o o--o o-- -1
        //                            | |      | |  | |  | |
        //           indeces: [0,  1, ~~~,  2, ~~~, ~~~, ~~~, 3]
        //                     |   |        |                 |
        //                     |   o----o   o--o     o--------o
        //                     V        V      V     V
        //            items: [item1, item2, item3, item4 ]
        //
        // Now if we want to add a new item we just go to the free_list slot instead of searching.
        //
        // Now consider removal. To remove an item from the items array and still keep it packed in cosnatnt time
        // we need to swap it with the last item and then pop it. If we still want to keep all links valid however we
        // need to:
        //   1) find the removed item slot (=: remv_slot)
        //   2) find last item slot        (=: last_slot)
        //   3) add remv_slot to start of the free list
        //   4) set remv_slot item index to -1
        //   5) set last_slot item index to removed item
        //   6) swap and pop the item
        //
        //Again to do this in linear time we need a way of retrieving the last item slot given the last item index.
        // so we add one final array which does exactly that:
        //
        //                      0   1  2    3   4    5     6   7
        //       next (free): [~~, ~~, 4, ~~~,  5,   6,  ~~~, ~~]
        // 
        //         free_list: 2
        // free_list (graph):    -----o o------o o--o o--o o-- -1
        //                            | |      | |  | |  | |
        //           indeces: [0,  1, ~~~,  2, ~~~, ~~~, ~~~, 3]
        //                     |   |        |                 |
        //                     |   o----o   o--o     o--------o
        //                     V        V      V     V
        //            items: [item1, item2, item3, item4 ]
        //                     ||     ||     ||     ||      (|| means that they always share the same index)
        //           owners: [ 0,     1,     3,     7 ]     
        //
        // so if we wanted to get slot of item4 we would do owners[3]. The resulting index is 7 which means if we did 
        // indeces[7] we would get item index 3 which is exactly item4. This is to say that together owners and 
        // indeces make a closed loop for each item index.
        //
        // Addition of this final array somewhat complicates the functions so far but not too substantionally.
        //
        // To optimize performence of all those independent array reads we splat them all together into the Slot struct
        // slots[{index, owner, next}, {index, owner, next}, {index, owner, next}, ...]
        // 
        // To optimize further notice that whenever we use index we dont use next and wise versa. This means we can have them
        // both occupy the same place in memory thus shrinking the memory footprint by 33%! 
        //
        // This results in us having to do: 
        //   2 memory reads per slot lookup
        //   2 memory reads per insertion
        //   5 memory reads per insertion (removed item, removed slot, swapped item, swapped slot, last owner lookup)
        // The deletion seems scary but in practice it is extremely fast. Only about 3x slower than array pop
        // this is A LOT faster than removal from bucket array or hash map and about 10x faster than from std::unordered_map!

        struct Slot
        {
            union
            {
                uint32_t item;
                uint32_t next;
            };
            uint32_t owner;
        };

        template<class T>
        isize set_capacity_failing(Slot_Array<T>* slot_array, isize new_capacity) noexcept
        {
            assert(is_invariant(*slot_array));

            const isize SLOT_ALIGNMENT = 8;
            const isize ITEM_BYTES = (isize) sizeof(T);
            const isize SLOT_BYTES = (isize) sizeof(Slot);

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

                if(!s1) return new_cap*ITEM_BYTES;
                if(!s2) return new_cap*SLOT_BYTES;
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
                    
                    Slot* new_slot = (Slot*) new_slots + i;
                    new_slot->owner = (uint32_t) -1;
                    new_slot->next = (uint32_t) i + 1;
                }

                Slot* last_new_slot = (Slot*) new_slots + new_cap - 1;
                last_new_slot->next = (uint32_t) slot_array->_free_list;
                slot_array->_free_list = (uint32_t) old_cap;
            }

            memory_resize_deallocate(alloc, &new_data, new_cap*ITEM_BYTES, slot_array->_data, old_cap*ITEM_BYTES, DEF_ALIGNMENT<T>, GET_LINE_INFO());
            memory_resize_deallocate(alloc, &new_slots, new_cap*SLOT_BYTES, slot_array->_slots, old_cap*SLOT_BYTES, SLOT_ALIGNMENT, GET_LINE_INFO());

            slot_array->_size = (uint32_t) to_size;
            slot_array->_data = (T*) new_data;
            slot_array->_slots = (Slot*) new_slots;
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
            Slot curr = slot_array._slots[i];
            assert(curr.owner < slot_array._capacity && "must be in bounds");

            Slot owner = slot_array._slots[curr.owner];
            assert(i == owner.item && "all items must form closed loop");
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
    Handle insert(Slot_Array<T>* slot_array, T val)
    {
        assert(is_invariant(*slot_array));
        grow(slot_array, slot_array->_size + 1);
        
        using namespace slot_array_internal;
        Slot* slots = slot_array->_slots;

        uint32_t added_item_i = slot_array->_size;
        uint32_t added_slot_i = slot_array->_free_list;
        assert(added_slot_i < slot_array->_capacity && "should be valid!");

        new (&slot_array->_data[added_item_i]) T((T&&) val);
        slot_array->_free_list = slots[added_slot_i].next;

        slots[added_slot_i].item = added_item_i;
        slots[added_item_i].owner = added_slot_i; //set owned by the newly added

        slot_array->_size++;
        assert(is_invariant(*slot_array));
        return Handle{added_slot_i + slot_array->_handle_offset};
    }

    template<class T>
    T remove(Slot_Array<T>* slot_array, Handle handle) noexcept
    {
        uint32_t removed_slot_i = handle.index - slot_array->_handle_offset;
        using namespace slot_array_internal;

        assert(is_invariant(*slot_array));
        assert(slot_array->_size > 0 && "cannot remove from empty!");
        assert(removed_slot_i < slot_array->_capacity && "handle must be valid!");

        Slot* slots = slot_array->_slots;
        
        uint32_t last_item_i = slot_array->_size - 1;
        uint32_t excange_slot_i = slots[last_item_i].owner;

        assert(excange_slot_i < slot_array->_capacity);

        uint32_t removed_item_i = slots[removed_slot_i].item;

        assert(last_item_i == slots[excange_slot_i].item);
        slots[excange_slot_i].item = removed_item_i;
        slots[removed_item_i].owner = excange_slot_i;
        slots[removed_slot_i].next = slot_array->_free_list;

        T removed = (T&&) slot_array->_data[removed_item_i];
        slot_array->_data[removed_item_i] = (T&&) slot_array->_data[last_item_i];
        slot_array->_data[last_item_i].~T();
        slot_array->_free_list = removed_slot_i;
        slot_array->_size--;

        assert(is_invariant(*slot_array));

        return removed;
    }
    
    template<class T>
    isize to_index(Slot_Array<T> const& slot_array, Handle handle) noexcept
    {
        using namespace slot_array_internal;
        uint32_t slot_i = handle.index - slot_array._handle_offset;
        assert(slot_i < slot_array._capacity && "handle out of bounds!");
        return (isize) slot_array._slots[slot_i].item; //uwu
    }
    
    template<class T>
    Handle to_handle(Slot_Array<T> const& slot_array, isize index) noexcept
    {
        using namespace slot_array_internal;
        assert(index < slot_array->_size && "index out of bounds!");
        Slot* slot = slot_array._slots + index ;
        return Handle{slot->owner + slot_array._handle_offset};
    }
    
    template<class T>
    T const& get(Slot_Array<T> const& slot_array, Handle handle) noexcept
    {
        isize index = to_index(slot_array, handle);
        assert(index < slot_array._size && "invlaid handle!");
        return slot_array._data[index];
    }
    
    template<class T>
    T* get(Slot_Array<T>* slot_array, Handle handle) noexcept
    {
        isize index = to_index(*slot_array, handle);
        assert(index < slot_array->_size && "invlaid handle!");
        return &slot_array->_data[index];
    }
}