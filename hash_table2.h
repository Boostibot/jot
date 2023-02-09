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


    #define HASH_TABLE_ENTRIES_ALIGN 32
    #define HASH_TABLE_LINKER_ALIGN 32
    #define HASH_TABLE_LINKER_BASE_SIZE 8

    //rehashes to same size if gravestones make more than
    // HASH_TABLE_MAX_GRAVESTONES_NUM / HASH_TABLE_MAX_GRAVESTONES_DEN portion
    #define HASH_TABLE_MAX_GRAVESTONES_NUM 1
    #define HASH_TABLE_MAX_GRAVESTONES_DEN 4

    // template<typename T>
    // concept hashable = !std::is_base_of_v<No_Default, Hashable<T>>;
    
    template<typename Key, typename Value>
    struct Hash_Table_Entry
    {
        Key key;
        Value value;
    };
    
    template<typename Key, typename Value>
    using Hash_Table_Link = std::conditional_t<(sizeof(Hash_Table_Entry<Key, Value>) >= 8), u32, u64>;

    template<
        typename Key_, 
        typename Value_, 
        typename Hash_ = Hashable<Key_>
        //,
        //typename Compare_ = Key_Comparable<Key_>
        >
    struct Hash_Table2
    {
        using Key = Key_;
        using Value = Value_;
        using Hash = Hash_;
        //using Compare = Compare_;
        using Entry = Hash_Table_Entry<Key, Value>;
        
        //we assume that if the entry size is equal or greater than 8 (which is usually the case) 
        // we wont ever hold more than 4 million entries (would be more than 32GB of data)
        // this reduces the necessary size for linker by 50%
        using Link = Hash_Table_Link<Key, Value>;

        Allocator* _allocator = memory_globals::default_allocator();
        Key* _keys = nullptr;
        Value* _values = nullptr;
        Link* _linker = nullptr;
        
        isize _entries_size = 0;
        isize _entries_capacity = 0;
        isize _linker_size = 0;
        isize _gravestone_count = 0; //the count of gravestones in linker array + the count of unreferenced entries
        // when too large triggers cleaning rehash

        //@NOTE: we *could* use Stack to hold keys and Stack to hold values since 
        // we effectively use one. That would howeever cost us extra 3*8 redundant bytes 
        // which would mean this structure would no longer fit into 64B cache line and thus 
        // requiring more than one memory read. This could very negatively impact performance.
        
        //@NOTE: we can implement multi hash table or properly support deletion if we added extra array
        //   called _group containg for each entry a next and prev entry index (as u32). That way we 
        //   could upon deletion go to back or prev and change the _linker index to point to either of them

        Hash_Table2() noexcept = default;
        Hash_Table2(Allocator* alloc) noexcept : _allocator(alloc) {}
        Hash_Table2(Hash_Table2&& other) noexcept;
        Hash_Table2(Hash_Table2 const& other) = delete;
        ~Hash_Table2() noexcept;

        Hash_Table2& operator=(Hash_Table2&& other) noexcept;
        Hash_Table2& operator=(Hash_Table2 const& other) = delete;
    };
    
    template <typename Key, typename Value, typename Hash> nodisc
    bool is_invariant(Hash_Table2<Key, Value, Hash> const& hash)
    {
        bool is_size_power = hash._linker_size == 0;
        if(is_size_power == false)
            is_size_power = is_power_of_two(hash._linker_size);

        bool is_alloc_not_null = hash._allocator != nullptr;
        bool are_entries_simulatinous_alloced = (hash._keys == nullptr) == (hash._values == nullptr);
        bool are_entry_sizes_correct = (hash._keys == nullptr) == (hash._entries_size == 0);
        bool are_linker_sizes_correct = (hash._linker == nullptr) == (hash._linker_size == 0);

        bool are_sizes_in_range = hash._entries_size <= hash._entries_capacity;

        bool res = is_size_power && is_alloc_not_null && are_entries_simulatinous_alloced 
            && are_entry_sizes_correct && are_linker_sizes_correct && are_sizes_in_range;

        assert(res);
        return res;
    }

    template <typename Key, typename Value, typename Hash> nodisc
    Slice<const Key> keys(Hash_Table2<Key, Value, Hash> const& hash)
    {
        Slice<const Key> out = {hash._keys, hash._entries_size};
        return out;
    }
    
    template <typename Key, typename Value, typename Hash> nodisc
    Slice<Value> values(Hash_Table2<Key, Value, Hash>* hash)
    {
        Slice<Value> out = {hash->_values, hash->_entries_size};
        return out;
    }
    
    template <typename Key, typename Value, typename Hash> nodisc
    Slice<const Value> values(Hash_Table2<Key, Value, Hash> const& hash)
    {
        Slice<const Value> out = {hash._values, hash._entries_size};
        return out;
    }

    template <typename Key, typename Value, typename Hash> nodisc
    isize jump_table_size(Hash_Table2<Key, Value, Hash> const& hash)
    {
        return hash._linker_size;
    }

    template <typename Key, typename Value, typename Fns>
    void swap(Hash_Table2<Key, Value, Fns>* left, Hash_Table2<Key, Value, Fns>* right) noexcept
    {
        swap(&left->_allocator, &right->_allocator);
        swap(&left->_keys, &right->_keys);
        swap(&left->_values, &right->_values);
        swap(&left->_linker, &right->_linker);
        swap(&left->_entries_size, &right->_entries_size);
        swap(&left->_entries_capacity, &right->_entries_capacity);
        swap(&left->_gravestone_count, &right->_gravestone_count);
        swap(&left->_linker_size, &right->_linker_size);
    }

    template <typename Key, typename Value, typename Fns>
    Hash_Table2<Key, Value, Fns>::Hash_Table2(Hash_Table2 && other) noexcept 
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

        template <typename Key, typename Value, typename Hash> nodisc
        auto linker(Hash_Table2<Key, Value, Hash> const& hash)
        {
            Slice<const Hash_Table_Link<Key, Value>> out = {hash._linker, hash._linker_size};
            return out;
        }

        template <typename Key, typename Value, typename Hash> nodisc
        auto linker(Hash_Table2<Key, Value, Hash>* hash)
        {
            Slice<Hash_Table_Link<Key, Value>> out = {hash->_linker, hash->_linker_size};
            return out;
        }
        
        template <typename Key, typename Value, typename Hash> nodisc
        Allocator_State_Type set_entries_capacity(Hash_Table2<Key, Value, Hash>* hash, isize new_capacity) noexcept
        {
            Allocator* alloc = hash->_allocator;
            isize align = HASH_TABLE_ENTRIES_ALIGN;
            isize size = hash->_entries_size;

            Slice<Key> old_keys = Slice<Key>{hash->_keys, hash->_entries_capacity};
            Slice<Value> old_values = Slice<Value>{hash->_values, hash->_entries_capacity};

            Set_Capacity_Result<Key> key_res = set_capacity_allocation_stage(alloc, &old_keys, align, new_capacity, true);
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

        template <typename Key, typename Value, typename Hash> nodisc
        Allocator_State_Type push_entry(Hash_Table2<Key, Value, Hash>* hash, Key key, Value value) noexcept
        {
            assert(is_invariant(*hash));

            isize size = hash->_entries_size;
            isize capacity = hash->_entries_capacity;
            if(size >= capacity)
            {
                isize new_capacity = calculate_stack_growth(hash->_entries_capacity, size + 1);
                Allocator_State_Type state = set_entries_capacity(hash, new_capacity);
                if(state == ERROR)
                    return state;
            }

            new (&hash->_keys[size]) Key(move(&key));
            new (&hash->_values[size]) Value(move(&value));

            hash->_entries_size = size + 1;
            
            assert(is_invariant(*hash));
            return Allocator_State::OK;
        }
    
        template <typename Key, typename Value, typename Hash> nodisc
        Allocator_State_Type unsafe_rehash(Hash_Table2<Key, Value, Hash>* table, isize to_size) noexcept
        {
            assert(is_invariant(*table));
            using Link = Hash_Table_Link<Key, Value>;

            isize required_min_size = div_round_up(table->_entries_size, sizeof(Link));
            assert(to_size >= required_min_size && is_power_of_two(to_size) && "must be big enough (no more shrinking than by 4 at a time) and power of two");


            to_size = max(to_size, required_min_size);

            Slice<Link> old_linker = linker(table);
            Allocation_Result result = table->_allocator->allocate(to_size * cast(isize) sizeof(Link), HASH_TABLE_LINKER_ALIGN);
            if(result.state == ERROR)
               return result.state; 

            //mark occurences of each entry index in a bool array
            Slice<bool> marks = trim(cast_slice<bool>(result.items), table->_entries_size);
            null_items(&marks);

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
                else
                    alive_count ++;
                #endif
                
                isize link = cast(isize) old_linker[i];
                //skip gravestones and empty slots
                if(link > table->_entries_size)
                    continue;


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
            isize swap_count = 0;
                
            while(forward_index < backward_index)
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
                    if(forward_index < 0)
                        break;
                }
                    
                if(forward_index >= backward_index)
                    break;

                table->_keys[forward_index] = move(&table->_keys[backward_index]);
                table->_values[forward_index] = move(&table->_values[backward_index]);

                swap_count ++;
                forward_index ++;
                backward_index--;
            }

            isize alive_entries_size = forward_index;
            assert(alive_count == alive_entries_size);
            assert(swap_count <= graveston_count);
            
            //fill new_linker to empty
            Slice<Link> new_linker = cast_slice<Link>(result.items);
            fill(&new_linker, cast(Link) EMPTY_LINK);

            //rehash every entry up to alive_entries_size
            hash_t mask = cast(hash_t) new_linker.size - 1;
            for(isize i = 0; i < alive_entries_size; i++)
            {
                Key const& key = table->_keys[i];
                hash_t hash = Hash::hash(key);
                hash_t index = hash & mask;
                new_linker[cast(isize) index] = cast(Link) i;
            }

            //destroy the dead entries
            for(isize i = alive_entries_size; i < table->_entries_size; i++)
            {
                table->_keys[i].~Key();
                table->_values[i].~Value();
            }
            
            assert(table->_entries_size <= alive_entries_size + table->_gravestone_count);

            table->_gravestone_count = 0;
            table->_entries_size = alive_entries_size;
            table->_linker_size = new_linker.size;
            table->_linker = new_linker.data;
            table->_allocator->deallocate(cast_slice<u8>(old_linker), HASH_TABLE_LINKER_ALIGN);
            
            assert(is_invariant(*table));

            return Allocator_State::OK;
        }

        template <typename Key, typename Value, typename Hash> nodisc
        bool is_overful(Hash_Table2<Key, Value, Hash> const& table)
        {
            //max 25% utlization
            return table._linker_size - table._gravestone_count <= table._entries_size * 4;
        }
        
        template <typename Key, typename Value, typename Hash> nodisc
        Allocator_State_Type grow(Hash_Table2<Key, Value, Hash>* table) noexcept
        {
            isize rehash_to = table->_linker_size * 2;

            //if too many gravestones keeps the same size onky clears out the grabage
            // ( gravestones / size >= MAX_NUM / MAX_DEN )
            if(table->_gravestone_count * HASH_TABLE_MAX_GRAVESTONES_DEN >= table->_linker_size * HASH_TABLE_MAX_GRAVESTONES_NUM)
                rehash_to = table->_linker_size;
            
            //If zero go to base size
            if(rehash_to == 0)
                rehash_to = HASH_TABLE_LINKER_BASE_SIZE;

            return unsafe_rehash(table, rehash_to);
        }
    }
    
    template <typename Key, typename Value, typename Hash> nodisc
    Allocator_State_Type rehash(Hash_Table2<Key, Value, Hash>* table, isize to_size) noexcept
    {
        using Link = Hash_Table_Link<Key, Value>;
        isize rehash_to = HASH_TABLE_LINKER_BASE_SIZE;

        isize required_min_size = div_round_up(table->_entries_size, sizeof(Link)); //cannot shrink below this
        isize normed = max(to_size, required_min_size);
        while(rehash_to < normed)
            rehash_to *= 2; //must be power of two and this is the simplest way of ensuring it

        return hash_table_internal::unsafe_rehash(table, rehash_to);
    }
    
    template <typename Key, typename Value, typename Hash> nodisc
    Allocator_State_Type rehash(Hash_Table2<Key, Value, Hash>* table) noexcept
    {
        isize rehash_to = jump_table_size(*table);
        return hash_table_internal::unsafe_rehash(table, rehash_to);
    }

    template <typename Key, typename Value, typename Hash> nodisc
    Allocator_State_Type reserve(Hash_Table2<Key, Value, Hash>* table, isize to_fit) noexcept
    {
        if(to_fit < table->_linker_size)
            return Allocator_State_Type::OK;

        return unsafe_rehash(table, to_fit);
    }

    template <typename Key, typename Value, typename Hash, bool break_on_gravestone = false> nodisc
    Hash_Found find(Hash_Table2<Key, Value, Hash> const& table, Key const& key, hash_t hash) noexcept
    {
        using Link = Hash_Table_Link<Key, Value>;
        assert(is_invariant(table));
        Hash_Found found = {};
        found.hash_index = -1;
        found.entry_index = -1;
        found.finished_at = -1;

        if(table._linker_size == 0)
            return found;

        hash_t mask = cast(hash_t) table._linker_size - 1;
        hash_t modulated = hash & mask;

        Slice<const Link> links = hash_table_internal::linker(table);
        Slice<const Key> keys = jot::keys(table);

        hash_t i = modulated;
        for(isize passed = 0;; i = (i + 1) & mask, passed ++)
        {
            Link link = links[cast(isize) i];
            if(link == cast(Link) hash_table_internal::EMPTY_LINK || passed > table._linker_size)
                break;
        
            if(link == cast(Link) hash_table_internal::GRAVESTONE_LINK)
            {
                if constexpr(break_on_gravestone)
                    break;

                continue;
            }

            Key const& curr = keys[cast(isize) link];
            if(curr == key)
            {
                found.hash_index = cast(isize) i;
                found.entry_index = link;
                break;
            }
        }

        found.finished_at = cast(isize) i;
        return found;
    }
    
    template <typename Key, typename Value, typename Hash> nodisc
    Hash_Found find(Hash_Table2<Key, Value, Hash> const& table, no_infer(Key) const& key) noexcept
    {
        hash_t hash = Hash::hash(key);
        return find(table, key, hash);
    }

    template <typename Key, typename Value, typename Hash> nodisc
    void mark_removed(Hash_Table2<Key, Value, Hash>* table, Hash_Found removed)
    {
        assert(0 <= removed.hash_index && removed.hash_index < table->_linker_size && "out of range!");

        table->_linker[removed.hash_index] = cast(Hash_Table_Link<Key, Value>) hash_table_internal::GRAVESTONE_LINK;
        table->_gravestone_count += 2; //one for the link and one for the entry 
        //=> when only marking entries we will rehash faster then when removing them
    }

    template <typename Key, typename Value, typename Hash> nodisc
    Hash_Table_Entry<Key, Value> remove(Hash_Table2<Key, Value, Hash>* table, Hash_Found removed)
    {
        assert(0 <= removed.hash_index && removed.hash_index < table->_linker_size && "out of range!");
        assert(0 <= removed.entry_index && removed.entry_index < table->_entries_size && "out of range!");
        assert(table->_entries_size > 0 && "cannot remove from empty");

        isize last = table->_entries_size - 1;
        isize removed_i = removed.entry_index;
        table->_linker[removed.hash_index] = cast(Hash_Table_Link<Key, Value>) hash_table_internal::GRAVESTONE_LINK;
        table->_gravestone_count += 1;

        if(removed_i != last)
        {
            Hash_Found changed_for = find(*table, table->_keys[last]);
            table->_linker[changed_for.hash_index] = cast(Hash_Table_Link<Key, Value>) removed_i;
        }

        Hash_Table_Entry<Key, Value> removed_entry_data = {
            move(&table->_keys[removed_i]),
            move(&table->_values[removed_i]),
        };
        
        if(removed_i != last)
        {
            table->_keys[removed_i] = move(&table->_keys[last]);
            table->_values[removed_i] = move(&table->_values[last]);
        }

        table->_keys[last].~Key();
        table->_values[last].~Value();
            
        table->_entries_size -= 1;
        return removed_entry_data;
    }


    template <typename Key, typename Value, typename Hash>
    isize mark_removed(Hash_Table2<Key, Value, Hash>* table, no_infer(Key) const& key)
    {
        Hash_Found found = find(*table, key);
        if(found.entry_index == -1)
            return found.entry_index;

        cast(void) mark_removed(table, found);
        return found.entry_index;
    }

    template <typename Key, typename Value, typename Hash> 
    bool remove(Hash_Table2<Key, Value, Hash>* table, no_infer(Key) const& key)
    {
        Hash_Found found = find(*table, key);
        if(found.entry_index == -1)
            return false;

        cast(void) remove(table, found);
        return true;
    }

    template <typename Key, typename Value, typename Fns>
    Hash_Table2<Key, Value, Fns>::~Hash_Table2() noexcept 
    {
        assert(is_invariant(*this));
        cast(void) hash_table_internal::set_entries_capacity(this, cast(isize) 0);

        if(this->_linker != nullptr)
        {
            auto linker = hash_table_internal::linker(this);
            Slice<u8> bytes = cast_slice<u8>(linker);
            this->_allocator->deallocate(bytes, HASH_TABLE_LINKER_ALIGN);
        }
    }

    template <typename Key, typename Value, typename Hash> nodisc
    isize find_entry(Hash_Table2<Key, Value, Hash> const& table, no_infer(Key) const& key) noexcept
    {
        return find(table, key).entry_index;
    }
    
    template <typename Key, typename Value, typename Hash> nodisc
    bool has(Hash_Table2<Key, Value, Hash> const& table, no_infer(Key) const& key) noexcept
    {
        return find(table, key).entry_index != -1;
    }

    template <typename Key, typename Value, typename Hash> nodisc
    Value const& get(Hash_Table2<Key, Value, Hash> const&  table, no_infer(Key) const& key, no_infer(Value) const& if_not_found) noexcept
    {
        isize index = find(table, key).entry_index;
        if(index == -1)
            return if_not_found;

        return values(table)[index];
    }

    template <typename Key, typename Value, typename Hash> nodisc
    Value move_out(Hash_Table2<Key, Value, Hash>* table, no_infer(Key) const& key, no_infer(Value) if_not_found) noexcept
    {
        isize index = find(*table, key).entry_index;
        if(index == -1)
            return if_not_found;

        return move(&values(table)[index]);
    }

    template <typename Key, typename Value, typename Hash> nodisc
    Allocator_State_Type set(Hash_Table2<Key, Value, Hash>* table, Key key, hash_t hash, Value value) noexcept
    {
        assert(is_invariant(*table));
        if(hash_table_internal::is_overful(*table))
        {
            Allocator_State_Type state = hash_table_internal::grow(table);
            if(state == ERROR)
                return state;
        }

        Hash_Found found = find<Key, Value, Hash, true>(*table, key, hash);
        if(found.entry_index != -1)
        {
            values(table)[found.entry_index] = move(&value);
            return Allocator_State::OK;
        }
        
        assert(found.finished_at < table->_linker_size && "should be empty or gravestone at this point");

        //if finished on a gravestone we can overwrite it and thus remove it 
        if(found.finished_at == cast(Hash_Table_Link<Key, Value>) hash_table_internal::GRAVESTONE_LINK)
        {
            assert(table->_gravestone_count > 0);
            table->_gravestone_count -= 1;
        }

        Allocator_State_Type state = hash_table_internal::push_entry(table, move(&key), move(&value));
        if(state == OK)
            table->_linker[found.finished_at] = cast(Hash_Table_Link<Key, Value>) table->_entries_size - 1;

        return state;
    }
    
    template <typename Key, typename Value, typename Hash> nodisc
    Allocator_State_Type set(Hash_Table2<Key, Value, Hash>* table, no_infer(Key) key, no_infer(Value) value) noexcept
    {
        hash_t hash = Hash::hash(key);
        return set<Key, Value, Hash>(table, move(&key), hash, move(&value));
    }
}

#include "undefs.h"