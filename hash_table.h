#pragma once

#include "memory.h"
#include "hash.h"
#include "stack.h"
#include "defines.h"

namespace jot
{
    template <typename T, typename Enable = Enabled>
    struct Hashable : No_Default
    {
        static 
        hash_t hash(T const& val) noexcept
        {
            //just for illustartion:
            return uint64_hash(val);
        }
    };
    
    template <typename T, typename Enable = Enabled>
    struct Key_Comparable
    {
        static
        bool are_equal(T const& a, T const& b) noexcept
        {
            return a == b;
        }
    };

    template <typename From, typename To>
    struct Key_Castable
    {
        static 
        To key_cast(From const& from)
        {
            return cast(To const&) from;
        }
    };

    
    struct Ratio
    {
        isize num = 0;
        isize den = 0;
    };
    
    struct Hash_Table_Growth
    {
        //at what occupation of the jump table is rehash triggered
        Ratio rehash_at_fullness = {1, 4};

        //at what occupation of the jump table with gravestones
        // is rehash to same size triggered 
        // (if normal rehash happens first all gravestones are cleared)
        Ratio rehash_at_gravestone_fullness = {1, 4};

        //the ratio entries size grows when full according to formula:
        // new_size = old_size * entries_growth + entries_growth_linear
        Ratio entries_growth = {3, 2};
        isize entries_growth_linear = 8;

        //The size of the first allocation of jump table
        //Needs to be exact power of two
        isize jump_table_base_size = 32;
    };

    template<
        typename Stored_Key_, 
        typename Value_> 
    struct Hash_Table_Info
    {
        using Key = Stored_Key_;
        using Value = Value_;
        using Stored_Key = Stored_Key_;
        using Cast = Key_Castable<Stored_Key, Key>;
        using Compare = Key_Comparable<Key>;
        using Hash = Hashable<Key>;
    };
    
    template<typename Stored_Key, typename Value>
    struct Hash_Table_Entry
    {
        Stored_Key key;
        Value value;
    };

    //Cache efficient packed hash table. Stores the jump table, keys and values all in seperate arrays for maximum cache utilization.
    // This also allows us to expose the values array directly to the user which removes the need for custom iterators. 
    // Further we can also construct/decosntruct the hash table by transfering to/from it two Stacks. One for values and other for keys.
    // Because it doesnt have explicit links between keys with the same hash has to only ever delete entries in the jump table by marking 
    // them as deleted. After sufficient ammount of deleted entries rehashing is triggered (exactly the same ways as while adding) which 
    // only then properly removes the deleted jump table entries.
    template<class Info_>
    struct Hash_Table
    {
        using Info = Info_;

        //We will use these macros often to save (quite a lot) screen space
        #define Info_Key           typename Info::Key
        #define Info_Value         typename Info::Value
        #define Info_Stored_Key    typename Info::Stored_Key
        #define Info_Cast          typename Info::Cast
        #define Info_Compare       typename Info::Compare
        #define Info_Hash          typename Info::Hash
        #define Info_Link          typename Hash_Table<Info>::Link

        using Key = Info_Key;
        using Value = Info_Value;
        using Stored_Key = Info_Stored_Key;
        using Cast = Info_Cast;
        using Compare = Info_Compare;
        using Hash = Info_Hash;
        
        using Entry = Hash_Table_Entry<Stored_Key, Value>;

        //we assume that if the entry size is equal or greater than 8 (which is usually the case) 
        // we wont ever hold more than 4 million entries (would be more than 32GB of data)
        // this reduces the necessary size for linker by 50%
        using Link = std::conditional_t<(sizeof(Entry) >= 8), u32, u64>;
        

        Allocator* _allocator = memory_globals::default_allocator();
        Stored_Key* _keys = nullptr;
        Value* _values = nullptr;
        Slice<Link> _linker = {nullptr, 0};
        
        isize _entries_size = 0;
        isize _entries_capacity = 0;
        isize _gravestone_count = 0; //the count of gravestones in linker array + the count of unreferenced entries
        // when too large triggers cleaning rehash

        //@NOTE: we *could* use Stack to hold keys and Stack to hold values since 
        // we effectively use one. That would howeever cost us extra 3*8 redundant bytes 
        // which would mean this structure would no longer fit into 64B cache line and thus 
        // requiring more than one memory read. This could very negatively impact performance.
        
        //@NOTE: we can implement multi hash table or properly support deletion if we added extra array
        //   called _group containg for each entry a next and prev entry index (as u32). That way we 
        //   could upon deletion go to back or prev and change the _linker index to point to either of them

