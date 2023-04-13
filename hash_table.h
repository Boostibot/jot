#pragma once

#include "memory.h"
#include <ctime>

#ifndef NODISCARD
    #define NODISCARD [[nodiscard]]
#endif

namespace jot
{   
    template<typename Key> using Equal_Fn = bool   (*)(Key const&, Key const&);
    template<typename Key> using Hash_Fn  = uint64_t (*)(Key const&, uint64_t seed);

    template<bool cond, typename A, typename B>
    struct Conditional_Struct { using type = A; };
    
    template<typename A, typename B>
    struct Conditional_Struct<false, A, B> { using type = B; };
    
    template<bool cond, typename A, typename B>
    using Conditional = typename Conditional_Struct<cond, A, B>::type;

    namespace hash_table_globals
    {
        uint64_t* seed_ptr()
        {
            static uint64_t hash = (uint64_t) clock();
            return &hash;
        }
        
        uint64_t seed() 
        {
            return *seed_ptr(); 
        }

        void set_seed(uint64_t seed) 
        {
            *seed_ptr() = seed; 
        }
    }

    template <typename T>
    bool default_key_equals(T const& a, T const& b)
    {
        return a == b;
    }

    //Cache efficient packed hash table. Stores the jump table, keys and values all in seperate arrays for maximum cache utilization.
    // This also allows us to expose the values array directly to the user which removes the need for custom iterators. 
    // Further we can also construct/decosntruct the hash table by transfering to/from it two Stacks. One for values and other for keys.
    // Because it doesnt have explicit links between keys with the same hash has to only ever delete entries in the jump table by marking 
    // them as deleted. After sufficient ammount of deleted entries rehashing is triggered (exactly the same ways as while adding) which 
    // only then properly removes the deleted jump table entries.
    template<class Key_, class Value_, Hash_Fn<Key_> hash_, Equal_Fn<Key_> equals_ = default_key_equals<Key_>>
    struct Hash_Table
    {
        using Key = Key_;
        using Value = Value_;

        static constexpr Hash_Fn<Key>   hash = hash_;
        static constexpr Equal_Fn<Key>  equals = equals_;

        //we assume that if the entry size is equal or greater than 8 (which is usually the case) 
        // we wont ever hold more than 4 million entries (would be more than 32GB of data)
        // this reduces the necessary size for linker by 50%
        using Link = Conditional<(sizeof(Key) + sizeof(Value) > 4), uint64_t, uint32_t>;
        
        Allocator* _allocator = memory_globals::default_allocator();
        Key* _keys = nullptr;
        Value* _values = nullptr;
        Slice<Link> _linker = {nullptr, 0};
        
        Link _entries_size = 0;
        Link _entries_capacity = 0;

        //the count of gravestones in linker array + the count of unreferenced entries
        // when too large triggers rehash to same size (cleaning)
        Link _gravestone_count = 0; 
        
        //The count of hash colisions currently in the table. Does properly hold for multihash as well.
        Link _hash_collisions = 0;

        uint64_t _seed = hash_table_globals::seed();

        //@NOTE: we can implement multi hash table or properly support deletion if we added extra array
        //   called _group containg for each entry a next and prev entry index (as u32). That way we 
        //   could upon deletion go to back or prev and change the _linker index to point to either of them

        Hash_Table() noexcept = default;
        Hash_Table(Allocator* alloc, uint64_t seed = hash_table_globals::seed()) noexcept 
            : _allocator(alloc), _seed(seed) {}
        Hash_Table(Hash_Table&& other) noexcept;
        Hash_Table(Hash_Table const& other) = delete;
        ~Hash_Table() noexcept;

        Hash_Table& operator=(Hash_Table&& other) noexcept;
        Hash_Table& operator=(Hash_Table const& other) = delete;
    };
    
    //We will use these macros often to save (quite a lot) screen space
    #define Hash_Table_T       Hash_Table<Key, Value, hash, equals>
    #define Hash_Key           typename Hash_Table<Key, Value, hash, equals>::Key
    #define Hash_Value         typename Hash_Table<Key, Value, hash, equals>::Value
    #define Hash_Link          typename Hash_Table<Key, Value, hash, equals>::Link
    
    struct Hash_Table_Growth
    {
        //at what occupation of the jump table is rehash triggered
        int32_t rehash_at_fullness_num = 1;
        int32_t rehash_at_fullness_den = 4;

