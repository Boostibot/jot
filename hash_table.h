#pragma once

#include <string.h>

#include "memory.h"
//#include "slice.h"
//#include "panic.h"

namespace jot
{   
    namespace hash_table_globals
    {
        inline uint64_t* seed_ptr()
        {
            static uint64_t hash = 0;
            return &hash;
        }
    }

    template <typename T>
    bool default_key_equals(T const& a, T const& b)
    {
        return a == b;
    }

    template<typename Key> using Equal_Fn = bool   (*)(Key const&, Key const&);
    template<typename Key> using Hash_Fn  = uint64_t (*)(Key const&, uint64_t seed);
    
    //Cache efficient packed hash table. Stores the jump table, keys and values all in seperate arrays for maximum cache utilization.
    // This also allows us to expose the values array directly to the user which removes the need for custom iterators. 
    // Further we can also construct/decosntruct the hash table by transfering to/from it two Stacks. One for values and other for keys.
    // Because it doesnt have explicit links between keys with the same hash has to only ever delete entries in the jump table by marking 
    // them as deleted. After sufficient ammount of deleted entries rehashing is triggered (exactly the same ways as while adding) which 
    // only then properly removes the deleted jump table entries.
    template<class Key_, class Value_, Hash_Fn<Key_> hash, Equal_Fn<Key_> equals = default_key_equals<Key_>>
    struct Hash_Table
    {
        using Key = Key_;
        using Value = Value_;
        
        Allocator* _allocator = memory_globals::default_allocator();
        Key* _keys = nullptr;
        Value* _values = nullptr;
        uint32_t* _linker = nullptr;
        
        uint32_t _linker_size = 0;
        uint32_t _entries_size = 0;
        uint32_t _entries_capacity = 0;

        //the count of gravestones in linker array + the count of unreferenced entries
        // when too large triggers rehash to same size (cleaning)
        uint32_t _gravestone_count = 0; 
        
        uint32_t _hash_collisions = 0; //The count of hash colisions currently in the table. Multiplicit keys are counted into this
        uint32_t _max_hash_collisions = 0;
        uint64_t _seed = *hash_table_globals::seed_ptr(); //The current set seed. Can be changed during rehash

        //@NOTE: we can implement multi hash table or properly support deletion if we added extra array
        //   called _group containg for each entry a next and prev entry index (as u32). That way we 
        //   could upon deletion go to back or prev and change the _linker index to point to either of them

        Hash_Table() noexcept {};
        Hash_Table(Allocator* alloc, uint64_t seed = *hash_table_globals::seed_ptr()) noexcept 
            : _allocator(alloc), _seed(seed) {}
        Hash_Table(Hash_Table&& other) noexcept;
        Hash_Table(Hash_Table const& other) = delete;
        ~Hash_Table() noexcept;

        Hash_Table& operator=(Hash_Table&& other) noexcept;
        Hash_Table& operator=(Hash_Table const& other) = delete;
    };
    
    struct Hash_Found
    {
        isize hash_index;
        isize entry_index;
    };

    template<class Key, class Value>
    struct Hash_Table_Entry
    {
        Key key;
        Value value;
    };

    struct Hash_Table_Growth
    {
        //at what occupation of the jump table is rehash triggered
        uint8_t rehash_at_fullness_num = 1;
        uint8_t rehash_at_fullness_den = 4;

        //at what occupation of the jump table with gravestones
        // is rehash to same size triggered 
        // (if normal rehash happens first all gravestones are cleared)
        uint8_t rehash_at_gravestone_fullness_num = 1;
        uint8_t rehash_at_gravestone_fullness_den = 4;

        //the ratio entries size grows when full according to formula:
        // new_size = old_size * entries_growth + entries_growth_linear
        uint8_t entries_growth_num = 3;
        uint8_t entries_growth_den = 2;
        uint16_t entries_growth_linear = 8;

        //The size of the first allocation of jump table
        //Needs to be exact power of two
        uint16_t jump_table_base_size = 32;
    };
    
    template<class _T>
    struct _Id {using T = _T;};

