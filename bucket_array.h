#pragma once

#include "memory.h"
#include "intrusive_index_list.h"
#include "stack.h"
#include "intrin.h"
#include "defines.h"

namespace jot
{
    // BUCKET ARRAY
    // 
    // We keep an array of buckets each containing some number of slots
    // 
    // Within a single bucket we keep track of used slots via a bit mask 
    //  we use bitmask because of fast ffs operations on u64 letting us scan 64 slots in a single
    //  hardware op - this lets us find empty places pretty much instantly
    // 
    // We also keep a free list of all slots that have at least one slot open.
    //
    // We maintain this free list sorted by the occupacy of each bucket with buckets that
    //  have lower number of free slots first. This ensures we first fill all availible buckets before
    //  creating a new one and that the buckets will be always optimally packed (it is desirable to maintain
    //  the buckets fully packed because of better cache utilization and thus faster iteration times
    //  when iterating over all entries)
    //  (we can also maintain the list sorted by a < b <=> [a.size / 8] < [b.size / 8] which extends 
    //   the equivalance class 8 times and thus shrinks the cost of swapping of buckets during sorting)
    // 
    // The order of the free list is maintained with the simple following algorhitm:
    //   1 - Whenever we remove and element we decrease the Used counter
    //      - if the bucket is not yet in the free list add it to the first position 
    //        (this is valid because 'freeness' of 1 is the minimum)
    //      - if it is already added check if the sorted criterion holds with the neighboring elements
    //        and if not swap them (no more checks are required since we always decreese by 1)
    // 
    //   2 - Whenever we add an elemnt we go to the first bucket in the free list and add it there
    //       We also increase the Used counter and if the bucket is completely filled we remove it from
    //       the free list. We dont have to do any reordering because if we add to the first element it 
    //       will only become 'more' first.
    // 
    // This results in: 
    //  1 - fast constant time lookup (index based only 2 pointer derefs)
    //  2 - fast constant time addition (just deref start of the free list and ffs to find free slot)
    //  3 - fast constant time deletion (perform lookup then decrease counts and very rarely reorder buckets in a free list)
    //  4 - element adress stability
    //  5 - elemnts eventually form a densely packed array
    //  6 - is arbitrarily memory efficient (bucket size can be set arbitrarily high making
    //       it possible to drive down the ratio of 'stored data size' / 'total data structure size'
    //       as close to 0 as desired)
    
    #define BUCKET_ARRAY_PEDANTIC_GET

    #if 0
    #define BUCKET_ARRAY_PEDANTIC_GET
    #define BUCKET_ARRAY_PEDANTIC_LIST
    #endif

    struct Bucket_Index
    {   
        isize bucket_i = -1;
        isize slot_i = -1;
    };
    
    inline
    Bucket_Index to_bucket_index(isize index, isize log2_bucket_size)
    {
        assert(0 < log2_bucket_size && log2_bucket_size < 64);

        assert(index >= 0 && "invalid index");
        usize mask = ~(cast(usize) -1 << log2_bucket_size);

        Bucket_Index out = {};
        out.bucket_i = index >> log2_bucket_size;
        out.slot_i = index & mask;

        return out;
    }
    
    inline
    isize from_bucket_index(Bucket_Index index, isize log2_bucket_size)
    {
        assert(0 <= index.bucket_i);
        assert(0 <= index.slot_i && (index.slot_i >> log2_bucket_size) == 0 && "must be within range!");
        assert(0 < log2_bucket_size && log2_bucket_size < 64);

        usize out = (index.bucket_i << log2_bucket_size) + index.slot_i;
        return out;
    }

    namespace bucket_array_internal
    {
        using Mask = u64;
        static constexpr isize MASK_BITS = sizeof(Mask) * 8;

        struct Bucket
        {
            void* data = nullptr;
            Mask* mask = nullptr;
            u32 used_count = 0;
            u32 has_allocation = 0;
            
            u32 next = -1;
            u32 prev = -1;
        };

        struct Untyped_Bucket_Array
        {
            Stack<Bucket> _buckets = {}; //4
            Index_Chain _open_buckets = {}; //1
            isize _total_used = 0;      //1
            isize _total_capacity = 0;  //1
            u8 _log2_bucket_size = 0; //1/8
            u32 _max_bucket_size = 0;