        //at what occupation of the jump table with gravestones
        // is rehash to same size triggered 
        // (if normal rehash happens first all gravestones are cleared)
        int32_t rehash_at_gravestone_fullness_num = 1;
        int32_t rehash_at_gravestone_fullness_den = 4;

        //the ratio entries size grows when full according to formula:
        // new_size = old_size * entries_growth + entries_growth_linear
        int32_t entries_growth_num = 3;
        int32_t entries_growth_den = 2;
        int32_t entries_growth_linear = 8;

        //The size of the first allocation of jump table
        //Needs to be exact power of two
        int32_t jump_table_base_size = 32;
    };

    constexpr static isize HASH_TABLE_ENTRIES_ALIGN = 8;
    constexpr static isize HASH_TABLE_LINKER_ALIGN = 8;
    constexpr static isize HASH_TABLE_LINKER_BASE_SIZE = 16;

    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    bool is_invariant(Hash_Table_T const& table) noexcept
    {
        bool is_size_power = table._linker.size == 0;
        if(is_size_power == false)
            is_size_power = is_power_of_two(table._linker.size);

        bool is_alloc_not_null = table._allocator != nullptr;
        bool are_entries_simulatinous_alloced = (table._keys == nullptr) == (table._values == nullptr);
        bool are_entry_sizes_correct = (table._keys == nullptr) == (table._entries_capacity == 0);
        bool are_linker_sizes_correct = (table._linker.data == nullptr) == (table._linker.size == 0);

        bool are_sizes_in_range = table._entries_size <= table._entries_capacity;

        bool res = is_size_power && is_alloc_not_null && are_entries_simulatinous_alloced 
            && are_entry_sizes_correct && are_linker_sizes_correct && are_sizes_in_range;

        assert(res);
        return res;
    }

    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    Slice<const Key> keys(Hash_Table_T const& table)
    {
        return {table._keys, table._entries_size};
    }
    
    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    Slice<const Key> keys(Hash_Table_T* table)
    {
        return {table->_keys, table->_entries_size};
    }

    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    Slice<Value> values(Hash_Table_T* table)
    {
        return {table->_values, table->_entries_size};
    }
    
    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    Slice<const Value> values(Hash_Table_T const& table)
    {
        return {table._values, table._entries_size};
    }

    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    isize jump_table_size(Hash_Table_T const& table)
    {
        return table._linker.size;
    }

    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    void swap(Hash_Table_T* left, Hash_Table_T* right) noexcept
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
    Hash_Table_T::Hash_Table(Hash_Table && other) noexcept 
    {
        assert(is_invariant(*this));
        assert(is_invariant(other));

        swap(this, &other);
    }

    struct Hash_Found
    {
        isize hash_index;
        isize entry_index;
        isize finished_at;
    };
    
    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals> NODISCARD
    Allocation_State reserve_entries_failing(Hash_Table_T* table, isize to_fit, Hash_Table_Growth const& growth) noexcept
    {
        if(to_fit <= table->_entries_capacity)
            return Allocation_State::OK;

        assert(is_invariant(*table));
        isize new_capacity = calculate_stack_growth(table->_entries_capacity, to_fit, 
            growth.entries_growth_num, growth.entries_growth_den, growth.entries_growth_linear);

        Slice<uint8_t> new_keys;
        Slice<uint8_t> new_values;

        Allocator* alloc = table->_allocator;
        Allocation_State key_state = alloc->allocate(&new_keys, new_capacity * sizeof(Key), HASH_TABLE_ENTRIES_ALIGN);
        if(key_state != Allocation_State::OK)
            return key_state;

        Allocation_State value_state = alloc->allocate(&new_values, new_capacity * sizeof(Value), HASH_TABLE_ENTRIES_ALIGN);
        if(key_state != Allocation_State::OK)
        {
            alloc->deallocate(new_keys, HASH_TABLE_ENTRIES_ALIGN);
            return key_state;
        }
            
        Slice<uint8_t> old_keys = cast_slice<uint8_t>(Slice<Key>{table->_keys, table->_entries_capacity});
        Slice<uint8_t> old_values = cast_slice<uint8_t>(Slice<Value>{table->_values, table->_entries_capacity});

        //@NOTE: We assume transferable ie. that the object helds in each slice
        // do not depend on their own adress => can be freely moved around in memory
        // => we can simply memove without calling moves and destructors (see slice_ops.h)
        memmove(new_keys.data, old_keys.data, old_keys.size);
        memmove(new_keys.data, old_keys.data, old_keys.size);

        alloc->deallocate(old_keys, HASH_TABLE_ENTRIES_ALIGN);
        alloc->deallocate(old_values, HASH_TABLE_ENTRIES_ALIGN);

        table->_entries_capacity = new_capacity;
        table->_keys = (Key*) new_keys.data;
        table->_values = (Value*) new_values.data;
        
        assert(is_invariant(*table));
        return Allocation_State::OK;
    }