        Hash_Table() noexcept = default;
        Hash_Table(Allocator* alloc) noexcept : _allocator(alloc) {}
        Hash_Table(Hash_Table&& other) noexcept;
        Hash_Table(Hash_Table const& other) = delete;
        ~Hash_Table() noexcept;

        Hash_Table& operator=(Hash_Table&& other) noexcept;
        Hash_Table& operator=(Hash_Table const& other) = delete;
    };

    template<class Info>
    using Enable_If_Keys_Differ = Enable_If<!std::is_same_v<Info_Key, Info_Stored_Key>>;

    constexpr static isize HASH_TABLE_ENTRIES_ALIGN = 32;
    constexpr static isize HASH_TABLE_LINKER_ALIGN = 32;
    constexpr static isize HASH_TABLE_LINKER_BASE_SIZE = 16;

    //rehashes to same size if gravestones make more than
    // HASH_TABLE_MAX_GRAVESTONES.num / HASH_TABLE_MAX_GRAVESTONES.den portion
    constexpr static Ratio HASH_TABLE_MAX_GRAVESTONES = {1, 4}; 
    constexpr static Ratio HASH_TABLE_MAX_UTILIZATION = {1, 4}; 

    template <class Info> nodisc
    bool is_invariant(Hash_Table<Info> const& hash)
    {
        bool is_size_power = hash._linker.size == 0;
        if(is_size_power == false)
            is_size_power = is_power_of_two(hash._linker.size);

        bool is_alloc_not_null = hash._allocator != nullptr;
        bool are_entries_simulatinous_alloced = (hash._keys == nullptr) == (hash._values == nullptr);
        bool are_entry_sizes_correct = (hash._keys == nullptr) == (hash._entries_capacity == 0);
        bool are_linker_sizes_correct = (hash._linker.data == nullptr) == (hash._linker.size == 0);

        bool are_sizes_in_range = hash._entries_size <= hash._entries_capacity;

        bool res = is_size_power && is_alloc_not_null && are_entries_simulatinous_alloced 
            && are_entry_sizes_correct && are_linker_sizes_correct && are_sizes_in_range;

        assert(res);
        return res;
    }

    template <class Info> nodisc
    Slice<const Info_Stored_Key> keys(Hash_Table<Info> const& hash)
    {
        Slice<const Info_Stored_Key> out = {hash._keys, hash._entries_size};
        return out;
    }
    
    template <class Info> nodisc
    Slice<const Info_Stored_Key> keys(Hash_Table<Info>* hash)
    {
        return keys(*hash);
    }

    template <class Info> nodisc
    Slice<Info_Value> values(Hash_Table<Info>* hash)
    {
        Slice<Info_Value> out = {hash->_values, hash->_entries_size};
        return out;
    }
    
    template <class Info> nodisc
    Slice<const Info_Value> values(Hash_Table<Info> const& hash)
    {
        Slice<const Info_Value> out = {hash._values, hash._entries_size};
        return out;
    }

    template <class Info> nodisc
    isize jump_table_size(Hash_Table<Info> const& hash)
    {
        return hash._linker.size;
    }

    template <class Info>
    void swap(Hash_Table<Info>* left, Hash_Table<Info>* right) noexcept
    {
        swap(&left->_allocator, &right->_allocator);
        swap(&left->_keys, &right->_keys);
        swap(&left->_values, &right->_values);
        swap(&left->_linker, &right->_linker);
        swap(&left->_entries_size, &right->_entries_size);
        swap(&left->_entries_capacity, &right->_entries_capacity);
        swap(&left->_gravestone_count, &right->_gravestone_count);
    }

    template <class Info>
    Hash_Table<Info>::Hash_Table(Hash_Table && other) noexcept 
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

    namespace hash_table_internal
    {
        constexpr isize EMPTY_LINK = -1;
        constexpr isize GRAVESTONE_LINK = -2;
        
        template <class Info>
        hash_t do_hash(Info_Stored_Key const& stored_key)
        {
            if constexpr(std::is_same_v<Info_Key, Info_Stored_Key>)
                return Info::Hash::hash(stored_key);
            else
            {
                Info_Key key = Info::Cast::key_cast(stored_key);
                return Info::Hash::hash(key);
            }
        }