            Untyped_Bucket_Array(isize log2_bucket_size, memory_globals::Default_Alloc alloc = {}) noexcept 
                : _buckets(alloc.val), _log2_bucket_size(cast(u8) log2_bucket_size)
            {
                assert(0 < log2_bucket_size && log2_bucket_size < 32 && "size must be positive and must be smaller than 32 bit number!");
            }

            ~Untyped_Bucket_Array() noexcept
            {
                assert(_total_used == 0 && "not freed!");
            }
        };

        static_assert(sizeof(Untyped_Bucket_Array) <= 64, "must be max 64B for good performence");

        static constexpr isize USED_SLOTS_ALIGN = 16; //should be enough for all SIMD instructions

        #ifdef BUCKET_ARRAY_PEDANTIC_LIST
            #define BUCKET_ARRAY_DEFAULT_PEDANTIC_VALUE true
        #else
            #define BUCKET_ARRAY_DEFAULT_PEDANTIC_VALUE false
        #endif
        
        #define _UC_COMPARE_CONST 8
        #define cmp_uc(a, cmp, b) ((a) / _UC_COMPARE_CONST) cmp ((b) / _UC_COMPARE_CONST)

        inline 
        bool is_invariant(Untyped_Bucket_Array const& bucket_array, bool pedantic = BUCKET_ARRAY_DEFAULT_PEDANTIC_VALUE) noexcept
        {
            Slice<const Bucket> arr = slice(bucket_array._buckets);
            isize bucket_size = cast(isize) 1 << bucket_array._log2_bucket_size;
            u32 first = bucket_array._open_buckets.first;
            u32 last = bucket_array._open_buckets.last;

            bool is_total_size_plausible = bucket_array._total_used <= bucket_array._total_capacity;
            bool is_sorted = true;
            bool is_conencted = true;
            bool is_isolated = true;
            bool first_last_match = (first == NULL_LIST_INDEX) == (last == NULL_LIST_INDEX);
            bool total_size_matches = true;
            bool total_capacity_matches = true;
            
            const Bucket* first_bucket = nullptr;
            const Bucket* last_bucket = nullptr;

            if(first != NULL_LIST_INDEX && last != NULL_LIST_INDEX)
            {
                first_bucket = &arr[first];
                last_bucket = &arr[last];
            
                is_isolated = first_bucket->prev == NULL_LIST_INDEX && last_bucket->next == NULL_LIST_INDEX;
                assert(is_isolated);
            }
            
            if(pedantic)
            {
                u32 curr = first;
                u32 prev = NULL_LIST_INDEX;

                while(curr != NULL_LIST_INDEX && prev != last)
                {

                    if(prev != NULL_LIST_INDEX)
                    {
                        const Bucket* curr_bucket = &arr[curr];
                        const Bucket* prev_bucket = &arr[prev]; 
                        if(cmp_uc(arr[prev].used_count, <, arr[curr].used_count))
                        {
                            is_sorted = false;
                            break;
                        }
                    }

                    prev = curr;
                    curr = arr[curr].next;
                }

                is_conencted = prev == last;
                assert(is_conencted);
                assert(is_sorted);

                isize total_size = 0;
                isize total_capacity = arr.size * bucket_size;
                for(isize i = 0; i < arr.size; i++)
                    total_size += arr[i].used_count;

                total_size_matches = total_size == bucket_array._total_used;
                total_capacity_matches = total_capacity == bucket_array._total_capacity;

                assert(total_size_matches);
                assert(total_capacity_matches);
            }

            assert(is_total_size_plausible);
            assert(first_last_match);

            return is_total_size_plausible
                && is_sorted
                && is_conencted
                && is_isolated
                && first_last_match;
        }