    namespace hash_table_internal
    {
        constexpr isize EMPTY_LINK = -1;
        constexpr isize GRAVESTONE_LINK = -2;
    
        template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
        Allocation_State unsafe_rehash(Hash_Table_T* table, isize to_size) noexcept
        {
            assert(is_invariant(*table));
            using Link = Hash_Link;

            isize required_min_size = div_round_up(table->_entries_size, sizeof(Link));
            bool is_shrinking_ammount_allowed = to_size >= required_min_size && to_size >= table->_entries_size;
            bool is_resulting_size_allowed = is_power_of_two(to_size) || to_size == 0;

            assert(is_shrinking_ammount_allowed && is_resulting_size_allowed && 
                "must be big enough (no more shrinking than by 4 at a time) and power of two");

            to_size = max(to_size, required_min_size);

            Slice<Link> old_linker = table->_linker;
            Slice<uint8_t> result_items;
            
            //if to_size == 0 we only deallocate
            if(to_size != 0)
            {
                Allocation_State state = table->_allocator->allocate(to_size * (isize) sizeof(Link), HASH_TABLE_LINKER_ALIGN);
                if(state != Allocation_State::OK)
                   return state; 
            }

            //mark occurences of each entry index in a bool array
            Slice<bool> marks = head(cast_slice<bool>(result_items), table->_entries_size);
            null_items(marks);

            isize empty_count = 0;
            isize graveston_count = 0;
            isize alive_count = 0;

            for(isize i = 0; i < old_linker.size; i++)
            {
                #ifndef NDEBUG
                if(old_linker[i] == (Link) EMPTY_LINK)
                    empty_count ++;
                else if(old_linker[i] == (Link) GRAVESTONE_LINK)
                    graveston_count ++;
                #endif
                
                isize link = (isize) old_linker[i];

                //skip gravestones and empty slots
                if(link >= table->_entries_capacity)
                    continue;
                    
                alive_count ++;
                assert(marks[link] == false && "all links must be unique!");
                marks[link] = true;
            }

            assert(empty_count + graveston_count + alive_count == table->_linker.size);
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
            isize swap_count = 0;
                
            while(forward_index <= backward_index)
            {
                while(marks[forward_index] == true)
                {
                    forward_index++;
                    if(forward_index >= marks.size)
                        break;
                }

                while(marks[backward_index] == false)
                {
                    backward_index--;
                    if(backward_index < 0)
                        break;
                }
                    
                if(forward_index >= backward_index)
                    break;

                table->_keys[forward_index] = move(&table->_keys[backward_index]);
                table->_values[forward_index] = move(&table->_values[backward_index]);

                bool temp = marks[forward_index];
                marks[forward_index] = marks[backward_index];
                marks[backward_index] = temp;

                swap_count ++;
                forward_index ++;
                backward_index--;
            }

            //fill new_linker to empty
            Slice<Link> new_linker = cast_slice<Link>(result_items);
            for(isize i = 0; i < new_linker.size; i++)
                new_linker[i] = (Link) EMPTY_LINK;

            //rehash every entry up to alive_count
            uint64_t mask = (uint64_t) new_linker.size - 1;

            assert(alive_count <= new_linker.size && "there must be enough size to fit all entries");
            for(isize entry_index = 0; entry_index < alive_count; entry_index++)
            {
                Key const& stored_key = table->_keys[entry_index];
                uint64_t hashed = hash(stored_key, table->_seed);
                uint64_t slot = hashed & mask;

                //if linker slot is taken iterate until we find an empty slot
                for(isize passed = 0;; slot = (slot + 1) & mask, passed ++)
                {
                    Link link = new_linker[(isize) slot];
                    if(link == (Link) hash_table_internal::EMPTY_LINK)
                        break;

                    assert(passed < new_linker.size && 
                        "there must be enough size to fit all entries"
                        "this assert should never occur");
                }

                new_linker[(isize) slot] = (Link) entry_index;
            }

            //destroy the dead entries
            for(isize i = alive_count; i < table->_entries_size; i++)
            {
                table->_keys[i].~Key();
                table->_values[i].~Value();
            }
            
            assert(table->_entries_size <= alive_count + table->_gravestone_count);

            table->_gravestone_count = 0;
            table->_entries_size = alive_count;
            table->_linker = new_linker;
            table->_allocator->deallocate(cast_slice<uint8_t>(old_linker), HASH_TABLE_LINKER_ALIGN);
            
            assert(is_invariant(*table));

            return Allocation_State::OK;
        }