        template <class Info> nodisc
        Allocator_State_Type set_entries_capacity(Hash_Table<Info>* hash, isize new_capacity) noexcept
        {
            Allocator* alloc = hash->_allocator;
            isize align = HASH_TABLE_ENTRIES_ALIGN;
            isize size = hash->_entries_size;

            using Stored_Key = Info_Stored_Key;
            using Value = Info_Value;

            Slice<Stored_Key> old_keys = Slice<Stored_Key>{hash->_keys, hash->_entries_capacity};
            Slice<Value> old_values = Slice<Value>{hash->_values, hash->_entries_capacity};

            Set_Capacity_Result<Stored_Key> key_res = set_capacity_allocation_stage(alloc, &old_keys, align, new_capacity, true);
            if(key_res.state == ERROR)
                return key_res.state;
                
            Set_Capacity_Result<Value> value_res = set_capacity_allocation_stage(alloc, &old_values, align, new_capacity, true);
            if(value_res.state == ERROR)
            {
                Slice<u8> raw_keys = cast_slice<u8>(key_res.items);
                alloc->deallocate(raw_keys, align);

                return value_res.state;
            }

            set_capacity_deallocation_stage(alloc, &old_keys, size, align, &key_res);
            set_capacity_deallocation_stage(alloc, &old_values, size, align, &value_res);

            hash->_keys = key_res.items.data;
            hash->_values = value_res.items.data;
            hash->_entries_capacity = new_capacity;

            return Allocator_State::OK;
        }

        template <class Info> nodisc
        Allocator_State_Type reserve_entries(Hash_Table<Info>* hash, isize to_fit, Hash_Table_Growth const& growth) noexcept
        {
            if(to_fit > hash->_entries_capacity)
            {
                isize new_capacity = calculate_stack_growth(hash->_entries_capacity, to_fit, 
                    growth.entries_growth.num, growth.entries_growth.den, growth.entries_growth_linear);

                return set_entries_capacity(hash, new_capacity);
            }

            return Allocator_State::OK;
        }

        template <class Info> nodisc
        Allocator_State_Type push_entry(Hash_Table<Info>* hash, Info_Stored_Key key, Info_Value value, Hash_Table_Growth const& growth) noexcept
        {
            assert(is_invariant(*hash));
            isize size = hash->_entries_size;
            Allocator_State_Type state = hash_table_internal::reserve_entries(hash, size + 1, growth);
            if(state == ERROR)
                return state;

            new (&hash->_keys[size]) Info_Stored_Key(move(&key));
            new (&hash->_values[size]) Info_Value(move(&value));

            hash->_entries_size = size + 1;
            
            assert(is_invariant(*hash));
            return Allocator_State::OK;
        }
    