    template<class T>
    using Id = typename _Id<T>::T;

    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    Slice<const Key> keys(Hash_Table<Key, Value, hash, equals> const& table)
    {
        return {table._keys, (isize) table._entries_size};
    }
    
    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    Slice<const Key> keys(Hash_Table<Key, Value, hash, equals>* table)
    {
        return {table->_keys, (isize) table->_entries_size};
    }

    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    Slice<Value> values(Hash_Table<Key, Value, hash, equals>* table)
    {
        return {table->_values, (isize) table->_entries_size};
    }
    
    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    Slice<const Value> values(Hash_Table<Key, Value, hash, equals> const& table)
    {
        return {table._values, (isize) table._entries_size};
    }

    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    isize jump_table_size(Hash_Table<Key, Value, hash, equals> const& table)
    {
        return table._linker_size;
    }
    
    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    isize size(Hash_Table<Key, Value, hash, equals> const& table)
    {
        return table._entries_size;
    }

    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    isize hash_collisions(Hash_Table<Key, Value, hash, equals> const& table)
    {
        return table._hash_collisions;
    }

    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    isize max_hash_collisions(Hash_Table<Key, Value, hash, equals> const& table)
    {
        return table._max_hash_collisions;
    }
    
    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    uint64_t seed(Hash_Table<Key, Value, hash, equals> const& table)
    {
        return table._max_hash_collisions;
    }

    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    void swap(Hash_Table<Key, Value, hash, equals>* left, Hash_Table<Key, Value, hash, equals>* right) noexcept
    {
        swap(&left->_allocator, &right->_allocator);
        swap(&left->_keys, &right->_keys);
        swap(&left->_values, &right->_values);
        swap(&left->_linker, &right->_linker);
        swap(&left->_entries_size, &right->_entries_size);
        swap(&left->_entries_capacity, &right->_entries_capacity);
        swap(&left->_gravestone_count, &right->_gravestone_count);
    }

    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    Hash_Table<Key, Value, hash, equals>::Hash_Table(Hash_Table && other) noexcept 
    {
        swap(this, &other);
    }

    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    bool is_invariant(Hash_Table<Key, Value, hash, equals> const& table) noexcept
    {
        bool is_size_power = table._linker_size == 0;
        if(is_size_power == false)
            is_size_power = is_power_of_two(table._linker_size);

        bool is_alloc_not_null = table._allocator != nullptr;
        bool are_entries_simulatinous_alloced = (table._keys == nullptr) == (table._values == nullptr);
        bool are_entry_sizes_correct = (table._keys == nullptr) == (table._entries_capacity == 0);
        bool are_linker_sizes_correct = (table._linker == nullptr) == (table._linker_size == 0);

        bool are_sizes_in_range = table._entries_size <= table._entries_capacity;

        bool res = is_size_power && is_alloc_not_null && are_entries_simulatinous_alloced 
            && are_entry_sizes_correct && are_linker_sizes_correct && are_sizes_in_range;

        assert(res);
        return res;
    }

    namespace hash_table_internal
    {
        constexpr uint32_t EMPTY_LINK = (uint32_t) -1;
        constexpr uint32_t GRAVESTONE_LINK = (uint32_t) -2;
        constexpr isize HASH_TABLE_LINKER_ALIGN = 8;
        constexpr isize HASH_TABLE_LINKER_BASE_SIZE = 16;
        
