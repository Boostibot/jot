#pragma once

#include <limits>

#include "memory.h"
#include "hash.h"
#include "defines.h"

namespace jot
{
    constexpr isize HASH_SET_KEYS_ALIGN = 32;
    constexpr isize HASH_SET_VALUES_ALIGN = 32;
    constexpr isize HASH_SET_BASE_SIZE = 16;
    constexpr isize HASH_SET_BASE_BYTES = 128;
    constexpr isize HASH_SET_MAX_UTILIZATIO_NUM = 32;
    constexpr isize HASH_SET_MAX_UTILIZATIO_DEN = 128;
    
    template <typename Key, typename Enable = Enabled>
    struct Default_Hash_Functions
    {
        static uint64_t hash(Key const& key);
        static bool is_equal(Key const& a, Key const& b);
        static void set_null_state(Key* key);
        static bool is_null_state(Key const& key);
    };

    template <typename Key, typename Value, typename Hash_Functions = Default_Hash_Functions<Key>>
    struct Hash_Table
    {
        Allocator* allocator = memory_globals::default_allocator();
        Key* keys = nullptr;
        Value* values = nullptr;
        isize size = 0;
        isize used = 0; //number of current set key-value pairs
        bool null_state_used = false;

        Hash_Table() noexcept = default;
        Hash_Table(Allocator* alloc) noexcept : allocator(alloc) {}
        Hash_Table(Hash_Table&& other) noexcept;
        Hash_Table(Hash_Table const& other) = delete;
        ~Hash_Table() noexcept;

        Hash_Table& operator=(Hash_Table&& other) noexcept;
        Hash_Table& operator=(Hash_Table const& other) = delete;
    };
    
    template <typename Key, typename Value, typename Fns> nodisc
    bool is_invariant(Hash_Table<Key, Value, Fns> const& hash)
    {
        bool is_size_power = hash.size == 0;
        if(is_size_power == false)
            is_size_power = is_power_of_two(hash.size - 1) && hash.size >= 3;

        bool is_alloc_not_null = hash.allocator != nullptr;
        bool are_both_alloced = (hash.keys == nullptr) == (hash.values == nullptr);
        bool are_sizes_correct = (hash.keys == nullptr) == (hash.size == 0);
        bool is_used_in_range = hash.size >= hash.used && hash.used >= 0;

        return is_size_power && is_alloc_not_null && are_both_alloced && are_sizes_correct && is_used_in_range;
    }

    template <typename Key, typename Value, typename Fns> nodisc
    Slice<Key> keys(Hash_Table<Key, Value, Fns>* hash)
    {
        Slice<Key> out = {hash->keys, hash->size};
        return out;
    }
    
    template <typename Key, typename Value, typename Fns> nodisc
    Slice<const Key> keys(Hash_Table<Key, Value, Fns> const& hash)
    {
        Slice<const Key> out = {hash.keys, hash.size};
        return out;
    }
    
    template <typename Key, typename Value, typename Fns> nodisc
    Slice<Value> values(Hash_Table<Key, Value, Fns>* hash)
    {
        Slice<Value> out = {hash->values, hash->size};
        return out;
    }
    
    template <typename Key, typename Value, typename Fns> nodisc
    Slice<const Value> values(Hash_Table<Key, Value, Fns> const& hash)
    {
        Slice<const Value> out = {hash.values, hash.size};
        return out;
    }
    
    template <typename Key, typename Value, typename Fns> nodisc
    bool is_used(Hash_Table<Key, Value, Fns> const& hash, isize index)
    {
        assert(index >= 0 && index < hash.size && "index out of bounds!");

        Key const& key = hash.keys[index];
        bool is_used = Fns::is_null_state(key) == false;
        bool is_null_state_used = index == 0 && hash.null_state_used;
            
        return is_used || is_null_state_used;
    }
    
    template <typename Key, typename Value, typename Fns> nodisc
    bool is_empty(Hash_Table<Key, Value, Fns> const& hash, isize index)
    {
        return is_used(hash, index) == false;
    }

    template <typename Key, typename Value, typename Fns>
    void swap(Hash_Table<Key, Value, Fns>* left, Hash_Table<Key, Value, Fns>* right) noexcept
    {
        swap(&left->allocator, &right->allocator);
        swap(&left->keys, &right->keys);
        swap(&left->values, &right->values);
        swap(&left->size, &right->size);
        swap(&left->used, &right->used);
        swap(&left->null_state_used, &right->null_state_used);
    }

    template <typename Key, typename Value, typename Fns>
    Hash_Table<Key, Value, Fns>::Hash_Table(Hash_Table && other) noexcept 
    {
        assert(is_invariant(*this));
        assert(is_invariant(other));

        swap(this, &other);
    }
    