        template <class Info> nodisc
        Allocator_State_Type unsafe_rehash(Hash_Table<Info>* table, isize to_size) noexcept
        {
            assert(is_invariant(*table));
            using Link = Info_Link;;
            using Stored_Key = Info_Stored_Key;
            using Value = Info_Value;

            isize required_min_size = div_round_up(table->_entries_size, sizeof(Link));
            bool is_shrinking_ammount_allowed = to_size >= required_min_size;
            bool is_resulting_size_allowed = is_power_of_two(to_size) || to_size == 0;

            assert(is_shrinking_ammount_allowed && is_resulting_size_allowed && "must be big enough (no more shrinking than by 4 at a time) and power of two");

            to_size = max(to_size, required_min_size);

            Slice<Link> old_linker = table->_linker;
            Allocation_Result result = {};
            
            //if to_size == 0 we only deallocate
            if(to_size != 0)
            {
                result = table->_allocator->allocate(to_size * cast(isize) sizeof(Link), HASH_TABLE_LINKER_ALIGN);
                if(result.state != Allocator_State::OK)
                   return result.state; 
            }

            //mark occurences of each entry index in a bool array
            Slice<bool> marks = trim(cast_slice<bool>(result.items), table->_entries_size);
            null_items<bool>(&marks);

            isize empty_count = 0;
            isize graveston_count = 0;
            isize alive_count = 0;

            for(isize i = 0; i < old_linker.size; i++)
            {
                #ifndef NDEBUG
                if(old_linker[i] == cast(Link) EMPTY_LINK)
                    empty_count ++;
                else if(old_linker[i] == cast(Link) GRAVESTONE_LINK)
                    graveston_count ++;
                #endif
                
                isize link = cast(isize) old_linker[i];

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
            Slice<Link> new_linker = cast_slice<Link>(result.items);
            fill(&new_linker, cast(Link) EMPTY_LINK);

            //rehash every entry up to alive_count
            hash_t mask = cast(hash_t) new_linker.size - 1;

            assert(alive_count <= new_linker.size && "there must be enough size to fit all entries");
            for(isize entry_index = 0; entry_index < alive_count; entry_index++)
            {
                Stored_Key const& stored_key = table->_keys[entry_index];
                hash_t hash = do_hash<Info>(stored_key);
                hash_t slot = hash & mask;

                //if linker slot is taken iterate until we find an empty slot
                for(isize passed = 0;; slot = (slot + 1) & mask, passed ++)
                {
                    Link link = new_linker[cast(isize) slot];
                    if(link == cast(Link) hash_table_internal::EMPTY_LINK)
                        break;

                    assert(passed < new_linker.size && 
                        "there must be enough size to fit all entries"
                        "this assert should never occur");
                }

                new_linker[cast(isize) slot] = cast(Link) entry_index;
            }

            //destroy the dead entries
            for(isize i = alive_count; i < table->_entries_size; i++)
            {
                table->_keys[i].~Stored_Key();
                table->_values[i].~Value();
            }
            
            assert(table->_entries_size <= alive_count + table->_gravestone_count);

            table->_gravestone_count = 0;
            table->_entries_size = alive_count;
            table->_linker = new_linker;
            table->_allocator->deallocate(cast_slice<u8>(old_linker), HASH_TABLE_LINKER_ALIGN);
            
            assert(is_invariant(*table));

            return Allocator_State::OK;
        }

        template <class Info> nodisc
        bool is_overful(Hash_Table<Info> const& table, Hash_Table_Growth const& growth) noexcept
        {
            return (table._linker.size - table._gravestone_count) * growth.rehash_at_fullness.num <= table._entries_size * growth.rehash_at_fullness.den;
        }
        
        template <class Info> nodisc
        Allocator_State_Type grow(Hash_Table<Info>* table, Hash_Table_Growth const& growth) noexcept
        {
            isize rehash_to = table->_linker.size * 2;

            //if too many gravestones keeps the same size onky clears out the grabage
            // ( gravestones / size >= MAX_NUM / MAX_DEN )
            if(table->_gravestone_count * growth.rehash_at_gravestone_fullness.den >= table->_linker.size * growth.rehash_at_gravestone_fullness.num)
                rehash_to = table->_linker.size;
            
            //If zero go to base size
            if(rehash_to == 0)
                rehash_to = growth.jump_table_base_size;

            return unsafe_rehash(table, rehash_to);
        }
    }
    
    template <class Info> nodisc
    Allocator_State_Type rehash_failing(Hash_Table<Info>* table, isize to_size, Hash_Table_Growth const& growth = {}) noexcept
    {
        isize rehash_to = growth.jump_table_base_size;

        isize required_min_size = div_round_up(table->_entries_size, sizeof(Info_Link)); //cannot shrink below this
        isize normed = max(to_size, required_min_size);
        while(rehash_to < normed)
            rehash_to *= 2; //must be power of two and this is the simplest way of ensuring it

        return hash_table_internal::unsafe_rehash(table, rehash_to);
    }

    template <class Info> nodisc
    Allocator_State_Type reserve_jump_table_failing(Hash_Table<Info>* table, isize to_fit, Hash_Table_Growth const& growth = {}) noexcept
    {
        if(to_fit < table->_linker.size)
            return Allocator_State::OK;

        return rehash_failing(table, to_fit, growth);
    }

    template <class Info> nodisc
    Allocator_State_Type reserve_entries_failing(Hash_Table<Info>* table, isize to_fit, Hash_Table_Growth const& growth = {}) noexcept
    {
        return hash_table_internal::reserve_entries(table, to_fit, growth);
    }

    constexpr inline 
    isize calculate_jump_table_size(isize entries_size, Hash_Table_Growth const& growth = {}) noexcept
    {
        return entries_size * growth.rehash_at_fullness.den / growth.rehash_at_fullness.num;
    }

    template <class Info> nodisc
    Allocator_State_Type reserve_failing(Hash_Table<Info>* table, isize to_fit, Hash_Table_Growth const& growth = {}) noexcept
    {
        //we reserve such that inserting to_fit element swill not result in a reallocation of entries nor rehashing
        isize jump_table_size = calculate_jump_table_size(to_fit, growth);
        Allocator_State_Type state1 = reserve_jump_table_failing(table, jump_table_size, growth);
        Allocator_State_Type state2 = reserve_entries_failing(table, to_fit, growth);

        if(state1 == ERROR)
            return state1;

        return state2;
    }

    template <class Info>
    void reserve_entries(Hash_Table<Info>* table, isize to_fit, Hash_Table_Growth const& growth = {})
    {
        State state = reserve_failing(table, to_fit, growth);
        force(state == OK_STATE && "reserve failed!");
    }
    