        template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
        bool set_entries_capacity(Hash_Table<Key, Value, hash, equals>* table, isize new_capacity) noexcept
        {
            assert(is_invariant(*table));

            Allocator* alloc = table->_allocator;
            isize capa = table->_entries_capacity;
            isize key_size = (isize) sizeof(Key);
            isize value_size = (isize) sizeof(Value);

            void* new_keys = nullptr;
            void* new_values = nullptr;
            
            bool state1 = memory_resize_allocate(alloc, &new_keys, new_capacity*key_size, table->_keys, capa*key_size, DEF_ALIGNMENT<Key>, GET_LINE_INFO());
            bool state2 = memory_resize_allocate(alloc, &new_values, new_capacity*value_size, table->_values, capa*value_size, DEF_ALIGNMENT<Value>, GET_LINE_INFO());

            if(!state1 || !state2)
            {
                memory_resize_undo(alloc, &new_keys,   new_capacity*key_size,   table->_keys,   capa*key_size, DEF_ALIGNMENT<Key>, GET_LINE_INFO());
                memory_resize_undo(alloc, &new_values, new_capacity*value_size, table->_values, capa*value_size, DEF_ALIGNMENT<Value>, GET_LINE_INFO());
            }

            //destruct extra
            for(isize i = new_capacity; i < table->_entries_size; i++)
                table->_keys[i].~Key();

            for(isize i = new_capacity; i < table->_entries_size; i++)
                table->_values[i].~Value();

            //@NOTE: We assume reallocatble ie. that the object helds in each slice
            // do not depend on their own adress => can be freely moved around in memory
            // => we can simply memove without calling moves and destructors (see slice_ops.h)
            //assert(JOT_IS_REALLOCATABLE(T) && "we assume reallocatble!");
            size_t new_size = (size_t) min(new_capacity, table->_entries_size);

            if(new_keys != table->_keys)        memmove(new_keys, table->_keys, new_size*sizeof(Key));
            if(new_values != table->_values)    memmove(new_values, table->_values, new_size*sizeof(Value));
        
            memory_resize_deallocate(alloc, &new_keys,   new_capacity*key_size,   table->_keys,   capa*key_size, DEF_ALIGNMENT<Key>, GET_LINE_INFO());
            memory_resize_deallocate(alloc, &new_values, new_capacity*value_size, table->_values, capa*value_size, DEF_ALIGNMENT<Value>, GET_LINE_INFO());

            table->_entries_size = (uint32_t) new_size;
            table->_entries_capacity = (uint32_t) new_capacity;
            table->_keys = (Key*) new_keys;
            table->_values = (Value*) new_values;
        
            assert(is_invariant(*table));
            return true;
        }
    
        template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
        bool unsafe_rehash(Hash_Table<Key, Value, hash, equals>* table, isize to_size, uint64_t seed) noexcept
        {
            assert(is_invariant(*table));

            #ifndef NDEBUG
            isize required_min_size = div_round_up(table->_entries_size, (isize) sizeof(uint32_t));
            bool is_shrinking_ammount_allowed = to_size >= required_min_size && to_size >= table->_entries_size;
            bool is_resulting_size_allowed = is_power_of_two(to_size);

            assert(is_shrinking_ammount_allowed && is_resulting_size_allowed && 
                "must be big enough (no more shrinking than by factor of 4 at a time) and power of two");
            #endif

            isize alloc_size = to_size * (isize) sizeof(uint32_t);
            void* allocation_result = table->_allocator->allocate(alloc_size, HASH_TABLE_LINKER_ALIGN, GET_LINE_INFO());
            if(allocation_result == nullptr)
                return false; 

            //mark occurences of each entry index in a bool array
            Slice<bool> marks = {(bool*) allocation_result, table->_entries_size};
            for(isize i = 0; i < marks.size; i++)
                marks.data[i] = false;

            #ifndef NDEBUG
            isize empty_count = 0;
            isize graveston_count = 0;
            #endif
            isize alive_count = 0;
            
            Slice<uint32_t> old_linker = {table->_linker, table->_linker_size};
            for(isize i = 0; i < old_linker.size; i++)
            {
                #ifndef NDEBUG
                if(old_linker[i] == EMPTY_LINK)
                    empty_count ++;
                else if(old_linker[i] == GRAVESTONE_LINK)
                    graveston_count ++;
                #endif
                
                uint32_t link = old_linker[i];

                //skip gravestones and empty slots
                if(link >= table->_entries_capacity)
                    continue;
                    
                alive_count ++;
                assert(marks[link] == false && "all links must be unique!");
                marks[link] = true;
            }

            assert(empty_count + graveston_count + alive_count == table->_linker_size);
            assert(graveston_count <= table->_gravestone_count);

            //iterate the marks from front and back 
            // swapping mark from the back into holes from front
            //  (we dont actually need to swap marks so we skip it)
            // swapping the entries in entry array in the same time
            // the final position of the forward index is the filtered size
            //this is the optimal way to remove any dead entries while 
            // moving as little entries as possible
            isize forward_index = 0;
            isize backward_index = marks.size - 1;
                
            while(forward_index <= backward_index)
            {
                while(forward_index < marks.size && marks[forward_index] == true)
                    forward_index++;

                while(backward_index >= 0 && marks[backward_index] == false)
                    backward_index--;
                    
                if(forward_index >= backward_index)
                    break;

                table->_keys[forward_index] = move(&table->_keys[backward_index]);
                table->_values[forward_index] = move(&table->_values[backward_index]);

                swap(&marks[forward_index], &marks[backward_index]);

                forward_index ++;
                backward_index--;
            }

            //fill new_linker to empty
            Slice<uint32_t> new_linker = {(uint32_t*) allocation_result, to_size};
            for(isize i = 0; i < new_linker.size; i++)
                new_linker[i] = EMPTY_LINK;

            //rehash every entry up to alive_count
            uint64_t mask = (uint64_t) new_linker.size - 1;
            uint32_t hash_collision_count = 0;

            assert(alive_count <= new_linker.size && "there must be enough size to fit all entries");
            for(isize entry_index = 0; entry_index < alive_count; entry_index++)
            {
                uint64_t hashed = hash(table->_keys[entry_index], seed);
                uint64_t slot = hashed & mask;

                //if linker slot is taken iterate until we find an empty slot
                for(isize passed = 0;; slot = (slot + 1) & mask, passed ++)
                {
                    uint32_t link = new_linker[(isize) slot];
                    if(link == hash_table_internal::EMPTY_LINK)
                        break;

                    assert(passed < new_linker.size && 
                        "there must be enough size to fit all entries"
                        "this assert should never occur");
                }

                //if was not placed in it originalindex => hash collision occured
                if(slot != (hashed & mask))
                    hash_collision_count += 1;

                new_linker[(isize) slot] = (uint32_t) entry_index;
            }

            //destroy the dead entries
            for(isize i = alive_count; i < table->_entries_size; i++)
            {
                table->_keys[i].~Key();
                table->_values[i].~Value();
            }
            
            table->_hash_collisions = hash_collision_count;
            table->_max_hash_collisions = (uint32_t) max(table->_max_hash_collisions, table->_hash_collisions);
            table->_seed = seed;
            table->_gravestone_count = 0;
            table->_entries_size = (uint32_t) alive_count;
            table->_linker = new_linker.data;
            table->_linker_size = (uint32_t) to_size;

            if(old_linker.size != 0)
            {
                isize dealloc_size = old_linker.size * (isize) sizeof(uint32_t);
                table->_allocator->deallocate(old_linker.data, dealloc_size, HASH_TABLE_LINKER_ALIGN, GET_LINE_INFO());
            }
            
            assert(is_invariant(*table));

            return true;
        }

