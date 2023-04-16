#pragma once

#include "memory.h"
#include "intrusive_index_list.h"
#include "array.h" //remove this dependency
#include "intrin.h"

namespace jot
{
    // BUCKET ARRAY
    // 
    // We keep an array of buckets each containing some number of slots
    // 
    // Within a single bucket we keep track of used slots via a bit mask 
    //  we use bitmask because of fast ffs operations on uint64_t letting us scan 64 slots in a single
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

    #ifdef BUCKET_ARRAY_PEDANTIC_LIST
        #define BUCKET_ARRAY_DEFAULT_PEDANTIC_VALUE true
    #else
        #define BUCKET_ARRAY_DEFAULT_PEDANTIC_VALUE false
    #endif
    
    #ifndef NODISCARD
        #define NODISCARD [[nodiscard]]
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
        usize mask = ~((usize) -1 << log2_bucket_size);

        Bucket_Index out = {};
        out.bucket_i = index >> log2_bucket_size;
        out.slot_i = (isize) (index & mask);

        return out;
    }
    
    inline
    isize from_bucket_index(Bucket_Index index, isize log2_bucket_size)
    {
        assert(0 <= index.bucket_i);
        assert(0 <= index.slot_i && (index.slot_i >> log2_bucket_size) == 0 && "must be within range!");
        assert(0 < log2_bucket_size && log2_bucket_size < 64);

        isize out = (index.bucket_i << log2_bucket_size) + index.slot_i;
        return out;
    }

    namespace bucket_array_internal
    {
        using Mask = uint64_t;
        static constexpr isize MASK_BITS = (isize) sizeof(Mask) * 8;

        struct Bucket
        {
            void* data = nullptr;
            Mask* mask = nullptr;
            uint32_t used_count = 0;
            uint32_t has_allocation = 0;
            
            uint32_t next = NULL_LIST_INDEX;
            uint32_t prev = NULL_LIST_INDEX;
        };

        struct Untyped_Bucket_Array
        {
            Array<Bucket> _buckets = {}; //4
            Index_Chain _open_buckets = {}; //1
            isize _total_used = 0;      //1
            isize _total_capacity = 0;  //1
            uint8_t _log2_bucket_size = 0; //1/8
            uint32_t _max_bucket_size = 0;

            Untyped_Bucket_Array(isize log2_bucket_size, Allocator* alloc) noexcept 
                : _buckets(alloc), _log2_bucket_size((uint8_t) log2_bucket_size)
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

        
        #define _UC_COMPARE_CONST 8
        #define cmp_uc(a, cmp, b) ((a) / _UC_COMPARE_CONST) cmp ((b) / _UC_COMPARE_CONST)

        inline 
        bool is_invariant(Untyped_Bucket_Array const& bucket_array, bool pedantic = BUCKET_ARRAY_DEFAULT_PEDANTIC_VALUE) noexcept
        {
            Slice<const Bucket> arr = slice(bucket_array._buckets);
            isize bucket_size = (isize) 1 << bucket_array._log2_bucket_size;
            uint32_t first = bucket_array._open_buckets.first;
            uint32_t last = bucket_array._open_buckets.last;

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
                uint32_t curr = first;
                uint32_t prev = NULL_LIST_INDEX;

                while(curr != NULL_LIST_INDEX && prev != last)
                {

                    if(prev != NULL_LIST_INDEX)
                    {
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
        static
        bool add_bucket_block(Untyped_Bucket_Array* bucket_array, isize total_block_size, isize slot_size, isize slots_align) noexcept
        {
            assert(total_block_size > 0 && slot_size > 0 && slots_align > 0);

            Allocator* alloc = bucket_array->_buckets._allocator;
            Array<Bucket>* buckets = &bucket_array->_buckets;

            isize bucket_size = (isize) 1 << bucket_array->_log2_bucket_size;
            isize bucket_count = div_round_up(total_block_size, bucket_size);

            isize single_mask_size = div_round_up(bucket_size, MASK_BITS);
            isize new_block_size = bucket_count * bucket_size;

            isize data_size = new_block_size * slot_size;
            isize masks_size = single_mask_size * bucket_count * (isize) sizeof(Mask);
            
            if(reserve_failing(buckets, bucket_count + size(buckets)) == false)
                return false;

            void* new_data = alloc->allocate(data_size, slots_align, GET_LINE_INFO());
            if(new_data == nullptr)
                return false;

            void* new_mask = alloc->allocate(masks_size, USED_SLOTS_ALIGN, GET_LINE_INFO());
            if(new_mask == nullptr)
            {
                alloc->deallocate(new_data, data_size, slots_align, GET_LINE_INFO());
                return false;
            }
            
            memset(new_data, 0, data_size);

            uint8_t* curr_data = (uint8_t*) new_data;
            Mask* curr_mask = (Mask*) new_mask;
            for(isize i = 0; i < bucket_count; i++)
            {
                Bucket bucket;
                bucket.used_count = 0;
                bucket.data = (void*) curr_data;
                bucket.mask = curr_mask;
                bucket.has_allocation = i == 0; 

                push(&bucket_array->_buckets, bucket);

                curr_data += bucket_size * slot_size;
                curr_mask += single_mask_size;
            }

            bucket_array->_max_bucket_size = max(bucket_array->_max_bucket_size, (uint32_t) total_block_size);
            bucket_array->_total_capacity += new_block_size;

            return true;
        }
        
        struct Bucket_Array_Growth
        {
            isize base_size = 256;
            isize add_increment = 0;
            isize mult_increment_num = 3;
            isize mult_increment_den = 2;
        };
        
        static 
        uint32_t add_free_buckets(Untyped_Bucket_Array* bucket_array, Bucket_Array_Growth const& growth, isize slot_size, isize slots_align) 
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
            if(add_bucket_block(bucket_array, new_size, slot_size, slots_align) == false)
            {
                PANIC(panic_cformat, "bucket array allocation failed! "
                    "attempted size: %t slot size: %t slot align: %t "
                    "bucket array: {used: %t, capacity %t, buckets %t, log2_size: %t}", 
                    new_size, slot_size, slots_align,
                    bucket_array->_total_used, bucket_array->_total_capacity, size(bucket_array->_buckets), bucket_array->_log2_bucket_size);
            }
            
            //insert all allocated buckets to the free list
            Slice<Bucket> buckets = slice(&bucket_array->_buckets);
            for(isize to_bucket_i = size_before; to_bucket_i < buckets.size; to_bucket_i++)
                insert_node(&bucket_array->_open_buckets, bucket_array->_open_buckets.last, (uint32_t) to_bucket_i, buckets);

            return (uint32_t) size_before;
        }
        
        static 
        Bucket_Index prepare_for_insert(Untyped_Bucket_Array* bucket_array, Bucket_Array_Growth const& growth, isize slot_size, isize slots_align)
        {
            using namespace bucket_array_internal;
            isize bucket_size = (isize) 1 << bucket_array->_log2_bucket_size;

            uint32_t to_bucket_i = bucket_array->_open_buckets.first;
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
                    was_found = intrin__find_first_set_32(&bit_pos, (uint32_t) ~*mask);

                if(was_found)
                {
                    Mask bit = (Mask) 1 << bit_pos;
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
            
            isize bucket_size = (isize) 1 << bucket_array->_log2_bucket_size;
            Slice<Bucket> buckets = slice(&bucket_array->_buckets);
            Bucket* bucket = &buckets[index.bucket_i];
            assert(0 <= index.slot_i && index.slot_i < bucket_size && "out of bounds!");

            //mark slot as free        
            isize slot_mask_i = index.slot_i / MASK_BITS;
            isize slot_bit_i = index.slot_i % MASK_BITS;

            Mask bit = (Mask) 1 << slot_bit_i;
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
                insert_node(&bucket_array->_open_buckets, NULL_LIST_INDEX, (uint32_t) index.bucket_i, buckets);
                is_invariant(*bucket_array, true);
            }
            else if(bucket->next != NULL_LIST_INDEX && cmp_uc(buckets[bucket->next].used_count, >, used_count))
            {
                uint32_t insert_after = bucket->next;
                while(true)
                {
                    Bucket* curr_bucket = &buckets[insert_after];
                    if(curr_bucket->next == NULL_LIST_INDEX || cmp_uc(buckets[curr_bucket->next].used_count, <=, used_count))   
                        break;

                    insert_after = curr_bucket->next;
                }

                assert(insert_after != NULL_LIST_INDEX && insert_after != index.bucket_i);
                extract_node(&bucket_array->_open_buckets, bucket->prev, (uint32_t) index.bucket_i, buckets);
                insert_node(&bucket_array->_open_buckets, insert_after, (uint32_t) index.bucket_i, buckets);
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
        explicit Bucket_Array(isize log2_bucket_size = 8, Allocator* alloc = default_allocator()) noexcept 
            : bucket_array_internal::Untyped_Bucket_Array(log2_bucket_size, alloc) 
        {}

        explicit Bucket_Array(Allocator* alloc) noexcept 
            : bucket_array_internal::Untyped_Bucket_Array(8, alloc) 
        {}

        ~Bucket_Array() noexcept;
    };
    
    template <typename T>
    Bucket_Array<T>::~Bucket_Array() noexcept
    {
        using namespace bucket_array_internal;

        is_invariant(*this, true);
        Slice<Bucket> buckets = slice(&_buckets);
        isize bucket_size = (isize) 1 << _log2_bucket_size;
        isize mask_size = div_round_up(bucket_size, MASK_BITS);
        Allocator* alloc = _buckets._allocator;
        
        //since multiple buckets can be joined into a single allocation block
        // we only deallocate the block once it changes to a new one
        uint8_t* accumulated_data_ptr = nullptr;
        uint8_t* accumulated_mask_ptr = nullptr;

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

            Slice<T> items = {(T*) bucket->data, bucket_size};

            //for each slot check if it is used and if so call destructor on it
            for(isize j = 0; j < mask_size; j++)
            {
                //if is block empty skip
                if(bucket->mask[j] == 0)
                    continue;

                for(isize k = 0; k < MASK_BITS; k++)
                {
                    Mask bit = (Mask) 1 << k;
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
                accumulated_data_ptr = (uint8_t*) bucket->data;
                accumulated_mask_ptr = (uint8_t*) bucket->mask;
            }
            
            accumulated_data_byte_size += bucket_size * (isize) sizeof(T);
            accumulated_mask_byte_size += mask_size * (isize) sizeof(Mask);

            if(i == buckets.size - 1 || buckets[i+1].has_allocation)
            {
                alloc->deallocate(accumulated_data_ptr, accumulated_data_byte_size, alignof(T), GET_LINE_INFO());
                alloc->deallocate(accumulated_mask_ptr, accumulated_mask_byte_size, USED_SLOTS_ALIGN, GET_LINE_INFO());

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
    
    template <typename T>
    bool is_used(Bucket_Array<T> const& bucket_array, Bucket_Index index)
    {
        using namespace bucket_array_internal;
        const Bucket& bucket = bucket_array._buckets[index.bucket_i];
        
        isize bucket_size = (isize) 1 << bucket_array._log2_bucket_size;
        assert(0 <= index.slot_i && index.slot_i < bucket_size && "out of bounds!");

        isize mask_index = index.slot_i / MASK_BITS;
        isize bit_index = index.slot_i % MASK_BITS;

        Mask mask = bucket.mask[mask_index];
        Mask bit = (Mask) 1 << bit_index;

        bool res = (mask & bit) != 0;
        return res;
    }
    
    template <typename T>
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
        isize bucket_size = (isize) 1 << bucket_array->_log2_bucket_size;
        isize used_slots_size = div_round_up(bucket_size, MASK_BITS);

        for(isize i = 0; i < buckets.size; i++)
        {
            Bucket* bucket = &buckets[i];
            Slice<T> items = {(T*) bucket->data, bucket_size};

            for(isize j = 0; j < used_slots_size; j++)
            {
                if(bucket->mask[j] == 0)
                    continue;

                for(isize k = 0; k < MASK_BITS; k++)
                {
                    Mask bit = (Mask) 1 << k;
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

    template <typename T>
    T* get(Bucket_Array<T>* bucket_array, Bucket_Index index) noexcept
    {
        assert(is_used(*bucket_array, index) == true && "must be used!");

        bucket_array_internal::Bucket* bucket = &bucket_array->_buckets[index.bucket_i];
        T* bucket_data = (T*) bucket->data;
        
        return &bucket_data[index.slot_i];
    }
    
    template <typename T>
    T const& get(Bucket_Array<T> const& from, Bucket_Index index) noexcept
    {
        //sorry c++ but I am not going to implement this two times just because of const
        Bucket_Array<T>* cheated = (Bucket_Array<T>*) (void*) &from;
        return *get(cheated, index);
    }

    template <typename T>
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
    
    template <typename T> NODISCARD
    bool reserve_failing(Bucket_Array<T>* bucket_array, isize to_size) noexcept
    {
        if(to_size <= bucket_array->_total_capacity)
            return true;

        return bucket_array_internal::add_bucket_block(bucket_array, to_size, (isize) sizeof(T), alignof(T));
    }
    
    template <typename T>
    void reserve(Bucket_Array<T>* bucket_array, isize to_size) noexcept
    {
        if(to_size <= bucket_array->_total_capacity)
            return;

        if(bucket_array_internal::add_bucket_block(bucket_array, to_size, (isize) sizeof(T), alignof(T)))
        {
            PANIC(Panic, "Bucket_Array allocation failed!")
        }
    }

    template <typename T>
    Bucket_Index insert_bucket_index(Bucket_Array<T>* bucket_array, T val, Bucket_Array_Growth growth = {})
    {
        Bucket_Index loc = bucket_array_internal::prepare_for_insert(bucket_array, growth, (isize) sizeof(T), alignof(T));
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
    
    template <typename T>
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