        //Allocates contiguously space for total_block_size elements and adds total_block_size / bucket_size blocks to the block stack.
        nodisc static
        Allocator_State_Type add_bucket_block(Untyped_Bucket_Array* bucket_array, isize total_block_size, isize slot_size, isize slots_align) noexcept
        {
            assert(total_block_size > 0 && slot_size > 0 && slots_align > 0);

            Allocator* alloc = bucket_array->_buckets._allocator;
            Stack<Bucket>* buckets = &bucket_array->_buckets;

            isize bucket_size = cast(isize) 1 << bucket_array->_log2_bucket_size;
            isize bucket_count = div_round_up(total_block_size, bucket_size);

            isize single_mask_size = div_round_up(bucket_size, MASK_BITS);
            isize new_block_size = bucket_count * bucket_size;

            isize data_size = new_block_size * slot_size;
            isize masks_size = single_mask_size * bucket_count * sizeof(Mask);
            
            Allocator_State_Type bucket_reserve = reserve_failing(buckets, bucket_count + size(buckets));
            if(bucket_reserve != Allocator_State::OK)
                return bucket_reserve;

            Allocation_Result data = alloc->allocate(data_size, slots_align);
            if(data.state != Allocator_State::OK)
                return data.state;

            Allocation_Result mask = alloc->allocate(masks_size, USED_SLOTS_ALIGN);
            if(mask.state != Allocator_State::OK)
            {
                alloc->deallocate(data.items, slots_align);
                return mask.state;
            }
            
            null_items(mask.items);

            u8* curr_data = data.items.data;
            Mask* curr_mask = cast(Mask*) mask.items.data;
            for(isize i = 0; i < bucket_count; i++)
            {
                Bucket bucket;
                bucket.used_count = 0;
                bucket.data = cast(void*) curr_data;
                bucket.mask = curr_mask;
                bucket.has_allocation = i == 0; 

                push(&bucket_array->_buckets, bucket);

                curr_data += bucket_size * slot_size;
                curr_mask += single_mask_size;
            }

            bucket_array->_max_bucket_size = max(bucket_array->_max_bucket_size, cast(u32) total_block_size);
            bucket_array->_total_capacity += new_block_size;

            return Allocator_State::OK;
        }
        
        struct Bucket_Array_Growth
        {
            isize base_size = 256;
            isize add_increment = 0;
            isize mult_increment_num = 3;
            isize mult_increment_den = 2;
        };
        
        static 
        u32 add_free_buckets(Untyped_Bucket_Array* bucket_array, Bucket_Array_Growth const& growth, isize slot_size, isize slots_align) 
        {
            isize size_before = size(bucket_array->_buckets);
            isize last_size = bucket_array->_max_bucket_size;

            //Calculate growth
            assert(growth.base_size >= 0
                && growth.add_increment >= 0
                && growth.mult_increment_num >= 0
                && growth.mult_increment_den > 0 && "invalid growth!");

            last_size = max(last_size, growth.base_size);
            isize new_size = last_size + growth.add_increment
                + last_size * growth.mult_increment_num / growth.mult_increment_den;

            assert(new_size > 0 && "resulting size must be nonzero!");
            Allocator_State_Type alloc_state = add_bucket_block(bucket_array, new_size, slot_size, slots_align);
            force(alloc_state == Allocator_State::OK && "bucket array allocation failed!");
            
            //insert all allocated buckets to the free list
            Slice<Bucket> buckets = slice(&bucket_array->_buckets);
            for(isize to_bucket_i = size_before; to_bucket_i < buckets.size; to_bucket_i++)
                insert_node(&bucket_array->_open_buckets, bucket_array->_open_buckets.last, cast(u32) to_bucket_i, buckets);

            return cast(u32) size_before;
        }
        