        template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals> NODISCARD
        void panic_out_of_memory(Hash_Table<Key, Value, hash, equals> const& table, Line_Info info, isize requested, const char* on_op)
        {
            const char* alloc_name = table._allocator->get_stats().name; 
            memory_globals::out_of_memory_hadler()(info, "Hash_Table<T> memory allocation failed! "
                "Attempted to allocated %t bytes from allocator %p name %s while doing an action: %s ",
                requested, table._allocator, alloc_name ? alloc_name : "<No alloc name>", on_op);
        }
    }
    
    template <class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    Hash_Found find(Hash_Table<Key, Value, hash, equals> const& table, Id<Key> const& key, uint64_t hashed) noexcept
    {
        assert(is_invariant(table));
        Hash_Found found = {};
        found.hash_index = -1;
        found.entry_index = -1;

        if(table._linker_size == 0)
            return found;

        uint64_t mask = (uint64_t) table._linker_size - 1;
        uint64_t i = hashed & mask;
        for(isize passed = 0;; i = (i + 1) & mask, passed ++)
        {
            uint32_t link = table._linker[(isize) i];
            if(link == hash_table_internal::EMPTY_LINK || passed > table._linker_size)
                break;

            if(link == hash_table_internal::GRAVESTONE_LINK)
                continue;

            assert(link < table._entries_size);
            if(equals(table._keys[link], key))
            {
                found.hash_index = (isize) i;
                found.entry_index = link;
                break;
            }
        }

        return found;
    }