    template <typename Key, typename Value, typename Fns>
    Hash_Table<Key, Value, Fns>::~Hash_Table() noexcept 
    {
        assert(is_invariant(*this));

        if(this->keys != nullptr)
        {
            //destruct all keys and used values
            Slice<Key> key_items = jot::keys(this);
            Slice<Value> value_items = jot::values(this);

            for(isize i = 0; i < this->size; i++)
            {
                if(is_used(*this, i))
                    value_items[i].~Value();

                key_items[i].~Key();
            }

            //deallocate alloced
            this->allocator->deallocate(cast_slice<u8>(key_items), HASH_SET_KEYS_ALIGN);
            this->allocator->deallocate(cast_slice<u8>(value_items), HASH_SET_VALUES_ALIGN);
        }
    }

    template <typename Key, typename Value, typename Fns>
    Hash_Table<Key, Value, Fns>& Hash_Table<Key, Value, Fns>::operator=(Hash_Table && other) noexcept 
    {
        assert(is_invariant(*this));
        assert(is_invariant(other));

        swap(this, &other);
        return *this;
    }

    template <typename Key, typename Value, typename Fns> nodisc
    isize find_entry(Hash_Table<Key, Value, Fns> const& hash, no_infer(Key) const& key)
    {
        assert(is_invariant(hash));

        if(hash.size == 0)
            return -1;

        hash_t result = Fns::hash(key);
        hash_t mask = cast(hash_t) hash.size - 2;

        hash_t index = result & mask;
        isize contention = 0;

        assert(hash.size >= 3 && "size must not be smaller than 3 (2 regular size)");
        for(hash_t i = index;; contention++)
        {
            assert(contention != hash.size && "the contention must not be 100%");

            Key const& current = hash.keys[i + 1];
            if(Fns::is_null_state(current))
                break;

            if(Fns::is_equal(current, key))
                return cast(isize) i + 1;

            i = (i + 1) & mask;
        }

        if(Fns::is_null_state(key) && hash.null_state_used)
            return 0;

        return -1;
    }
    
    template <typename Key, typename Value, typename Hash> nodisc
    bool has(Hash_Table<Key, Value, Hash> const& table, no_infer(Key) const& key) noexcept
    {
        return find_entry(table, key) != -1;
    }

    template <typename Key, typename Value, typename Hash> nodisc
    Value const& get(Hash_Table<Key, Value, Hash>const& table, no_infer(Key) const& key, no_infer(Value) const& if_not_found) noexcept
    {
        isize index = find_entry(table, key);
        if(index == -1)
            return if_not_found;

        return values(table)[index];
    }

    template <typename Key, typename Value, typename Hash> nodisc
    Value move_out(Hash_Table<Key, Value, Hash>* table, no_infer(Key) const& key, no_infer(Value) if_not_found) noexcept
    {
        isize index = find_entry(table, key);
        if(index == -1)
            return if_not_found;

        return move(&values(table)[index]);
    }


    template <typename Key, typename Value, typename Fns> nodisc
    Allocator_State_Type rehash(Hash_Table<Key, Value, Fns>* hash, isize min_size)
    {
        assert(is_invariant(*hash));
        constexpr isize base_elems = max(HASH_SET_BASE_BYTES / cast(isize) sizeof(Key), HASH_SET_BASE_SIZE);

        isize old_regular_size = hash->size - 1;
        isize new_regular_size = max(old_regular_size, base_elems);
        while(min_size - 1 > new_regular_size)
            new_regular_size *= 2;

        isize new_size = new_regular_size + 1;
        assert(is_power_of_two(new_size - 1));
        assert(new_size >= 3);

        Allocation_Result keys_res = hash->allocator->allocate(new_size * cast(isize) sizeof(Key), HASH_SET_KEYS_ALIGN);
        if(keys_res.state == ERROR)
            return keys_res.state;

        Allocation_Result vals_res = hash->allocator->allocate(new_size * cast(isize) sizeof(Value), HASH_SET_VALUES_ALIGN);
        if(vals_res.state == ERROR)
        {
            if(keys_res.state == OK)
                hash->allocator->deallocate(keys_res.items, HASH_SET_KEYS_ALIGN);

            return vals_res.state;
        }
            
        Slice<Key> new_keys = cast_slice<Key>(keys_res.items);
        Slice<Value> new_vals = cast_slice<Value>(vals_res.items);

        //set all keys to null state
        for(isize i = 0; i < new_keys.size; i++)
        {
            Key* current = new (&new_keys[i]) Key();
            Fns::set_null_state(current);
        }
        
        hash_t new_mask = cast(hash_t) new_size - 2;
        for(isize i = 1; i < hash->size; i++)
        {
            Key& key = hash->keys[i];
            //i from 1 => we handle the null slot seperately
            if(Fns::is_null_state(key))
                continue;
            
            Value* val = &hash->values[i + 1];

            hash_t hashed = Fns::hash(key);
            hash_t index = hashed & new_mask;
            new_keys[cast(isize) index + 1] = move(&key);
            new (&new_vals[cast(isize) index + 1]) Value(move(val));
        }

        if(hash->null_state_used)
        {
            Value* val = &hash->values[0];
            new (&new_vals[0]) Value(move(val));
        }

        Hash_Table<Key, Value, Fns> new_table(hash->allocator);
        new_table.size = new_size;
        new_table.keys = new_keys.data;
        new_table.values = new_vals.data;
        new_table.used = hash->used;
        new_table.null_state_used = hash->null_state_used;

        *hash = move(&new_table);
        
        assert(is_invariant(*hash));
        return Allocator_State::OK;
    }