        //@TODO: fix crashes of map on stress test when shrinking rehash (add rehash test to other tests 
        // then add rehash to size to test options
        //@TODO: speed up compilation and type erasure of everything
        //@TODO: more elaborated benchmark of iteration
        //@TODO: make separate stack header that contains the bare minimum
        static 
        Bucket_Index prepare_for_insert(Untyped_Bucket_Array* bucket_array, Bucket_Array_Growth const& growth, isize slot_size, isize slots_align)
        {
            using namespace bucket_array_internal;
            isize bucket_size = cast(isize) 1 << bucket_array->_log2_bucket_size;

            u32 to_bucket_i = bucket_array->_open_buckets.first;
            if(to_bucket_i == NULL_LIST_INDEX)
            {
                is_invariant(*bucket_array, true);
                to_bucket_i = add_free_buckets(bucket_array, growth, slot_size, slots_align);
            }
            else
                is_invariant(*bucket_array);
        
            Bucket* to_bucket = &bucket_array->_buckets[to_bucket_i];
            assert(to_bucket->used_count < bucket_size && "should have a free slot");
            assert(to_bucket->prev == NULL_LIST_INDEX && "should be first node");
        
            //Iterate over all bits of mask and when an empty space is found
            // insert val in place.
            isize found_index = -1;
            isize slot_block_size = div_round_up(bucket_size, MASK_BITS);
            
            for(isize i = 0; i < slot_block_size; i++)
            {
                Mask* mask = &to_bucket->mask[i];
                size_t bit_pos = 0;
                bool was_found = false;
                if constexpr(sizeof(Mask) == 8)
                    was_found = intrin__find_first_set_64(&bit_pos, ~*mask);
                else
                    was_found = intrin__find_first_set_32(&bit_pos, cast(u32) ~*mask);

                if(was_found)
                {
                    Mask bit = cast(Mask) 1 << bit_pos;
                    *mask |= bit;
                    found_index = MASK_BITS * i + bit_pos;
                        
                    i = slot_block_size;
                    break;
                }
            }
            assert(found_index != -1 && "should have been found");
            assert(found_index < bucket_size && "has corrupted mask bits");

            //increment counters
            to_bucket->used_count += 1;
            bucket_array->_total_used += 1;
            assert(to_bucket->used_count <= bucket_size && "should not be overfull!");
        
            //if is completely full extract it from open bucket_array
            if(to_bucket->used_count == bucket_size)
            {
                extract_node(&bucket_array->_open_buckets, to_bucket->prev, to_bucket_i, slice(&bucket_array->_buckets));
                is_invariant(*bucket_array, true);
            }

            //Assert local consistency
            assert(to_bucket->prev == NULL_LIST_INDEX && "should be first node");
            if(to_bucket->next != NULL_LIST_INDEX)
            {
                Bucket* next_bucket = &bucket_array->_buckets[to_bucket->next];
                assert(cmp_uc(next_bucket->used_count, <=, to_bucket->used_count) && "used_count's should be consistent");
            }

            Bucket_Index out = {0};
            out.bucket_i = to_bucket_i;
            out.slot_i = found_index;

            is_invariant(*bucket_array);

            return out;
        }

        void prepare_for_remove(Untyped_Bucket_Array* bucket_array, Bucket_Index index) noexcept
        {
            using namespace bucket_array_internal;
            is_invariant(*bucket_array);
            
            isize bucket_size = cast(isize) 1 << bucket_array->_log2_bucket_size;
            Slice<Bucket> buckets = slice(&bucket_array->_buckets);
            Bucket* bucket = &buckets[index.bucket_i];
            assert(0 <= index.slot_i && index.slot_i < bucket_size && "out of bounds!");

            //mark slot as free        
            isize slot_mask_i = index.slot_i / MASK_BITS;
            isize slot_bit_i = index.slot_i % MASK_BITS;

            Mask bit = cast(Mask) 1 << slot_bit_i;
            Mask* mask = &bucket->mask[slot_mask_i];
            assert((*mask & bit) > 0 && "provided index is invalid! was not previously allocated to!");
            *mask &= ~bit; //sets the bit to 0;

            //decrease used_count and swap blocks if necessary
            bucket->used_count -= 1;
            bucket_array->_total_used -= 1;
            isize used_count = bucket->used_count;

            //if was full insert it to the start of the list
            if(bucket->used_count == bucket_size - 1)
            {
                insert_node(&bucket_array->_open_buckets, NULL_LIST_INDEX, cast(u32) index.bucket_i, buckets);
                is_invariant(*bucket_array, true);
            }
            else if(bucket->next != NULL_LIST_INDEX && cmp_uc(buckets[bucket->next].used_count, >, used_count))
            {
                u32 insert_after = bucket->next;
                while(true)
                {
                    Bucket* curr_bucket = &buckets[insert_after];
                    if(curr_bucket->next == NULL_LIST_INDEX || cmp_uc(buckets[curr_bucket->next].used_count, <=, used_count))   
                        break;

                    insert_after = curr_bucket->next;
                }

                assert(insert_after != NULL_LIST_INDEX && insert_after != index.bucket_i);
                extract_node(&bucket_array->_open_buckets, bucket->prev, cast(u32) index.bucket_i, buckets);
                insert_node(&bucket_array->_open_buckets, insert_after, cast(u32) index.bucket_i, buckets);
                is_invariant(*bucket_array, true);
            }
            else
            {
                is_invariant(*bucket_array);
            }
        }
    }

    using bucket_array_internal::Bucket_Array_Growth;

    template <typename T>
    struct Bucket_Array : public bucket_array_internal::Untyped_Bucket_Array
    {
        explicit Bucket_Array(isize log2_bucket_size = 8, memory_globals::Default_Alloc alloc = {}) noexcept 
            : bucket_array_internal::Untyped_Bucket_Array(log2_bucket_size, alloc) 
        {}