    template <class Info>
    void reserve_jump_table(Hash_Table<Info>* table, isize to_fit, Hash_Table_Growth const& growth = {})
    {
        State state = reserve_failing(table, to_fit, growth);
        force(state == OK_STATE && "reserve failed!");
    }
    
    template <class Info>
    void rehash(Hash_Table<Info>* table, isize to_size, Hash_Table_Growth const& growth = {})
    {
        State state = rehash_failing(table, to_size, growth);
        force(state == OK_STATE && "rehashing failed!");
    }

    template <class Info> 
    void rehash(Hash_Table<Info>* table, Hash_Table_Growth const& growth = {})
    {
        isize rehash_to = jump_table_size(*table);
        rehash(table, rehash_to, growth);
    }
    
    template <class Info>
    void reserve(Hash_Table<Info>* table, isize to_fit, Hash_Table_Growth const& growth = {})
    {
        State state = reserve_failing(table, to_fit, growth);
        force(state == OK_STATE && "reserve failed!");
    }

    template <class Info, bool break_on_gravestone = false> nodisc
    Hash_Found find(Hash_Table<Info> const& table, Info_Key const& key, hash_t hash) noexcept
    {
        using Link = Info_Link;
        using Stored_Key = Info_Stored_Key;

        assert(is_invariant(table));
        Hash_Found found = {};
        found.hash_index = -1;
        found.entry_index = -1;
        found.finished_at = -1;

        if(table._linker.size == 0)
            return found;

        hash_t mask = cast(hash_t) table._linker.size - 1;

        Slice<const Link> links = table._linker;
        Slice<const Stored_Key> keys = jot::keys(table);

        hash_t i = hash & mask;
        for(isize passed = 0;; i = (i + 1) & mask, passed ++)
        {
            Link link = links[cast(isize) i];
            if(link == cast(Link) hash_table_internal::EMPTY_LINK || passed > links.size)
                break;
        
            if(link == cast(Link) hash_table_internal::GRAVESTONE_LINK)
            {
                if constexpr(break_on_gravestone)
                    break;

                continue;
            }

            Stored_Key const& curr = keys[cast(isize) link];

            bool are_equal = false;
            if constexpr(std::is_same_v<Info_Key, Info_Stored_Key>)
            {
                are_equal = Info::Compare::are_equal(curr, key);
            }
            else
            {
                Info_Key curr_key = Info::Cast::key_cast(curr);
                are_equal = Info::Compare::are_equal(curr_key, key);
            }

            if(are_equal)
            {
                found.hash_index = cast(isize) i;
                found.entry_index = link;
                break;
            }
        }

        found.finished_at = cast(isize) i;
        return found;
    }
   
    template <class Info, bool break_on_gravestone = false, Enable_If_Keys_Differ<Info> = ENABLED> nodisc
    Hash_Found find(Hash_Table<Info> const& table, Info_Stored_Key const& key, hash_t hash) noexcept
    {
        Info_Key casted = Info::Cast::key_cast(key);
        return find(table, casted, hash);
    }

    template <class Info, bool break_on_gravestone = false> nodisc
    Hash_Found find_found_entry(Hash_Table<Info> const& table, isize entry_i, hash_t hash) noexcept
    {
        using Link = Info_Link;
        assert(is_invariant(table));

        Hash_Found found = {};
        found.hash_index = -1;
        found.entry_index = -1;
        found.finished_at = -1;

        if(table._linker.size == 0)
            return found;

        hash_t mask = cast(hash_t) table._linker.size - 1;

        Slice<const Link> links = table._linker;

        hash_t i = hash & mask;
        for(isize passed = 0;; i = (i + 1) & mask, passed ++)
        {
            Link link = links[cast(isize) i];
            if(link == cast(Link) hash_table_internal::EMPTY_LINK || passed > table._linker.size)
                break;
        
            if constexpr(break_on_gravestone)
                if(link == cast(Link) hash_table_internal::GRAVESTONE_LINK)
                    break;

            if(link == cast(Link) entry_i)
            {
                found.hash_index = cast(isize) i;
                found.entry_index = link;
                break;
            }
        }

        found.finished_at = cast(isize) i;
        return found;
    }
    
    template <class Info> nodisc
    Hash_Found find(Hash_Table<Info> const& table, Info_Key const& key) noexcept
    {
        hash_t hash = Info::Hash::hash(key);
        return find(table, key, hash);
    }
    
    template <class Info, Enable_If_Keys_Differ<Info> = ENABLED> nodisc
    Hash_Found find(Hash_Table<Info> const& table, Info_Stored_Key const& key) noexcept
    {
        Info_Key casted = Info::Cast::key_cast(key);
        return find(table, casted);
    }