        template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
        Allocation_State grow(Hash_Table_T* table, Hash_Table_Growth const& growth) noexcept
        {
            isize rehash_to = table->_linker.size * 2;

            //if too many gravestones keeps the same size onky clears out the grabage
            // ( gravestones / size >= MAX_NUM / MAX_DEN )
            if(table->_gravestone_count * growth.rehash_at_gravestone_fullness_den >= table->_linker.size * growth.rehash_at_gravestone_fullness_num)
                rehash_to = table->_linker.size;
            
            //If zero go to base size
            if(rehash_to == 0)
                rehash_to = growth.jump_table_base_size;

            return unsafe_rehash(table, rehash_to);
        }
    }
    
    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    Allocation_State rehash_failing(Hash_Table_T* table, isize to_size, Hash_Table_Growth const& growth = {}) noexcept
    {
        isize rehash_to = growth.jump_table_base_size;

        isize required_min_size = div_round_up(table->_entries_size, sizeof(Hash_Link)); //cannot shrink below this
        isize normed = max(to_size, required_min_size);
        normed = max(normed, table->_entries_size);
        while(rehash_to < normed)
            rehash_to *= 2; //must be power of two and this is the simplest way of ensuring it

        return hash_table_internal::unsafe_rehash(table, rehash_to);
    }

    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals> NODISCARD
    Allocation_State reserve_jump_table_failing(Hash_Table_T* table, isize to_fit, Hash_Table_Growth const& growth = {}) noexcept
    {
        if(to_fit < table->_linker.size)
            return Allocation_State::OK;

        return rehash_failing(table, to_fit, growth);
    }

    constexpr inline 
    isize calculate_jump_table_size(isize entries_size, Hash_Table_Growth const& growth = {}) noexcept
    {
        return entries_size * growth.rehash_at_fullness_den / growth.rehash_at_fullness_num;
    }

    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals> NODISCARD
    Allocation_State reserve_failing(Hash_Table_T* table, isize to_fit, Hash_Table_Growth const& growth = {}) noexcept
    {
        //we reserve such that inserting to_fit element swill not result in a reallocation of entries nor rehashing
        isize jump_table_size = calculate_jump_table_size(to_fit, growth);
        Allocation_State state1 = reserve_jump_table_failing(table, jump_table_size, growth);
        if(state1 != Allocation_State::OK)
            return state1;

        Allocation_State state2 = reserve_entries_failing(table, to_fit, growth);
        return state2;
    }

    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    void reserve_entries(Hash_Table_T* table, isize to_fit, Hash_Table_Growth const& growth = {})
    {
        Allocation_State state = reserve_entries_failing(table, to_fit, growth);
        force(state == Allocation_State::OK && "reserve failed!");
    }
    
    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    void reserve_jump_table(Hash_Table_T* table, isize to_fit, Hash_Table_Growth const& growth = {})
    {
        Allocation_State state = reserve_jump_table_failing(table, to_fit, growth);
        force(state == Allocation_State::OK && "reserve failed!");
    }
    
    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    void rehash(Hash_Table_T* table, isize to_size, Hash_Table_Growth const& growth = {})
    {
        Allocation_State state = rehash_failing(table, to_size, growth);
        force(state == Allocation_State::OK && "rehashing failed!");
    }

    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals> 
    void rehash(Hash_Table_T* table, Hash_Table_Growth const& growth = {})
    {
        isize rehash_to = jump_table_size(*table);
        rehash(table, rehash_to, growth);
    }
    
    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    void reserve(Hash_Table_T* table, isize to_fit, Hash_Table_Growth const& growth = {})
    {
        Allocation_State state = reserve_failing(table, to_fit, growth);
        force(state == Allocation_State::OK && "reserve failed!");
    }