        ~Bucket_Array() noexcept;
    };
    
    template <typename T>
    Bucket_Array<T>::~Bucket_Array() noexcept
    {
        using namespace bucket_array_internal;

        is_invariant(*this, true);
        Slice<Bucket> buckets = slice(&_buckets);
        isize bucket_size = cast(isize) 1 << _log2_bucket_size;
        isize mask_size = div_round_up(bucket_size, MASK_BITS);
        Allocator* alloc = _buckets._allocator;
        
        //since multiple buckets can be joined into a single allocation block
        // we only deallocate the block once it changes to a new one
        u8* accumulated_data_ptr = nullptr;
        u8* accumulated_mask_ptr = nullptr;

        isize accumulated_data_byte_size = 0;
        isize accumulated_mask_byte_size = 0;
        isize buckets_without_allocation = 0;
        
        //For each bucket scan through all of its active elements and call
        // a destructor on them
        for(isize i = 0; i < buckets.size; i++)
        {
            Bucket* bucket = &buckets[i];
            assert(bucket->mask != nullptr && "should be init");
            assert(bucket->data != nullptr && "should be init");

            Slice<T> items = {cast(T*) bucket->data, bucket_size};

            //for each slot check if it is used and if so call destructor on it
            for(isize j = 0; j < mask_size; j++)
            {
                //if is block empty skip
                if(bucket->mask[j] == 0)
                    continue;

                for(isize k = 0; k < MASK_BITS; k++)
                {
                    Mask bit = cast(Mask) 1 << k;
                    if(bucket->mask[j] & bit)
                    {
                        //if the slot_i is in the padding area...
                        isize slot_i = j * MASK_BITS + k;
                        if(slot_i >= bucket_size)
                            break;

                        T& item = items[slot_i];
                        item.~T();
                    }
                }
            }

            if(accumulated_data_ptr == nullptr)
            {
                accumulated_data_ptr = cast(u8*) bucket->data;
                accumulated_mask_ptr = cast(u8*) bucket->mask;
            }
            
            accumulated_data_byte_size += bucket_size * cast(isize) sizeof(T);
            accumulated_mask_byte_size += mask_size * cast(isize) sizeof(Mask);

            if(i == buckets.size - 1 || buckets[i+1].has_allocation)
            {
                Slice<u8> data = {accumulated_data_ptr, accumulated_data_byte_size};
                Slice<u8> mask = {accumulated_mask_ptr, accumulated_mask_byte_size};
                    
                alloc->deallocate(data, alignof(T));
                alloc->deallocate(mask, USED_SLOTS_ALIGN);

                accumulated_data_byte_size = 0;
                accumulated_mask_byte_size = 0;

                accumulated_data_ptr = nullptr;
                accumulated_mask_ptr = nullptr;
            }
            else
            {
                buckets_without_allocation ++;
            }
        }

        _total_used = 0;
    }
    
    template <typename T> nodisc
    bool is_used(Bucket_Array<T> const& bucket_array, Bucket_Index index)
    {
        using namespace bucket_array_internal;
        const Bucket& bucket = bucket_array._buckets[index.bucket_i];
        
        isize bucket_size = cast(isize) 1 << bucket_array._log2_bucket_size;
        assert(0 <= index.slot_i && index.slot_i < bucket_size && "out of bounds!");

        isize mask_index = index.slot_i / MASK_BITS;
        isize bit_index = index.slot_i % MASK_BITS;

        Mask mask = bucket.mask[mask_index];
        Mask bit = cast(Mask) 1 << bit_index;

        bool res = (mask & bit) != 0;
        return res;
    }
    
    template <typename T> nodisc
    bool is_used(Bucket_Array<T> const& bucket_array, isize index)
    {
        Bucket_Index bucket_index = to_bucket_index(index, bucket_array._log2_bucket_size);
        return is_used(bucket_array, bucket_index);
    }