    template <typename Key, typename Value, typename Fns> nodisc
    Allocator_State_Type set(Hash_Table<Key, Value, Fns>* hash, no_infer(Key) key, no_infer(Value) value)
    {
        assert(is_invariant(*hash));
        if(hash->used * HASH_SET_MAX_UTILIZATIO_DEN >= hash->size * HASH_SET_MAX_UTILIZATIO_NUM)
        {
            Allocator_State_Type state = rehash(hash, hash->size + 1);
            if(state == ERROR)
                return state;
        }

        hash_t result = Fns::hash(key);
        hash_t mask = cast(hash_t) hash->size - 2;

        hash_t index = result & mask;
        isize contention = 0;
        isize found_i = 0;
        bool first_used = false;
        
        for(hash_t i = index;; contention++)
        {
            assert(contention != hash->size && "the contention must not be 100%");

            isize curr_i = cast(isize) i + 1;
            Key const& current = hash->keys[curr_i];
            if(Fns::is_null_state(current))
            {
                bool is_null_state = Fns::is_null_state(key);
                if(is_null_state)
                {
                    found_i = 0;
                    first_used = hash->null_state_used == false;
                    hash->null_state_used = true;
                }
                else
                {
                    found_i = curr_i;
                    first_used = true;
                }

                break;
            }

            if(Fns::is_equal(current, key))
            {
                found_i = curr_i;
                break;
            }

            i = (i + 1) & mask;
        }
        
        if(first_used)
        {
            new (&values(hash)[found_i]) Value(move(&value));
            hash->used += 1;
            keys(hash)[found_i] = move(&key);
        }
        else
        {
            values(hash)[found_i] = move(&value);
        }

        assert(is_invariant(*hash));
        return Allocator_State::OK;
    }

  
    struct Not_Integral_Hash {};

    template <typename Int_Key>
    struct Default_Hash_Functions<Int_Key, Enable_If<std::is_integral_v<Int_Key>>>
    {
        static constexpr Int_Key NULL_STATE = std::is_unsigned_v<Int_Key> 
            ? std::numeric_limits<Int_Key>::max() 
            : std::numeric_limits<Int_Key>::min(); 

        static uint64_t hash(Int_Key const& key)
        {
            hash_t convreted = cast(hash_t) key;
            return uint64_hash(convreted);
        }

        static bool is_equal(Int_Key const& a, Int_Key const& b)
        {
            return a == b;
        }

        static void set_null_state(Int_Key* key)
        {
            *key = NULL_STATE;
        }

        static bool is_null_state(Int_Key const& key)
        {
            return key == NULL_STATE;
        }
    };

    struct Not_Direct_Container {};

    #if 0
    //can be used for Slice, String, String_Builder and others
    template <typename Container>
    struct Default_Hash_Functions<Container, Enable_If<std::is_integral_v<Int_Key>>>
    {
        static uint64_t hash(Container const& key)
        {
            auto items = slice(key);
            Slice<const u8> bytes = cast_slice<const u8>(items);
            return murmur_hash64(bytes.data, bytes.size, 0);
        }

        static bool is_equal(Container const& a, Container const& b)
        {
            auto a_s = slice(a);
            auto b_s = slice(b);
            return compare(a_s, b_s) == 0;
        }

        static void set_null_state(Container* key)
        {
            *key = Container();
        }

        static bool is_null_state(Container const& key)
        {
            return std::data(key) == nullptr;
        }
    };
    #endif
}

#include "undefs.h"