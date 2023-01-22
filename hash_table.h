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
    
    template <typename Key, typename Enable = True>
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
        ~Hash_Table() noexcept;

        Hash_Table& operator=(Hash_Table&& other) noexcept;
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
            Slice<Key> keys = jot::keys(this);
            Slice<Value> value = jot::values(this);

            for(isize i = 0; i < this->size; i++)
            {
                if(is_used(*this, i))
                    value[i].~Value();

                keys[i].~Key();
            }

            //deallocate alloced
            this->allocator->deallocate(cast_slice<u8>(keys), HASH_SET_KEYS_ALIGN);
            this->allocator->deallocate(cast_slice<u8>(value), HASH_SET_KEYS_ALIGN);
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
    isize find_key(Hash_Table<Key, Value, Fns> const& hash, Key key)
    {
        assert(is_invariant(hash));

        if(hash.size == 0)
            return -1;

        u64 result = Fns::hash(key);
        u64 mask = cast(u64) hash.size - 2;

        u64 index = result & mask;
        u64 contention = 0;

        assert(hash.size >= 3 && "size must not be smaller than 3 (2 regular size)");
        for(u64 i = index;; contention++)
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
    
    template <typename Key, typename Value, typename Fns> nodisc
    Allocator_State_Type rehash(Hash_Table<Key, Value, Fns>* hash, isize min_size)
    {
        assert(is_invariant(*hash));
        constexpr isize base_elems = max(HASH_SET_BASE_BYTES / sizeof(Key), HASH_SET_BASE_SIZE);

        isize old_regular_size = hash->size - 1;
        isize new_regular_size = max(old_regular_size, base_elems);
        while(min_size - 1 > new_regular_size)
            new_regular_size *= 2;

        isize new_size = new_regular_size + 1;
        assert(is_power_of_two(new_size - 1));
        assert(new_size >= 3);

        Allocation_Result keys_res = hash->allocator->allocate(new_size * sizeof(Key), HASH_SET_KEYS_ALIGN);
        if(keys_res.state == ERROR)
            return keys_res.state;

        Allocation_Result vals_res = hash->allocator->allocate(new_size * sizeof(Value), HASH_SET_VALUES_ALIGN);
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
        
        u64 new_mask = new_size - 2;
        for(isize i = 1; i < hash->size; i++)
        {
            Key& key = hash->keys[i];
            //i from 1 => we handle the null slot seperately
            if(Fns::is_null_state(key))
                continue;
            
            Value* val = &hash->values[i + 1];

            u64 hash = Fns::hash(key);
            u64 index = hash & new_mask;
            new_keys[index + 1] = move(&key);
            new (&new_vals[index + 1]) Value(move(val));
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
    Allocator_State_Type add_or_set(Hash_Table<Key, Value, Fns>* hash, no_infer(Key) key, no_infer(Value) value)
    {
        assert(is_invariant(*hash));
        if(hash->used * HASH_SET_MAX_UTILIZATIO_DEN >= hash->size * HASH_SET_MAX_UTILIZATIO_NUM)
        {
            Allocator_State_Type state = rehash(hash, hash->size + 1);
            if(state == ERROR)
                return state;
        }

        u64 result = Fns::hash(key);
        u64 mask = cast(u64) hash->size - 2;

        u64 index = result & mask;
        u64 contention = 0;
        isize found_i = 0;
        bool first_used = false;
        
        for(u64 i = index;; contention++)
        {
            assert(contention != hash->size && "the contention must not be 100%");

            isize curr_i = i + 1;
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
    struct Default_Hash_Functions<Int_Key, std::conditional_t<std::is_integral_v<Int_Key>, True, Not_Integral_Hash>>
    {
        static constexpr Int_Key NULL_STATE = std::is_unsigned_v<Int_Key> 
            ? std::numeric_limits<Int_Key>::max() 
            : std::numeric_limits<Int_Key>::min(); 

        static uint64_t hash(Int_Key const& key)
        {
            u64 convreted = cast(u64) key;
            return uint64_hash(key);
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

    //can be used for Slice, String, String_Builder and others
    template <typename Container>
    struct Default_Hash_Functions<Container, std::conditional_t<direct_container<Container>, True, Not_Direct_Container>>
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

    namespace tests
    {
        isize tracking_objects_alive = 0;
        struct Tracking
        {
            isize val = 0;
            
            Tracking() noexcept 
                : val(0) { tracking_objects_alive++; };

            Tracking(isize val) noexcept 
                : val(val) { tracking_objects_alive++; }

            Tracking(Tracking && other) noexcept 
                : val(other.val) { tracking_objects_alive++; }

            Tracking(Tracking const& other) noexcept = delete;
            //Tracking(Tracking const& other) noexcept 
                //: val(other.val) { tracking_objects_alive++; }

            ~Tracking() noexcept 
                { tracking_objects_alive--; }

            Tracking& operator =(Tracking &&) noexcept = default;
            Tracking& operator =(Tracking const&) noexcept = default;

            bool operator ==(Tracking const& other) const noexcept { return val == other.val; };
            bool operator !=(Tracking const& other) const noexcept { return val != other.val; };

            static isize alive_count() noexcept { return tracking_objects_alive; }
        };
    
        template <typename Key>
        struct Test_Int_Hash_Functions
        {
            static uint64_t hash(Key const& key) {return cast(u64) key;}
            static bool is_equal(Key const& a, Key const& b) {return a == b;}
            static void set_null_state(Key* key) { *key = 0; }
            static bool is_null_state(Key const& key) {return key == 0; }
        };
        
        struct Test_Tracking_Hash_Functions
        {
            static uint64_t hash(Tracking const& key) {return cast(u64) key.val;}
            static bool is_equal(Tracking const& a, Tracking const& b) {return a.val == b.val;}
            static void set_null_state(Tracking* key) { key->val = 0; }
            static bool is_null_state(Tracking const& key) {return key.val == 0; }
        };
        
        template <typename Key, typename Value, typename Fns> nodisc
        bool value_matches_at(Hash_Table<Key, Value, Fns> const& hash, no_infer(Key) key, no_infer(Value) value, bool is_empty = false, bool fail = false)
        {
            isize found = find_key(hash, move(&key));
            if(found == -1)
                return false;

            Value const& val = values(hash)[found];
            return val == value;
        }

        template <typename Key, typename Value, typename Fns> nodisc
        bool empty_at(Hash_Table<Key, Value, Fns> const& hash, no_infer(Key) key)
        {
            return find_key(hash, move(&key)) == -1;
        }
        
        template <typename Key, typename Value, typename Fns> 
        void test_hash_type()
        {
            isize alive_before = Tracking::alive_count();
            {
                Hash_Table<Key, Value, Fns> hash;

                force(empty_at(hash, 1));
                force(empty_at(hash, 101));
                force(empty_at(hash, 0));

                force(add_or_set(&hash, 1, 10));
                force(empty_at(hash, 1) == false);
                force(value_matches_at(hash, 1, 10));
                force(value_matches_at(hash, 1, 100) == false);
            
                force(empty_at(hash, 101));
                force(empty_at(hash, 0));

                force(empty_at(hash, 0));
                force(empty_at(hash, 2));
                force(empty_at(hash, 133));

                force(add_or_set(&hash, 3, 30));
                force(add_or_set(&hash, 2, 20));

                force(value_matches_at(hash, 1, 10));
                force(empty_at(hash, 442120));
                force(value_matches_at(hash, 2, 20));
                force(empty_at(hash, 654351));
                force(value_matches_at(hash, 3, 30));
                force(empty_at(hash, 5));

                force(add_or_set(&hash, 15, 15));
                force(add_or_set(&hash, 31, 15));
        
                force(add_or_set(&hash, 0, 100));
                force(value_matches_at(hash, 0, 100));
                force(add_or_set(&hash, 0, 1000));
                force(value_matches_at(hash, 0, 1000));
                force(value_matches_at(hash, 0, 100) == false);
                force(empty_at(hash, 5));
            }
            
            isize alive_after = Tracking::alive_count();
            force(alive_before == alive_after);
        }
        
        void test_hash()
        {
            test_hash_type<u64, i32,            Test_Int_Hash_Functions<u64>>();
            test_hash_type<u32, u32,            Default_Hash_Functions<u32>>();
            test_hash_type<u32, Tracking,       Default_Hash_Functions<u32>>();
            test_hash_type<Tracking, u32,       Test_Tracking_Hash_Functions>();
            test_hash_type<Tracking, Tracking,  Test_Tracking_Hash_Functions>();
        }
    }
}

#include "undefs.h"