    //Item_Fn: void item_callback(T* item, Bucket_Array<T>* arr, Bcuket_Index index)
    template <typename T, typename Item_Fn>
    void map_mutate(Bucket_Array<T>* bucket_array, Item_Fn item_callback)
    {
        using namespace bucket_array_internal;
        
        Slice<Bucket> buckets = slice(&bucket_array->_buckets);
        isize bucket_size = cast(isize) 1 << bucket_array->_log2_bucket_size;
        isize used_slots_size = div_round_up(bucket_size, MASK_BITS);

        for(isize i = 0; i < buckets.size; i++)
        {
            Bucket* bucket = &buckets[i];
            Slice<T> items = {cast(T*) bucket->data, bucket_size};

            for(isize j = 0; j < used_slots_size; j++)
            {
                if(bucket->mask[j] == 0)
                    continue;

                for(isize k = 0; k < MASK_BITS; k++)
                {
                    Mask bit = cast(Mask) 1 << k;
                    if(bucket->mask[j] & bit)
                    {
                        isize slot_index = j * MASK_BITS + k;
                        isize bucket_index = i;
                        T* item = &items[slot_index];
                        item_callback(item, bucket_array, bucket_index, slot_index);
                    }
                }
            }
        }
    }

    template <typename T> nodisc
    T* get(Bucket_Array<T>* bucket_array, Bucket_Index index) noexcept
    {
        assert(is_used(*bucket_array, index) == true && "must be used!");

        bucket_array_internal::Bucket* bucket = &bucket_array->_buckets[index.bucket_i];
        T* bucket_data = cast(T*) bucket->data;
        
        return &bucket_data[index.slot_i];
    }
    
    template <typename T>
    T const& get(Bucket_Array<T> const& from, Bucket_Index index) noexcept
    {
        //sorry c++ but I am not going to implement this two times just because of const
        Bucket_Array<T>* cheated = cast(Bucket_Array<T>*) cast(void*) &from;
        return *get(cheated, index);
    }

    template <typename T> nodisc
    T* get(Bucket_Array<T>* from, isize index) noexcept
    {
        return get(from, to_bucket_index(index, from->_log2_bucket_size));
    }
    
    template <typename T>
    T const& get(Bucket_Array<T> const& from, isize index) noexcept
    {
        return get(from, to_bucket_index(index, from._log2_bucket_size));
    }

    template <typename T>
    isize size(Bucket_Array<T> const& bucket_array) noexcept
    {
        return bucket_array._total_used;
    }

    template <typename T>
    isize capacity(Bucket_Array<T> const& bucket_array) noexcept
    {
        return bucket_array._total_capacity;
    }
    
    template <typename T> nodisc
    Allocator_State_Type reserve_failing(Bucket_Array<T>* bucket_array, isize to_size) noexcept
    {
        if(to_size <= bucket_array->_total_capacity)
            return Allocator_State::OK;

        return bucket_array_internal::add_bucket_block(bucket_array, to_size, sizeof(T), alignof(T));
    }
    
    template <typename T> nodisc
    void reserve(Bucket_Array<T>* bucket_array, isize to_size) noexcept
    {
        if(to_size <= bucket_array->_total_capacity)
            return;

        Allocator_State_Type s = bucket_array_internal::add_bucket_block(bucket_array, to_size, sizeof(T), alignof(T));
        force(s == Allocator_State::OK && "Bucket_Array allocation failed!");
    }

    template <typename T> nodisc
    Bucket_Index insert_bucket_index(Bucket_Array<T>* bucket_array, T val, Bucket_Array_Growth growth = {})
    {
        Bucket_Index loc = bucket_array_internal::prepare_for_insert(bucket_array, growth, sizeof(T), alignof(T));
        T* address = get(bucket_array, loc);

        new (address) T(move(&val));

        return loc;
    }

    template <typename T>
    T remove(Bucket_Array<T>* bucket_array, Bucket_Index index) noexcept
    {
        T* address = get(bucket_array, index);
        bucket_array_internal::prepare_for_remove(bucket_array, index);

        T val = move(address);
        address->~T();

        return val;
    }
    
    template <typename T> nodisc
    isize insert(Bucket_Array<T>* bucket_array, T val, Bucket_Array_Growth growth = {})
    {
        Bucket_Index index = insert_bucket_index(bucket_array, move(&val), growth);
        return from_bucket_index(index, bucket_array->_log2_bucket_size);
    }
    
    template <typename T>
    T remove(Bucket_Array<T>* bucket_array, isize index) noexcept
    {
        Bucket_Index bucket_index = to_bucket_index(index, bucket_array->_log2_bucket_size);
        return remove(bucket_array, bucket_index);
    }
}

#include "jot/undefs.h"