    template <class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    Hash_Found find_found_entry(Hash_Table<Key, Value, hash, equals> const& table, isize entry_i, uint64_t hashed) noexcept
    {
        assert(is_invariant(table));

        Hash_Found found = {};
        found.hash_index = -1;
        found.entry_index = -1;

        if(table._linker_size == 0)
            return found;

        uint64_t mask = (uint64_t) table._linker_size - 1;
        uint64_t i = hashed & mask;
        for(isize passed = 0;; i = (i + 1) & mask, passed ++)
        {
            uint32_t link = table._linker[(isize) i];
            if(link == (uint32_t) hash_table_internal::EMPTY_LINK || passed > table._linker_size)
                break;

            if(link == (uint32_t) entry_i)
            {
                found.hash_index = (isize) i;
                found.entry_index = link;
                break;
            }
        }

        return found;
    }
    
    
    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    void mark_removed(Hash_Table<Key, Value, hash, equals>* table, Hash_Found removed)
    {
        assert(0 <= removed.hash_index && removed.hash_index < table->_linker_size && "out of range!");

        table->_linker[removed.hash_index] = (uint32_t) hash_table_internal::GRAVESTONE_LINK;
        table->_gravestone_count += 2; //one for the link and one for the entry 
        //=> when only marking entries we will rehash faster then when removing them
    }

    //Removes an entry from the keys and values array and marks the jump table slot as deleted
    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    Hash_Table_Entry<Key, Value> remove(Hash_Table<Key, Value, hash, equals>* table, Hash_Found removed)
    {
        assert(0 <= removed.hash_index && removed.hash_index < table->_linker_size && "out of range!");
        assert(0 <= removed.entry_index && removed.entry_index < table->_entries_size && "out of range!");
        assert(table->_entries_size > 0 && "cannot remove from empty");

        isize last = table->_entries_size - 1;
        isize removed_i = removed.entry_index;

        Slice<uint32_t> linker  = {table->_linker, table->_linker_size};
        Slice<Key> keys         = {table->_keys,   table->_entries_size};
        Slice<Value> values     = {table->_values, table->_entries_size};

        linker[removed.hash_index] = (uint32_t) hash_table_internal::GRAVESTONE_LINK;
        table->_gravestone_count += 1;
        
        Hash_Table_Entry<Key, Value> removed_entry_data = {
            move(&keys[removed_i]),
            move(&values[removed_i]),
        };

        bool delete_last = true;
        if(removed_i != last)
        {
            uint64_t last_hash = hash(keys[last], table->_seed);
            Hash_Found changed_for = find_found_entry(*table, last, last_hash);

            //in the case the table contains 'mark_removed' entries
            // we can no longer assure that the last key will actually exists
            // in that case we simply only mark the entry as deleted and nothing more
            // (we could scan back for other not removed entries to swap with but that has no use since
            //  the user cannot make any assumptions about which entries are used and and which ones 
            //  arent when using mark_removed)
            if(changed_for.hash_index != -1)
            {
                linker[changed_for.hash_index] = (uint32_t) removed_i;
                keys[removed_i] = move(&keys[last]);
                values[removed_i] = move(&values[last]);
            }
            else
                delete_last = false;
        }

        if(delete_last)
        {
            keys[last].~Key();
            values[last].~Value();
        
            table->_entries_size -= 1;
        }

            
        return removed_entry_data;
    }