    template <class Info> nodisc
    isize find_entry(Hash_Table<Info> const& table, Info_Key const& key) noexcept
    {
        return find(table, key).entry_index;
    }
    
    template <class Info, Enable_If_Keys_Differ<Info> = ENABLED> nodisc
    isize find_entry(Hash_Table<Info> const& table, Info_Stored_Key const& key) noexcept
    {
        Info_Key casted = Info::Cast::key_cast(key);
        return find_entry(table, casted);
    
    }
    template <class Info> nodisc
    bool has(Hash_Table<Info> const& table, Info_Key const& key) noexcept
    {
        return find(table, key).entry_index != -1;
    }
    
    template <class Info, Enable_If_Keys_Differ<Info> = ENABLED> nodisc
    bool has(Hash_Table<Info> const& table, Info_Stored_Key const& key) noexcept
    {
        Info_Key casted = Info::Cast::key_cast(key);
        return has(table, casted);
    }

    template <class Info> nodisc
    void mark_removed(Hash_Table<Info>* table, Hash_Found removed)
    {
        assert(0 <= removed.hash_index && removed.hash_index < table->_linker.size && "out of range!");

        table->_linker[removed.hash_index] = cast(Info_Link) hash_table_internal::GRAVESTONE_LINK;
        table->_gravestone_count += 2; //one for the link and one for the entry 
        //=> when only marking entries we will rehash faster then when removing them
    }