    template <class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    Hash_Found find(Hash_Table_T const& table, Hash_Key const& key, uint64_t hashed, bool break_on_gravestone = false) noexcept
    {
        using Link = Hash_Link;

        assert(is_invariant(table));
        Hash_Found found = {};
        found.hash_index = -1;
        found.entry_index = -1;
        found.finished_at = -1;

        if(table._linker.size == 0)
            return found;

        uint64_t mask = (uint64_t) table._linker.size - 1;

        Slice<const Link> links = table._linker;
        Slice<const Key> keys = jot::keys(table);

        uint64_t i = hashed & mask;
        for(isize passed = 0;; i = (i + 1) & mask, passed ++)
        {
            Link link = links[(isize) i];
            if(link == (Link) hash_table_internal::EMPTY_LINK || passed > links.size)
                break;
        
            if(link == (Link) hash_table_internal::GRAVESTONE_LINK)
            {
                if(break_on_gravestone)
                    break;

                continue;
            }

            Key const& curr = keys[(isize) link];
            if(equals(curr, key))
            {
                found.hash_index = (isize) i;
                found.entry_index = link;
                break;
            }
        }

        found.finished_at = (isize) i;
        return found;
    }
   
    template <class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    Hash_Found find_found_entry(Hash_Table_T const& table, isize entry_i, uint64_t hashed, bool break_on_gravestone = false) noexcept
    {
        using Link = Hash_Link;
        assert(is_invariant(table));

        Hash_Found found = {};
        found.hash_index = -1;
        found.entry_index = -1;
        found.finished_at = -1;

        if(table._linker.size == 0)
            return found;

        uint64_t mask = (uint64_t) table._linker.size - 1;

        Slice<const Link> links = table._linker;

        uint64_t i = hashed & mask;
        for(isize passed = 0;; i = (i + 1) & mask, passed ++)
        {
            Link link = links[(isize) i];
            if(link == (Link) hash_table_internal::EMPTY_LINK || passed > table._linker.size)
                break;
        
            if(break_on_gravestone && link == (Link) hash_table_internal::GRAVESTONE_LINK)
                break;

            if(link == (Link) entry_i)
            {
                found.hash_index = (isize) i;
                found.entry_index = link;
                break;
            }
        }

        found.finished_at = (isize) i;
        return found;
    }
    
    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    Hash_Found find(Hash_Table_T const& table, Hash_Key const& key, bool break_on_gravestone = false) noexcept
    {
        uint64_t hash = hash(key, table->_seed);
        return find(table, key, hash);
    }
    
    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    bool has(Hash_Table_T const& table, Hash_Key const& key) noexcept
    {
        return find(table, key).entry_index != -1;
    }
    
    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    void mark_removed(Hash_Table_T* table, Hash_Found removed)
    {
        assert(0 <= removed.hash_index && removed.hash_index < table->_linker.size && "out of range!");

        table->_linker[removed.hash_index] = (Hash_Link) hash_table_internal::GRAVESTONE_LINK;
        table->_gravestone_count += 2; //one for the link and one for the entry 
        //=> when only marking entries we will rehash faster then when removing them
    }

