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
    constexpr isize HASH_SET_MAX_UTILIZATIO_NUM = 1;
    constexpr isize HASH_SET_MAX_UTILIZATIO_DEN = 4;
    
    template <typename Key, typename Enable = Enabled>
    struct Default_Hash_Functions
    {
        static uint64_t hash(Key const& key);
        static bool is_equal(Key const& a, Key const& b);
        static void set_null_state(Key* key);
        static bool is_null_state(Key const& key);
    };

    template <typename Key_, typename Value_, typename Hash_Functions = Default_Hash_Functions<Key_>>
    struct Marker_Hash_Table
    {
        using Key = Key_;
        using Value = Value_;

        Allocator* allocator = memory_globals::default_allocator();
        Key* keys = nullptr;
        Value* values = nullptr;
        isize size = 0;
        isize used = 0; //number of current set key-value pairs
        bool null_state_used = false;

        Marker_Hash_Table() noexcept = default;
        Marker_Hash_Table(Allocator* alloc) noexcept : allocator(alloc) {}
        Marker_Hash_Table(Marker_Hash_Table&& other) noexcept;
        Marker_Hash_Table(Marker_Hash_Table const& other) = delete;
        ~Marker_Hash_Table() noexcept;

        Marker_Hash_Table& operator=(Marker_Hash_Table&& other) noexcept;
        Marker_Hash_Table& operator=(Marker_Hash_Table const& other) = delete;
    };
    
    template <typename Key, typename Value, typename Fns> nodisc
    bool is_invariant(Marker_Hash_Table<Key, Value, Fns> const& hash)
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
    Slice<Key> keys(Marker_Hash_Table<Key, Value, Fns>* hash)
    {
        Slice<Key> out = {hash->keys, hash->size};
        return out;
    }
    
    template <typename Key, typename Value, typename Fns> nodisc
    Slice<const Key> keys(Marker_Hash_Table<Key, Value, Fns> const& hash)
    {
        Slice<const Key> out = {hash.keys, hash.size};
        return out;
    }
    
    template <typename Key, typename Value, typename Fns> nodisc
    Slice<Value> values(Marker_Hash_Table<Key, Value, Fns>* hash)
    {
        Slice<Value> out = {hash->values, hash->size};
        return out;
    }
    
    template <typename Key, typename Value, typename Fns> nodisc
    Slice<const Value> values(Marker_Hash_Table<Key, Value, Fns> const& hash)
    {
        Slice<const Value> out = {hash.values, hash.size};
        return out;
    }
    
    template <typename Key, typename Value, typename Fns> nodisc
    bool is_used(Marker_Hash_Table<Key, Value, Fns> const& hash, isize index)
    {
        assert(index >= 0 && index < hash.size && "index out of bounds!");

        Key const& key = hash.keys[index];
        bool is_used = Fns::is_null_state(key) == false;
        bool is_null_state_used = index == 0 && hash.null_state_used;
            
        return is_used || is_null_state_used;
    }
    
    template <typename Key, typename Value, typename Fns> nodisc
    bool is_empty(Marker_Hash_Table<Key, Value, Fns> const& hash, isize index)
    {
        return is_used(hash, index) == false;
    }

    template <typename Key, typename Value, typename Fns>
    void swap(Marker_Hash_Table<Key, Value, Fns>* left, Marker_Hash_Table<Key, Value, Fns>* right) noexcept
    {
        swap(&left->allocator, &right->allocator);
        swap(&left->keys, &right->keys);
        swap(&left->values, &right->values);
        swap(&left->size, &right->size);
        swap(&left->used, &right->used);
        swap(&left->null_state_used, &right->null_state_used);
    }

    template <typename Key, typename Value, typename Fns>
    Marker_Hash_Table<Key, Value, Fns>::Marker_Hash_Table(Marker_Hash_Table && other) noexcept 
    {
        assert(is_invariant(*this));
        assert(is_invariant(other));

        swap(this, &other);
    }
    
    template <typename Key, typename Value, typename Fns>
    Marker_Hash_Table<Key, Value, Fns>::~Marker_Hash_Table() noexcept 
    {
        assert(is_invariant(*this));

        if(this->keys != nullptr)
        {
            assert(this->values != nullptr);

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
    Marker_Hash_Table<Key, Value, Fns>& Marker_Hash_Table<Key, Value, Fns>::operator=(Marker_Hash_Table && other) noexcept 
    {
        assert(is_invariant(*this));
        assert(is_invariant(other));

        swap(this, &other);
        return *this;
    }

    template <typename Key, typename Value, typename Fns> nodisc
    isize find_entry(Marker_Hash_Table<Key, Value, Fns> const& hash, no_infer(Key) const& key)
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
    bool has(Marker_Hash_Table<Key, Value, Hash> const& table, no_infer(Key) const& key) noexcept
    {
        return find_entry(table, key) != -1;
    }

    template <typename Key, typename Value, typename Hash> nodisc
    Value const& get(Marker_Hash_Table<Key, Value, Hash>const& table, no_infer(Key) const& key, no_infer(Value) const& if_not_found) noexcept
    {
        isize index = find_entry(table, key);
        if(index == -1)
            return if_not_found;

        return values(table)[index];
    }

    template <typename Key, typename Value, typename Hash> nodisc
    Value move_out(Marker_Hash_Table<Key, Value, Hash>* table, no_infer(Key) const& key, no_infer(Value) if_not_found) noexcept
    {
        isize index = find_entry(table, key);
        if(index == -1)
            return if_not_found;

        return move(&values(table)[index]);
    }


    template <typename Key, typename Value, typename Fns> nodisc
    Allocator_State_Type rehash(Marker_Hash_Table<Key, Value, Fns>* hash, isize min_size)
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
        //i from 1 => we handle the null slot seperately
        for(isize i = 1; i < hash->size; i++)
        {
            Key& key = hash->keys[i];
            if(Fns::is_null_state(key))
                continue;
            
            Value* val = &hash->values[i];

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

        Marker_Hash_Table<Key, Value, Fns> new_table(hash->allocator);
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
    Allocator_State_Type set(Marker_Hash_Table<Key, Value, Fns>* hash, no_infer(Key) key, no_infer(Value) value)
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

    //can be used for Slice, String, String_Builder and others
    template <typename T>
    struct Default_Hash_Functions<Slice<T>>
    {
        static uint64_t hash(Slice<T> const& val)
        {
            //if is scalar (array of bultin types ie char, int...) just use the optimal hash
            if constexpr(std::is_scalar_v<T>)
                return cast(hash_t) murmur_hash64(val.data, val.size * cast(isize) sizeof(T), cast(u64) val.size);
            //else mixin the hashes for each element
            else
            {
                u64 running_hash = cast(u64) val.size;
                for(isize i = 0; i < val.size; i++)
                    running_hash ^= Default_Hash_Functions<T>::hash(val);
            
                return cast(hash_t) running_hash;
            }
        }

        static bool is_equal(Slice<T> const& a, Slice<T> const& b)
        {
            if constexpr(std::is_scalar_v<T>)
                return compare(cast_slice<const u8>(a),  cast_slice<const u8>(b)) == 0;       
            else
            {
                if(a.size != b.size)
                    return false;
                
                for(isize i = 0; i < a.size; i++)
                {
                    if(Default_Hash_Functions<T>::is_equal(a[i], b[i]) == false)
                        return false;
                }

                return true;
            }
        }

        static void set_null_state(Slice<T>* key)
        {
            *key = Slice<T>();
        }

        static bool is_null_state(Slice<T> const& key)
        {
            return std::data(key) == nullptr;
        }
    };
}

#include "undefs.h"