    //Removes an entry from the keys and values array and marks the jump table slot as deleted
    template <class Info> nodisc
    typename Hash_Table<Info>::Entry remove(Hash_Table<Info>* table, Hash_Found removed)
    {
        assert(0 <= removed.hash_index && removed.hash_index < table->_linker.size && "out of range!");
        assert(0 <= removed.entry_index && removed.entry_index < table->_entries_size && "out of range!");
        assert(table->_entries_size > 0 && "cannot remove from empty");

        isize last = table->_entries_size - 1;
        isize removed_i = removed.entry_index;

        using Value = Info_Value;
        using Link = Info_Link;
        using Stored_Key = Info_Stored_Key;

        Slice<Link> linker      = table->_linker;
        Slice<Stored_Key> keys  = {table->_keys,    table->_entries_size};
        Slice<Value> values     = {table->_values,  table->_entries_size};

        linker[removed.hash_index] = cast(Link) hash_table_internal::GRAVESTONE_LINK;
        table->_gravestone_count += 1;
        
        typename Hash_Table<Info>::Entry removed_entry_data = {
            move(&keys[removed_i]),
            move(&values[removed_i]),
        };

        bool delete_last = true;
        if(removed_i != last)
        {

            hash_t last_hash = hash_table_internal::do_hash<Info>(keys[last]);
            Hash_Found changed_for = find_found_entry(*table, last, last_hash);

            //in the case the table contains 'mark_removed' entries
            // we can no longer assure that the last key will actually exists
            // in that case we simply only mark the entry as deleted and nothing more
            // (we could scan back for other not removed entries to swap with but that has no use since
            //  the user cannot make any assumptions about which entries are used and and which ones 
            //  arent when using mark_removed)
            if(changed_for.hash_index != -1)
            {
                linker[changed_for.hash_index] = cast(Link) removed_i;
                keys[removed_i] = move(&keys[last]);
                values[removed_i] = move(&values[last]);
            }
            else
                delete_last = false;
        }

        if(delete_last)
        {
            keys[last].~Stored_Key();
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
    template <class Info>
    isize mark_removed(Hash_Table<Info>* table, Info_Key const& key)
    {
        Hash_Found found = find(*table, key);
        if(found.entry_index == -1)
            return found.entry_index;

        cast(void) mark_removed(table, found);
        return found.entry_index;
    }

    
    template <class Info, Enable_If_Keys_Differ<Info> = ENABLED>
    isize mark_removed(Hash_Table<Info>* table, Info_Stored_Key const& key)
    {
        Info_Key casted = Info::Cast::key_cast(key);
        return mark_removed(table, casted);
    }

    template <class Info> 
    bool remove(Hash_Table<Info>* table, Info_Key const& key)
    {
        Hash_Found found = find(*table, key);
        if(found.entry_index == -1)
            return false;

        cast(void) remove(table, found);
        return true;
    }
    
    template <class Info, Enable_If_Keys_Differ<Info> = ENABLED> 
    bool remove(Hash_Table<Info>* table, Info_Stored_Key const& key)
    {
        Info_Key casted = Info::Cast::key_cast(key);
        return remove(table, casted);
    }

    template <class Info>
    Hash_Table<Info>::~Hash_Table() noexcept 
    {
        assert(is_invariant(*this));
        cast(void) hash_table_internal::set_entries_capacity(this, cast(isize) 0);

        if(this->_linker.data != nullptr)
        {
            Slice<u8> bytes = cast_slice<u8>(this->_linker);
            this->_allocator->deallocate(bytes, HASH_TABLE_LINKER_ALIGN);
        }
    }

    template <class Info> nodisc
    Info_Value const& get(Hash_Table<Info> const&  table, Info_Key const& key, Info_Value const& if_not_found) noexcept
    {
        isize index = find(table, key).entry_index;
        if(index == -1)
            return if_not_found;

        return values(table)[index];
    }
    
    template <class Info, Enable_If_Keys_Differ<Info> = ENABLED> nodisc
    Info_Value const& get(Hash_Table<Info> const&  table, Info_Stored_Key const& key, Info_Value const& if_not_found) noexcept
    {
        Info_Key casted = Info::Cast::key_cast(key);
        return get(table, casted, if_not_found);
    }

    //@UNUSED
    template <class Info> nodisc
    Info_Value move_out(Hash_Table<Info>* table, Info_Key const& key, Info_Value if_not_found) noexcept
    {
        isize index = find(*table, key).entry_index;
        if(index == -1)
            return if_not_found;

        return move(&values(table)[index]);
    }
    
    template <class Info, Enable_If_Keys_Differ<Info> = ENABLED> nodisc
    Info_Value move_out(Hash_Table<Info>* table, Info_Stored_Key const& key, Info_Value if_not_found) noexcept
    {
        Info_Key casted = Info::Cast::key_cast(key);
        return move_out(table, casted, move(&if_not_found));
    }
    
    template <class Info>
    void grow_if_overfull(Hash_Table<Info>* table, Hash_Table_Growth const& growth = {}) 
    {
        assert(is_invariant(*table));
        if(hash_table_internal::is_overful(*table, growth))
        {
            Allocator_State_Type state = hash_table_internal::grow(table, growth);
            force(state == OK && "allocation failed!");
        }
    }

    template <class Info>
    void set(Hash_Table<Info>* table, Info_Stored_Key key, Info_Value value, Hash_Found at, Hash_Table_Growth const& growth = {})
    {
        assert(at.entry_index == -1 && "the entry must not be found!");
        assert(at.finished_at < table->_linker.size && "should be empty or gravestone at this point");

        //if finished on a gravestone we can overwrite it and thus remove it 
        if(at.finished_at == cast(Info_Link) hash_table_internal::GRAVESTONE_LINK)
        {
            assert(table->_gravestone_count > 0);
            table->_gravestone_count -= 1;
        }

        Allocator_State_Type state = hash_table_internal::push_entry(table, move(&key), move(&value), growth);
        force(state == OK && "allocation failed!");
        table->_linker[at.finished_at] = cast(Info_Link) table->_entries_size - 1;
    }
 
    template <class Info>
    void set(Hash_Table<Info>* table, Info_Stored_Key key, Info_Value value, Hash_Table_Growth const& growth = {})
    {
        grow_if_overfull(table, growth);

        //Here is a bit of ugliness...
        //If we already have the key type we can skip the 
        // casting to Key (which as it returns a copy can be rather expensive for strings)
        //This means we have to duplicate the code
        Hash_Found found = {};
        if constexpr(std::is_same_v<Info_Key, Info_Stored_Key>)
        {
            hash_t hash = Info::Hash::hash(key);
            found = find<Info, true>(*table, key, hash);
        }
        else
        {
            Info_Key casted = Info::Cast::key_cast(key);
            hash_t hash = Info::Hash::hash(casted);
            found = find<Info, true>(*table, casted, hash);
        }
        
        if(found.entry_index != -1)
        {
            values(table)[found.entry_index] = move(&value);
            return;
        }

        return set(table, move(&key), move(&value), found, growth);
    }
    
    namespace multi
    {
        template <class Info, bool break_on_gravestone = false> nodisc
        Hash_Found find_next(Hash_Table<Info> const& table, Info_Key const& prev_key, Hash_Found prev) noexcept
        {
            assert(prev.hash_index != -1 && "must be found!");
            assert(prev.entry_index != -1 && "must be found!");

            return find(table, prev_key, cast(hash_t) prev.hash_index + 1);
        }

        
        template <class Info, bool break_on_gravestone = false, Enable_If_Keys_Differ<Info> = ENABLED> nodisc
        Hash_Found find_next(Hash_Table<Info> const& table, Info_Stored_Key const& prev_key, Hash_Found prev) noexcept
        {
            Info_Key key = Info::Cast::key_cast(prev_key);
            return find_next(table, key, prev);
        }

        template <class Info>
        void add_another(Hash_Table<Info>* table, Info_Stored_Key stored_key, Info_Value value, Hash_Table_Growth const& growth = {})
        {
            grow_if_overfull(table, growth);
            
            //I wish there was a way to do this without having to duplicate code...
            Hash_Found found = {};
            if constexpr(std::is_same_v<Info_Key, Info_Stored_Key>)
            {
                hash_t hash = Info::Hash::hash(stored_key);
                found = find<Info, true>(*table, stored_key, hash);
                while(found.entry_index != -1)
                    found = find_next<Info, true>(*table, stored_key, found);
            }
            else
            {   
                Info_Key key = Info::Cast::key_cast(stored_key);
                hash_t hash = Info::Hash::hash(key);
                found = find<Info, true>(*table, key, hash);
                while(found.entry_index != -1)
                    found = find_next<Info, true>(*table, key, found);
            }
        
            return set(table, move(&stored_key), move(&value), found, growth);
        }
    }

    #undef Info_Key           
    #undef Info_Value         
    #undef Info_Stored_Key    
    #undef Info_Cast          
    #undef Info_Compare       
    #undef Info_Hash          
    #undef Info_Link         

    //Specializations of the Hashable and Key_Comparable for some of the common types:
    template <typename T>
    struct Hashable<T, Enable_If<std::is_integral_v<T>>>
    {
        static constexpr
        hash_t hash(T const& val) noexcept
        {
            return uint64_hash(cast(u64) val);
        }
    };

    template <typename T>
    struct Hashable<Slice<T>>
    {
        static 
        hash_t hash(Slice<T> const& val) noexcept
        {
            //if is scalar (array of bultin types ie char, int...) just use the optimal hash
            if constexpr(std::is_scalar_v<T>)
                return cast(hash_t) murmur_hash64(val.data, val.size * cast(isize) sizeof(T), cast(u64) val.size);
            //else mixin the hashes for each element
            else
            {
                u64 running_hash = cast(u64) val.size;
                for(isize i = 0; i < val.size; i++)
                    running_hash ^= Hashable<T>::hash(val);
            
                return cast(hash_t) running_hash;
            }
        }
    };
    
    template <typename T>
    struct Hashable<Stack<T>>
    {
        static 
        hash_t hash(Stack<T> const& val) noexcept
        {
            return Hashable<Slice<const T>>::hash(slice(val));
        }
    };

    template <typename T>
    struct Key_Comparable<Slice<T>>
    {
        static
        bool are_equal(Slice<T> const& a, Slice<T> const& b) noexcept
        {
            //if possible use by byte compare (memcmp)
            if constexpr(std::is_scalar_v<T>)
                return compare(cast_slice<const u8>(a),  cast_slice<const u8>(b)) == 0;       
            else
            {
                if(a.size != b.size)
                    return false;
                
                for(isize i = 0; i < a.size; i++)
                {
                    if(Key_Comparable<T>::are_equal(a[i], b[i]) == false)
                        return false;
                }

                return true;
            }
        }
    };

    template <typename T>
    struct Key_Comparable<Stack<T>>
    {
        static
        bool are_equal(Stack<T> const& a, Stack<T> const& b) noexcept
        {
            return Key_Comparable<Slice<const T>>::are_equal(slice(a), slice(b));
        }
    };

    template <typename T>
    struct Key_Castable<Stack<T>, Slice<const T>>
    {
        static
        Slice<const T> key_cast(Stack<T> const& from)
        {
            return slice(from);
        }
    };
    
    //Shorthand name
    template<typename Stored_Key, typename Value> 
    using Map = Hash_Table<Hash_Table_Info<Stored_Key, Value>>;
    
    //optimalization for all stacks (including strings)
    // making it possible to index by Slices
    //Note that custom Info's are still possible
    template<typename T, typename Value_> 
    struct Hash_Table_Info<Stack<T>, Value_>
    {
        using Key = Slice<const T>;
        using Value = Value_;
        using Stored_Key = Stack<T>;
        using Cast = Key_Castable<Stored_Key, Key>;
        using Compare = Key_Comparable<Key>;
        using Hash = Hashable<Key>;
    };
}

#include "undefs.h"