    //Only marks the jump table slot as deleted but keeps the key value entries in their place.
    // These marked but not removed entries will get cleaned up during the next rehashin in an optimal way.
    // When deleting large number of entries it is better to use this function than `remove` on every single
    // entry individually
    //Returns index of marked entry
    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    isize mark_removed(Hash_Table<Key, Value, hash, equals>* table, Id<Key> const& key)
    {
        Hash_Found found = find(*table, key);
        if(found.entry_index == -1)
            return found.entry_index;

        (void) mark_removed(table, found);
        return found.entry_index;
    }

    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals> 
    bool remove(Hash_Table<Key, Value, hash, equals>* table, Id<Key> const& key)
    {
        Hash_Found found = find(*table, key);
        if(found.entry_index == -1)
            return false;

        (void) remove(table, found);
        return true;
    }
    
    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals> NODISCARD
    bool reserve_entries_failing(Hash_Table<Key, Value, hash, equals>* table, isize to_fit, Hash_Table_Growth growth = {}) noexcept
    {
        if(to_fit <= table->_entries_capacity)
            return true;
            
        assert(is_invariant(*table));
        isize new_capacity = table->_entries_capacity;
        while(new_capacity < to_fit)
            new_capacity = new_capacity * growth.entries_growth_num/growth.entries_growth_den + growth.entries_growth_linear; 
            
        return hash_table_internal::set_entries_capacity(table, new_capacity);
    }

    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals> NODISCARD
    bool rehash_failing(Hash_Table<Key, Value, hash, equals>* table, isize to_size, uint64_t seed, Hash_Table_Growth growth = {}) noexcept
    {
        isize rehash_to = growth.jump_table_base_size;

        isize required_min_size = div_round_up(table->_entries_size, (isize) sizeof(uint32_t)); //cannot shrink below this
        isize normed = max(to_size, required_min_size);
        normed = max(normed, table->_entries_size);
        while(rehash_to < normed)
            rehash_to *= 2; //must be power of two and this is the simplest way of ensuring it

        return hash_table_internal::unsafe_rehash(table, rehash_to, seed);
    }
    
    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    void rehash(Hash_Table<Key, Value, hash, equals>* table, isize to_size, uint64_t seed, Hash_Table_Growth growth = {})
    {
        if(rehash_failing(table, to_size, seed, growth) == false)
            hash_table_internal::panic_out_of_memory(*table, GET_LINE_INFO(), to_size * (isize) sizeof(uint32_t), "rehash");
    }

    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals> 
    void rehash(Hash_Table<Key, Value, hash, equals>* table, Hash_Table_Growth growth = {})
    {
        rehash(table, table->_linker_size, table->_seed, growth);
    }
    
    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    void reserve(Hash_Table<Key, Value, hash, equals>* table, isize to_fit, Hash_Table_Growth growth = {})
    {
        //entries_size * growth.rehash_at_fullness_den / growth.rehash_at_fullness_num
        isize jump_table_size = to_fit * growth.rehash_at_fullness_den / growth.rehash_at_fullness_num;
        if(jump_table_size > table->_linker_size)
            rehash(table, jump_table_size, table->_seed, growth);

        if(reserve_entries_failing(table, to_fit, growth) == false)
            hash_table_internal::panic_out_of_memory(*table, GET_LINE_INFO(), to_fit * (isize) sizeof(Key), "reserve");
    }


    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    Hash_Table<Key, Value, hash, equals>::~Hash_Table() noexcept 
    {
        assert(is_invariant(*this));
        hash_table_internal::set_entries_capacity(this, 0);

        if(_linker != nullptr)
        {
            isize dealloc_size = _linker_size * (isize) sizeof(uint32_t);
            _allocator->deallocate(_linker, dealloc_size, hash_table_internal::HASH_TABLE_LINKER_ALIGN, GET_LINE_INFO());
        }
    }
    
    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    Hash_Found find(Hash_Table<Key, Value, hash, equals> const& table, Id<Key> const& key) noexcept
    {
        uint64_t hashed = hash(key, table._seed);
        return find(table, key, hashed);
    }
    
    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    bool has(Hash_Table<Key, Value, hash, equals> const& table, Id<Key> const& key) noexcept
    {
        return find(table, key).entry_index != -1;
    }

    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    Value const& get(Hash_Table<Key, Value, hash, equals> const&  table, Id<Key> const& key, Id<Value> const& if_not_found) noexcept
    {
        isize index = find(table, key).entry_index;
        if(index == -1)
            return if_not_found;

        return values(table)[index];
    }
    
    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    void grow_if_overfull(Hash_Table<Key, Value, hash, equals>* table, Hash_Table_Growth growth = {}) 
    {
        assert(is_invariant(*table));

        if((table->_linker_size - table->_gravestone_count) * growth.rehash_at_fullness_num <= 
            table->_entries_size * growth.rehash_at_fullness_den)
        {
            isize rehash_to = table->_linker_size * 2;
            assert(growth.rehash_at_gravestone_fullness_den > growth.rehash_at_gravestone_fullness_num && "growth must be less than 1!");
            assert(growth.rehash_at_gravestone_fullness_den > 0 && growth.rehash_at_gravestone_fullness_num > 0 && "growth must be positive");

            //if too many gravestones keeps the same size onky clears out the grabage
            // ( gravestones / size >= MAX_NUM / MAX_DEN )
            if(table->_gravestone_count * growth.rehash_at_gravestone_fullness_den >= table->_linker_size * growth.rehash_at_gravestone_fullness_num)
                rehash_to = table->_linker_size;
            
            //If zero go to base size
            if(rehash_to == 0)
                rehash_to = growth.jump_table_base_size;

            if(hash_table_internal::unsafe_rehash(table, rehash_to, table->_seed) == false)
                hash_table_internal::panic_out_of_memory(*table, GET_LINE_INFO(), rehash_to * (isize) sizeof(uint32_t), "grow_if_overfull");
        }
    }
    