    //Removes an entry from the keys and values array and marks the jump table slot as deleted
    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    typename Hash_Table_T::Entry remove(Hash_Table_T* table, Hash_Found removed)
    {
        assert(0 <= removed.hash_index && removed.hash_index < table->_linker.size && "out of range!");
        assert(0 <= removed.entry_index && removed.entry_index < table->_entries_size && "out of range!");
        assert(table->_entries_size > 0 && "cannot remove from empty");

        isize last = table->_entries_size - 1;
        isize removed_i = removed.entry_index;

        using Link = Hash_Link;

        Slice<Link> linker      = table->_linker;
        Slice<Key> keys  = {table->_keys,    table->_entries_size};
        Slice<Value> values     = {table->_values,  table->_entries_size};

        linker[removed.hash_index] = (Link) hash_table_internal::GRAVESTONE_LINK;
        table->_gravestone_count += 1;
        
        typename Hash_Table_T::Entry removed_entry_data = {
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
                linker[changed_for.hash_index] = (Link) removed_i;
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
    isize mark_removed(Hash_Table_T* table, Hash_Key const& key)
    {
        Hash_Found found = find(*table, key);
        if(found.entry_index == -1)
            return found.entry_index;

        (void) mark_removed(table, found);
        return found.entry_index;
    }

    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals> 
    bool remove(Hash_Table_T* table, Hash_Key const& key)
    {
        Hash_Found found = find(*table, key);
        if(found.entry_index == -1)
            return false;

        (void) remove(table, found);
        return true;
    }
    
    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    Hash_Table_T::~Hash_Table() noexcept 
    {
        assert(is_invariant(*this));
        (void) hash_table_internal::set_entries_capacity(this, (isize) 0);

        if(this->_linker.data != nullptr)
        {
            Slice<uint8_t> bytes = cast_slice<uint8_t>(this->_linker);
            this->_allocator->deallocate(bytes, HASH_TABLE_LINKER_ALIGN);
        }
    }

    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    Value const& get(Hash_Table_T const&  table, Hash_Key const& key, Hash_Value const& if_not_found) noexcept
    {
        isize index = find(table, key).entry_index;
        if(index == -1)
            return if_not_found;

        return values(table)[index];
    }
    
    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    void grow_if_overfull(Hash_Table_T* table, Hash_Table_Growth const& growth = {}) 
    {
        assert(is_invariant(*table));
        if((table->_linker.size - table->_gravestone_count) * growth.rehash_at_fullness.num <= 
            table->_entries_size * growth.rehash_at_fullness.den)
        {
            Allocation_State state = hash_table_internal::grow(table, growth);
            force(state == Allocation_State::OK && "allocation failed!");
        }
    }
    
    namespace hash_table_internal
    {
        template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
        void push_new(Hash_Table_T* table, Hash_Key key, Hash_Value value, Hash_Found at, Hash_Table_Growth const& growth)
        {
            assert(is_invariant(*table));
            assert(at.entry_index == -1 && "the entry must not be found!");
            assert(at.finished_at < table->_linker.size && "should be empty or gravestone at this point");

            //if finished on a gravestone we can overwrite it and thus remove it 
            if(at.finished_at == (Hash_Link) hash_table_internal::GRAVESTONE_LINK)
            {
                assert(table->_gravestone_count > 0);
                table->_gravestone_count -= 1;
            }
        
            isize size = table->_entries_size;
            reserve_entries(table, size + 1, growth);

            new (&table->_keys[size]) Key(move(&key));
            new (&table->_values[size]) Value(move(&value));

            table->_entries_size++;
            table->_linker[at.finished_at] = (Hash_Link) table->_entries_size - 1;
        
            assert(is_invariant(*table));
        }
    }
    template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
    void set(Hash_Table_T* table, Hash_Key key, Hash_Value value, Hash_Table_Growth const& growth = {})
    {
        grow_if_overfull(table, growth);

        Hash_Found found = find(*table, key, true);;
        if(found.entry_index != -1)
        {
            values(table)[found.entry_index] = move(&value);
            return;
        }

        return hash_table_internal::push_new(table, move(&key), move(&value), found, growth);
    }
    
    namespace multi
    {
        template <class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
        Hash_Found find_next(Hash_Table_T const& table, Hash_Key const& prev_key, Hash_Found prev, 
                bool break_on_gravestone = false) noexcept
        {
            assert(prev.hash_index != -1 && "must be found!");
            assert(prev.entry_index != -1 && "must be found!");

            return find(table, prev_key, (uint64_t) prev.hash_index + 1, break_on_gravestone);
        }

        template<class Key, class Value, Hash_Fn<Key> hash, Equal_Fn<Key> equals>
        void add_another(Hash_Table_T* table, Hash_Key key, Hash_Value value, Hash_Table_Growth const& growth = {})
        {
            grow_if_overfull(table, growth);
            
            //@TODO: make a specific function for this type of find.
            Hash_Found found = find(*table, key, true);
            while(found.entry_index != -1)
                found = find_next(*table, key, found, true);
        
            return hash_table_internal::push_new(table, move(&key), move(&value), found, growth);
        }
    }

    #undef Hash_Table_T           
    #undef Hash_Key         
    #undef Hash_Value    
    #undef Hash_Link         
}