#pragma once

#include "memory.h"
#include "intrusive_list.h"
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
    
    #if 0
    #define INTRUSIVE_INDEX_LIST_PEDANTIC 
    #define BUCKET_ARRAY_PEDANTIC
    #endif

    namespace bucket_array_internal
    {
        static constexpr u32 EMPTY_LINK = -1;

        
        struct Bucket
        {
            void* data = nullptr;
            u64* used_slots = nullptr;
            u32 used_count = 0;
            u32 size = 0;
            
            u32 next = EMPTY_LINK;
            u32 prev = EMPTY_LINK;
        };
        

        //@TODO: make intrusive_index_list
        struct Index_Chain
        {
            u32 first = EMPTY_LINK;
            u32 last = EMPTY_LINK;
        };
        
        inline 
        bool is_isolated(u32 node, const Bucket arr[]) noexcept
        {
            const Bucket* bucket = &arr[node];
            return bucket->prev == EMPTY_LINK && bucket->next == EMPTY_LINK;
        }
        
        inline 
        bool is_isolated(u32 first, u32 last, const Bucket arr[]) noexcept
        {
            return arr[first].prev == EMPTY_LINK && arr[last].next == EMPTY_LINK;
        }

        inline 
        bool is_connected(u32 first, u32 last, const Bucket arr[]) noexcept
        {
            u32 current = first;
            u32 prev = EMPTY_LINK;

            while(current != EMPTY_LINK && prev != last)
            {
                prev = current;
                current = arr[current].next;
            }

            return prev == last;
        }

        inline 
        bool _check_is_connected(u32 first, u32 last, const Bucket arr[]) noexcept
        {
            #ifdef INTRUSIVE_INDEX_LIST_PEDANTIC
            return is_connected(first, last, arr);
            #else
            return true;
            #endif

        }

        static
        void link_chain(u32 before, u32 first_inserted, u32 last_inserted, u32 after, Bucket arr[]) noexcept
        {
            assert(first_inserted != EMPTY_LINK && last_inserted != EMPTY_LINK && "must not be null");
        
            arr[last_inserted].next = after;
            if(before != EMPTY_LINK)
            {
                assert(arr[before].next == after && "before and after must be adjecent!");
                arr[before].next = first_inserted;
            }
            
            arr[first_inserted].prev = before;
            if(after != EMPTY_LINK)
            {
                assert(arr[after].prev == before && "before and after must be adjecent!");
                arr[after].prev = last_inserted;
            }
        }
        
        static
        void unlink_chain(u32 before, u32 first_inserted, u32 last_inserted, u32 after, Bucket arr[]) noexcept
        {
            assert(first_inserted != EMPTY_LINK && last_inserted != EMPTY_LINK && "must not be null");

            arr[last_inserted].next = EMPTY_LINK;
            if(before != EMPTY_LINK)
            {
                assert(arr[before].next == first_inserted && "before and first_inserted must be adjecent!");
                arr[before].next = after;
            }

            arr[first_inserted].prev = EMPTY_LINK;
            if(after != EMPTY_LINK)
            {
                assert(arr[after].prev == last_inserted && "last_inserted and after must be adjecent!");
                arr[after].prev = before;
            }
        }

        static
        u32 extract_node(Index_Chain* from, u32 extract_after, u32 what, Bucket arr[]) noexcept
        {
            assert(what != EMPTY_LINK && "cannot be nullptr");
            assert(from->first != EMPTY_LINK && "cant extract from empty chain");
            assert(_check_is_connected(from->first, from->last, arr));

            //if is start of chain
            if(extract_after == EMPTY_LINK)
                from->first = arr[what].next;
            else
                assert(arr[extract_after].next == what);
            
            //if is end of chain
            if(what == from->last)
                from->last = extract_after;

            unlink_chain(extract_after, what, what, arr[what].next, arr);
            
            if(from->first == EMPTY_LINK || from->last == EMPTY_LINK)
            {
                from->first = EMPTY_LINK;
                from->last = EMPTY_LINK;
            }
        
            assert(is_isolated(what, arr));
            assert(_check_is_connected(from->first, from->last, arr));
            
            return what;
        }

        static
        void insert_node(Index_Chain* to, u32 insert_after, u32 what, Bucket arr[]) noexcept
        {
            assert(what != EMPTY_LINK && "cannot be nullptr");
            assert(is_isolated(what, arr) && "must be isolated");
            assert(_check_is_connected(to->first, to->last, arr));

            if(to->first == EMPTY_LINK)
            {
                assert(insert_after == EMPTY_LINK);
                to->first = what;
                to->last = what;
                return;
            }

            //if is start of chain
            if(insert_after == EMPTY_LINK)
            {
                link_chain(EMPTY_LINK, what, what, to->first, arr);
                to->first = what;
            }
            //if is end of chain
            else if(insert_after == to->last)
            {
                link_chain(insert_after, what, what, EMPTY_LINK, arr);
                to->last = what;
            }
            else
            {
                link_chain(insert_after, what, what, arr[insert_after].next, arr);
            }
            
            assert(_check_is_connected(to->first, to->last, arr));
        }


    }

    // We use the most efficient strcuture for indexing which is just two ints 
    //  (i64 because we want to be general)
    // This however may be inefficient in terms of space as 99% of the time
    //  the number the number of elements will fit in i32. As such we provide 
    //  functions to convert from/to a compressed index into a Bucket_Index.
    // The interface for the compressed index is still isize (i64) but we can
    //  store it as anything we know will be enough for our number of elements.
    struct Bucket_Index
    {   
        isize bucket_i = -1;
        isize slot_i = -1;
    };

    inline
    Bucket_Index to_bucket_index(isize index, isize bucket_size)
    {
        assert(index > 0);

        //usize uindex = cast(usize) index;
        //usize bucket_size_mask = cast(usize) bucket_size - 1;

        //Bucket_Index out;
        //out.bucket_i = uindex & ~bucket_size_mask;
        //out.slot_i = uindex & bucket_size_mask;
        
        assert(index > 0 && "invalid index");

        Bucket_Index out = {};
        out.bucket_i = index / bucket_size;
        out.slot_i = index % bucket_size;

        return out;
    }
    
    inline
    isize from_bucket_index(Bucket_Index index, isize bucket_size)
    {
        assert(index.bucket_i > 0);
        assert(0 <= index.slot_i && index.slot_i < bucket_size);

        usize out = index.bucket_i * bucket_size + index.slot_i;
        return out;
    }
    
    namespace bucket_array_internal
    {
        struct Untyped_Bucket_Array
        {
            Stack<Bucket> _buckets = {}; //4
            Index_Chain _open_buckets = {}; //1
            isize _max_bucket_size = 0; //1
            isize _total_used = 0;      //1
            isize _total_capacity = 0;  //1

            Untyped_Bucket_Array(isize max_bucket_size = 256, memory_globals::Default_Alloc alloc = {}) noexcept 
                : _buckets(alloc.val), _max_bucket_size(max_bucket_size)
            {
                //assert(is_power_of_two(bucket_size) && "bucket size must be power of two!");
            }

            ~Untyped_Bucket_Array() noexcept
            {
                assert(_total_used == 0 && "not freed!");
            }
        };

        static_assert(sizeof(Untyped_Bucket_Array) <= 64, "must be max 64B for good performence");

        static constexpr isize USED_SLOTS_ALIGN = 16; //should be enough for all SIMD instructions
        
        static 
        void free_data(Untyped_Bucket_Array* bucket_array, isize slot_size, isize slots_align) noexcept
        {
            for(isize i = 0; i < size(bucket_array->_buckets); i++)
            {
                Bucket* bucket = &bucket_array->_buckets[i];
                assert(bucket->used_slots != nullptr && "shoudl be init");
                assert(bucket->data != nullptr && "shoudl be init");

                isize data_size = bucket->size * slot_size;
                isize used_slots_size = div_round_up(bucket->size, 64)*64;
                Slice<u8> data = {cast(u8*) bucket->data, data_size};
                Slice<u8> used_slots = {cast(u8*) bucket->used_slots, used_slots_size};
                    
                Allocator* alloc = bucket_array->_buckets._allocator;
                alloc->deallocate(data, slots_align);
                alloc->deallocate(used_slots, USED_SLOTS_ALIGN);
            }

            bucket_array->_total_used = 0;
        }

        #ifdef BUCKET_ARRAY_PEDANTIC
            #define BUCKET_ARRAY_DEFAULT_PEDANTIC_VALUE true
        #else
            #define BUCKET_ARRAY_DEFAULT_PEDANTIC_VALUE false
        #endif BUCKET_ARRAY_PEDANTIC
        
        #define _UC_COMPARE_CONST 8
        #define cmp_uc(a, cmp, b) ((a) / _UC_COMPARE_CONST) cmp ((b) / _UC_COMPARE_CONST)

        inline 
        bool is_invariant(Untyped_Bucket_Array const& bucket_array, bool pedantic = BUCKET_ARRAY_DEFAULT_PEDANTIC_VALUE) noexcept
        {
            Slice<const Bucket> arr = slice(bucket_array._buckets);
            u32 first = bucket_array._open_buckets.first;
            u32 last = bucket_array._open_buckets.last;

            bool is_total_size_plausible = bucket_array._total_used <= bucket_array._total_capacity;
            bool is_sorted = true;
            bool is_conencted = true;
            bool is_isolated = true;
            bool first_last_match = (first == EMPTY_LINK) == (last == EMPTY_LINK);
            bool total_size_matches = true;
            bool total_capacity_matches = true;
            
            const Bucket* first_bucket = nullptr;
            const Bucket* last_bucket = nullptr;

            if(first != EMPTY_LINK && last != EMPTY_LINK)
            {
                first_bucket = &arr[first];
                last_bucket = &arr[last];
            
                is_isolated = first_bucket->prev == EMPTY_LINK && last_bucket->next == EMPTY_LINK;
                assert(is_isolated);
            }
            
            if(pedantic)
            {
                u32 curr = first;
                u32 prev = EMPTY_LINK;

                while(curr != EMPTY_LINK && prev != last)
                {

                    if(prev != EMPTY_LINK)
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
                isize total_capacity = 0;
                for(isize i = 0; i < arr.size; i++)
                {
                    total_size += arr[i].used_count;
                    total_capacity += arr[i].size;
                }

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

        static 
        u32 add_bucket(Untyped_Bucket_Array* to, isize new_bucket_size, isize slot_size, isize slots_align) 
        {
            assert(new_bucket_size > 0 && slot_size > 0 && slots_align > 0);

            Allocator* alloc = to->_buckets._allocator;
            isize data_size = new_bucket_size * slot_size;
            isize used_slots_size = div_round_up(new_bucket_size, 64) * sizeof(u64);

            Allocation_Result data = alloc->allocate(data_size, slots_align);
            Allocation_Result used_slots = alloc->allocate(used_slots_size, USED_SLOTS_ALIGN);
            
            if(data.state != OK || used_slots.state != OK)
            {
                if(data.state == OK)
                    alloc->deallocate(data.items, slots_align);
                    
                if(used_slots.state == OK)
                    alloc->deallocate(used_slots.items, USED_SLOTS_ALIGN);

                panic("allocation failed!");
            }

            Bucket bucket;
            bucket.size = cast(u32) new_bucket_size;
            bucket.data = cast(void*) data.items.data;
            bucket.used_slots = cast(u64*) used_slots.items.data;
            null_items(used_slots.items);
            
            push(&to->_buckets, bucket);
            to->_total_capacity += new_bucket_size;

            return cast(u32) size(to->_buckets) - 1;
        }
        
        //@TODO: fix crashes of map on stress test when shrinking rehash (add rehash test to other tests 
        // then add rehash to size to test options
        //@TODO: speed up compilation and type erasure of everything
        //@TODO: more elaborated benchmark of iteration
        //@TODO: make separate stack header that contains the bare minimum
        //@TODO: make header for intrinsics - just some of them and just the ones we actually use
        //        this header should be absolutely free standing but still compatible with the lib similar to
        //        time, meta, cpu_id and others
        static 
        Bucket_Index prepare_for_insert(Untyped_Bucket_Array* buckets, isize new_bucket_size, isize slot_size, isize slots_align)
        {
            using namespace bucket_array_internal;

            u32 to_bucket_i = buckets->_open_buckets.first;
            if(to_bucket_i == EMPTY_LINK)
            {
                is_invariant(*buckets, true);
                to_bucket_i = add_bucket(buckets, new_bucket_size, slot_size, slots_align);
                insert_node(&buckets->_open_buckets, buckets->_open_buckets.last, to_bucket_i, data(&buckets->_buckets));
            }
            else
                is_invariant(*buckets);
        
            Bucket* to_bucket = &buckets->_buckets[to_bucket_i];
            assert(to_bucket->used_count < to_bucket->size && "should have a free slot");
            assert(to_bucket->prev == EMPTY_LINK && "should be first node");
        
            //Iterate over all bits of used_slots and when an empty space is found
            // insert val in place.
            isize found_index = -1;
            isize slot_block_size = div_round_up(to_bucket->size, 64);
            
            for(isize i = 0; i < slot_block_size; i++)
            {
                u64* block = &to_bucket->used_slots[i];
                size_t bit_pos = 0;
                bool was_found = intrin__find_first_set_64(&bit_pos, ~*block);

                if(was_found)
                {
                    u64 bit = cast(u64) 1 << bit_pos;
                    *block |= bit;
                    found_index = 64 * i + bit_pos;
                        
                    i = slot_block_size;
                    break;
                }
            }
            assert(found_index != -1 && "should have been found");
            assert(found_index < to_bucket->size && "has corrupted used_slots bits");

            //increment counters
            to_bucket->used_count += 1;
            buckets->_total_used += 1;
            assert(to_bucket->used_count <= to_bucket->size && "should not be overfull!");
        
            //if is completely full extract it from open buckets
            if(to_bucket->used_count == to_bucket->size)
            {
                extract_node(&buckets->_open_buckets, to_bucket->prev, to_bucket_i, data(&buckets->_buckets));
                is_invariant(*buckets, true);
            }

            //Assert local consistency
            assert(to_bucket->prev == EMPTY_LINK && "should be first node");
            if(to_bucket->next != EMPTY_LINK)
            {
                Bucket* next_bucket = &buckets->_buckets[to_bucket->next];
                assert(cmp_uc(next_bucket->used_count, <=, to_bucket->used_count) && "used_count's should be consistent");
            }

            Bucket_Index out = {0};
            out.bucket_i = to_bucket_i;
            out.slot_i = found_index;

            is_invariant(*buckets);

            return out;
        }

        void prepare_for_remove(Untyped_Bucket_Array* bucket_array, Bucket_Index index) noexcept
        {
            using namespace bucket_array_internal;
            is_invariant(*bucket_array);

            Slice<Bucket> buckets = slice(&bucket_array->_buckets);
            Bucket* bucket = &buckets[index.bucket_i];
            assert(0 <= index.slot_i && index.slot_i < bucket->size && "out of bounds!");

            //mark slot as free        
            isize slot_block_i = index.slot_i / 64;
            isize slot_bit_i = index.slot_i % 64;

            u64 bit = cast(u64) 1 << slot_bit_i;
            u64* block = &bucket->used_slots[slot_block_i];
            assert((*block & bit) > 0 && "provided index is invalid! was not previously allocated to!");
            *block &= ~bit; //sets the bit to 0;

            //decrease used_count and swap blocks if necessary
            bucket->used_count -= 1;
            bucket_array->_total_used -= 1;
            isize used_count = bucket->used_count;

            //if was full insert it to the start of the list
            if(bucket->used_count == bucket->size - 1)
            {
                insert_node(&bucket_array->_open_buckets, EMPTY_LINK, cast(u32) index.bucket_i, buckets.data);
                is_invariant(*bucket_array, true);
            }
            else if(bucket->next != EMPTY_LINK && cmp_uc(buckets[bucket->next].used_count, >, used_count))
            {
                u32 insert_after = bucket->next;
                while(true)
                {
                    Bucket* curr_bucket = &buckets[insert_after];
                    if(curr_bucket->next == EMPTY_LINK || cmp_uc(buckets[curr_bucket->next].used_count, <=, used_count))   
                        break;

                    insert_after = curr_bucket->next;
                }

                assert(insert_after != EMPTY_LINK && insert_after != index.bucket_i);
                extract_node(&bucket_array->_open_buckets, bucket->prev, cast(u32) index.bucket_i, buckets.data);
                insert_node(&bucket_array->_open_buckets, insert_after, cast(u32) index.bucket_i, buckets.data);
                is_invariant(*bucket_array, true);
            }
            else
            {
                is_invariant(*bucket_array);
            }
        }
    }

    template <typename T>
    struct Bucket_Array
    {
        bucket_array_internal::Untyped_Bucket_Array _contents = {};

        explicit Bucket_Array(isize max_bucket_size = 256, memory_globals::Default_Alloc alloc = {}) noexcept 
            : _contents(max_bucket_size, alloc)
        {}

        //@TODO: figure out how to type erase without too much run time overhead
        ~Bucket_Array()
        {
            is_invariant(_contents, true);
            using namespace bucket_array_internal;
            Slice<Bucket> buckets = slice(&_contents._buckets);
            //For each bucket scan through all of its active elements and call
            // a destructor on them
            for(isize i = 0; i < buckets.size; i++)
            {
                Bucket* bucket = &buckets[i];
                assert(bucket->used_slots != nullptr && "shoudl be init");
                assert(bucket->data != nullptr && "shoudl be init");

                Slice<T> items = {cast(T*) bucket->data, bucket->size};
                isize used_slots_size = div_round_up(bucket->size, 64);
                isize data_size = bucket->size * sizeof(T);

                //for each slot check if it is used and if so call destructor on it
                for(isize j = 0; j < used_slots_size; j++)
                {
                    //if is block empty skip
                    if(bucket->used_slots[j] == 0)
                        continue;

                    for(isize k = 0; k < 64; k++)
                    {
                        u64 bit = cast(u64) 1 << k;
                        if(bucket->used_slots[j] & bit)
                        {
                            T& item = items[j * 64 + k];
                            item.~T();
                        }
                    }
                }
                
                Slice<u8> data = {cast(u8*) bucket->data, bucket->size * cast(isize) sizeof(T)};
                Slice<u8> used_slots = {cast(u8*) bucket->used_slots, used_slots_size * cast(isize) sizeof(u64)};
                    
                Allocator* alloc = _contents._buckets._allocator;
                alloc->deallocate(data, alignof(T));
                alloc->deallocate(used_slots, USED_SLOTS_ALIGN);
            }

            _contents._total_used = 0;
        }
    };

    //@TODO: add define flag to trigger pedantic asserts that check:
    //  slot has used bit set
    template <typename T>
    T* get(Bucket_Array<T>* from, Bucket_Index index) noexcept
    {
        bucket_array_internal::Bucket* bucket = &from->_contents._buckets[index.bucket_i];
        T* bucket_data = cast(T*) bucket->data;
        
        assert(0 <= index.slot_i && index.slot_i < bucket->size && "out of bounds!");
        return &bucket_data[index.slot_i];
    }
    
    template <typename T>
    T const& get(Bucket_Array<T> const& from, Bucket_Index index) noexcept
    {
        //sorry c++ but I am not going to implement this two times just because of const
        Bucket_Array<T>* cheated = cast(Bucket_Array<T>*) cast(void*) &from;
        return *get(cheated, index);
    }

    template <typename T>
    isize size(Bucket_Array<T> const& bucket_array) noexcept
    {
        return bucket_array._contents._total_used;
    }

    template <typename T>
    isize capacity(Bucket_Array<T> const& bucket_array) noexcept
    {
        return bucket_array._contents._total_capacity;
    }

    template <typename T> nodisc
    Bucket_Index insert(Bucket_Array<T>* bucket_array, T val, isize new_bucket_size)
    {
        Bucket_Index loc = bucket_array_internal::prepare_for_insert(&bucket_array->_contents, new_bucket_size, sizeof(T), alignof(T));
        T* address = get(bucket_array, loc);

        new (address) T(move(&val));

        return loc;
    }

    template <typename T>
    T remove(Bucket_Array<T>* bucket_array, Bucket_Index index) noexcept
    {
        bucket_array_internal::prepare_for_remove(&bucket_array->_contents, index);
        T* address = get(bucket_array, index);

        T val = move(address);
        address->~T();

        return val;
    }
}

#include "jot/undefs.h"