    namespace hash_table_internal
    {
        template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
        void push_new(Hash_Table<Key, Value, hash, equals>* table, Key key, Value value, uint64_t original_hash_index, uint64_t insert_at_index, Hash_Table_Growth growth)
        {
            assert(is_invariant(*table));
            assert(insert_at_index < table->_linker_size && "must be within array!");

            //if finished on a gravestone we can overwrite it and thus remove it 
            if(table->_linker[insert_at_index] == hash_table_internal::GRAVESTONE_LINK)
            {
                assert(table->_gravestone_count > 0);
                table->_gravestone_count -= 1;
            }
            //If index moved then at least one hash colision occured
            //(if finished on a gravestone link we didnt *create* a new hash colision we just
            //put an entry into a place of previous hash colision)
            else if(original_hash_index != insert_at_index)
            {
                assert(table->_linker[insert_at_index] == hash_table_internal::EMPTY_LINK && "should be empty at this point");
                table->_hash_collisions += 1;
                table->_max_hash_collisions = (uint32_t) max(table->_max_hash_collisions, table->_hash_collisions);
            }

            isize size = (isize) table->_entries_size;
            if(reserve_entries_failing(table, size + 1, growth) == false)
                hash_table_internal::panic_out_of_memory(*table, GET_LINE_INFO(), (size + 1) * (isize) sizeof(Key), "push_new");

            new (&table->_keys[size]) Key(move(&key));
            new (&table->_values[size]) Value(move(&value));

            table->_entries_size += 1;
            table->_linker[insert_at_index] = (uint32_t) table->_entries_size - 1;
        
            assert(is_invariant(*table));
        }
    }
    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    isize set(Hash_Table<Key, Value, hash, equals>* table, Id<Key> key, Id<Value> value, Hash_Table_Growth growth = {})
    {
        grow_if_overfull(table, growth);
        assert(table->_linker_size != 0);

        uint64_t mask = (uint64_t) table->_linker_size - 1;
        uint64_t start_i = hash(key, table->_seed) & mask;
        uint64_t i = start_i;
        for(isize passed = 0;; i = (i + 1) & mask, passed ++)
        {
            assert(passed < table->_linker_size);

            uint32_t link = table->_linker[(isize) i];
            if(link == hash_table_internal::EMPTY_LINK || link == hash_table_internal::GRAVESTONE_LINK)
                break;

            if(equals(table->_keys[link], key))
            {
                values(table)[link] = move(&value);
                return i;
            }
        }

        hash_table_internal::push_new(table, move(&key), move(&value), start_i, i, growth);
        return table->_entries_size;
    }
    
    namespace multi
    {
        template <class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
        Hash_Found find_next(Hash_Table<Key, Value, hash, equals> const& table, Id<Key> const& prev_key, Hash_Found prev) noexcept
        {
            assert(prev.hash_index != -1 && prev.entry_index != -1 && "must be found!");
            return find(table, prev_key, (uint64_t) prev.hash_index + 1);
        }

        template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
        isize add_another(Hash_Table<Key, Value, hash, equals>* table, Id<Key> key, Id<Value> value, Hash_Table_Growth growth = {})
        {
            assert(is_invariant(*table));
            grow_if_overfull(table, growth);
            
            assert(table->_linker_size > 0);
            uint64_t mask = (uint64_t) table->_linker_size - 1;
            uint64_t start_i = hash(key, table->_seed) & mask;
            uint64_t i = start_i;
            for(isize passed = 0;; i = (i + 1) & mask, passed ++)
            {
                assert(passed < table->_linker_size && "should never make a full rotation!");
                uint32_t link = table->_linker[i];
                if(link == hash_table_internal::EMPTY_LINK || link == hash_table_internal::GRAVESTONE_LINK)
                    break;
            }

            hash_table_internal::push_new(table, move(&key), move(&value), start_i, i, growth);
            return table->_entries_size;
        }
    }
}