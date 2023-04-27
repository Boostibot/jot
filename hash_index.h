#pragma once

#include "memory.h"
#include "hash.h"
#include "array.h"
#include <string.h>

#define REHASH_AT_FULLNESS 2

using hash_int_t = uint64_t;
hash_int_t hash_index(hash_int_t index)
{
    //return jot::hash_32(index);
    return jot::hash_64(index);
}

namespace jot
{

    struct Hash64_Entry
    {
        hash_int_t hash;
        hash_int_t key;
    };

    struct Hash_Inline
    {
        Hash64_Entry* _data = nullptr;
        Allocator* _allocator = default_allocator();
        uint32_t _capacity = 0;
        uint32_t _size = 0;
        uint32_t _hash_collisions = 0;
        uint32_t _removed_count = 0;
        bool _is_multiplicit = false;

        ~Hash_Inline()
        {
            if(_data != nullptr)
                _allocator->deallocate(_data, _capacity*sizeof(Hash64_Entry), 8, GET_LINE_INFO());
        }
    };
    

    isize size(Hash_Inline const& hash) noexcept;
    isize capacity(Hash_Inline const& hash) noexcept;
    bool is_invariant(Hash_Inline const& hash) noexcept;
    bool is_multiplicit(Hash_Inline const& hash) noexcept;
    isize hash_collisions(Hash_Inline const& hash) noexcept;

    hash_int_t set(Hash_Inline* table, hash_int_t hash, hash_int_t key);
    hash_int_t get(Hash_Inline const& table, hash_int_t hash, hash_int_t if_not_found) noexcept;
    hash_int_t find(Hash_Inline const& table, hash_int_t hash) noexcept;

    hash_int_t insert(Hash_Inline* table, hash_int_t hash, hash_int_t key);
    bool remove(Hash_Inline* table, hash_int_t hash) noexcept;
    void grow_if_overfull(Hash_Inline* table);
    isize rehash_failing(Hash_Inline* hash, isize new_capacity) noexcept;
    void rehash(Hash_Inline* hash, isize new_capacity);

    isize size(Hash_Inline const& hash) noexcept
    {
        return hash._size;
    }
    
    isize capacity(Hash_Inline const& hash) noexcept
    {
        return hash._capacity;
    }

    bool is_invariant(Hash_Inline const& hash) noexcept
    {
        bool is_alloc_valid = (hash._allocator != nullptr);
        bool is_power_of_two_ = is_power_of_two(hash._capacity) || hash._capacity == 0;
        bool is_capacity_valid = hash._capacity >= hash._size && hash._capacity >= hash._removed_count;
        bool is_data_valid = (hash._data == nullptr) == (hash._capacity == 0);
        return is_power_of_two_ && is_alloc_valid && is_capacity_valid && is_data_valid;
    }
    
    bool is_multiplicit(Hash_Inline const& hash) noexcept
    {
        return hash._is_multiplicit;
    }
    
    isize hash_collisions(Hash_Inline const& hash) noexcept
    {
        return hash._hash_collisions;
    }

    isize rehash_failing(Hash_Inline* hash, isize new_capacity) noexcept
    {
        assert(is_invariant(*hash));
        assert(is_power_of_two(new_capacity) || new_capacity == 0);
        assert(new_capacity >= hash->_size && new_capacity >= hash->_capacity);

        const isize ALIGN = 8;
        const isize ENTRY_SIZE = (isize) sizeof(Hash64_Entry);
        
        Hash64_Entry* old_data = hash->_data;
        Hash64_Entry* new_data = (Hash64_Entry*) hash->_allocator->allocate(new_capacity*ENTRY_SIZE, ALIGN, GET_LINE_INFO());
        if(new_data == nullptr)
            return new_capacity*ENTRY_SIZE;

        memset(new_data, 0, new_capacity*ENTRY_SIZE);

        hash_int_t mask = new_capacity - 1;
        uint32_t hash_collisions = 0;
        bool is_multiplicit = false;
        for(isize i = 0; i < hash->_capacity; i++)
        {
            if(old_data[i].hash <= 1)
                continue;

            hash_int_t place_to = old_data[i].hash & mask;
            //If is new slot not empty find appropriate empty slot
            if(new_data[place_to].hash != 0)
            {
                //if contains exactly the hash then it must be duplicit hash
                if(new_data[place_to].hash == old_data[i].hash)
                    is_multiplicit = true;

                //Find empty
                isize counter = 0;
                while(new_data[place_to].hash != 0)
                {
                    assert(counter ++ < hash->_capacity);
                    place_to = (place_to + 1) & mask;
                }

                hash_collisions ++;
            }

            assert(new_data[place_to].hash == 0);
            new_data[place_to].hash = old_data[i].hash;
            new_data[place_to].key = old_data[i].key;
        }

        if(old_data != nullptr)
            hash->_allocator->deallocate(old_data, hash->_capacity*ENTRY_SIZE, ALIGN, GET_LINE_INFO());

        hash->_data = new_data;
        hash->_capacity = (uint32_t) new_capacity;
        hash->_hash_collisions = hash_collisions;
        hash->_is_multiplicit = is_multiplicit;
        hash->_removed_count = 0;

        assert(is_invariant(*hash));
        return 0;
    }
    
    void rehash(Hash_Inline* hash, isize new_capacity)
    {
        isize failed_bytes_requested = rehash_failing(hash, new_capacity);
        if(failed_bytes_requested != 0)
        {
            memory_globals::out_of_memory_hadler()(GET_LINE_INFO(),
                "Hash_Inline allocation failed! Attempted to allocated %t bytes from allocator %p",
                failed_bytes_requested, hash->_allocator);
        }
    }
    
    void grow_if_overfull(Hash_Inline* table)
    {
        if(table->_size * REHASH_AT_FULLNESS >= table->_capacity || table->_removed_count * REHASH_AT_FULLNESS >= table->_capacity)
        {
            isize new_size = 0;
            if(table->_size * REHASH_AT_FULLNESS >= table->_capacity)
                new_size = table->_capacity * 2;

            if(table->_removed_count * REHASH_AT_FULLNESS >= table->_capacity)
                new_size = table->_capacity;

            new_size = max(new_size, 8);
            rehash(table, new_size);
        }
    }

    hash_int_t get(Hash_Inline const& table, hash_int_t hash, hash_int_t if_not_found) noexcept
    {
        assert(is_invariant(table));
        if(hash < 2)
            hash += 2;

        if(table._capacity == 0)
            return if_not_found;
        
        hash_int_t mask = (hash_int_t) table._capacity - 1;
        isize counter = 0;
        for(hash_int_t i = hash & mask; table._data[i].hash > 1; i = (i + 1) & mask)
        {
            assert(counter ++ < table._capacity);
            if(table._data[i].hash == hash)
                return table._data[i].key;
        }

        return if_not_found;
    }

    hash_int_t set(Hash_Inline* table, hash_int_t hash, hash_int_t key)
    {
        assert(is_invariant(*table));
        if(hash < 2)
            hash += 2;
        
        grow_if_overfull(table);

        hash_int_t mask = (hash_int_t) table->_capacity - 1;
        hash_int_t i = hash & mask;
        if(table->_data[i].hash == hash)
        {
           table->_data[i].key = key;
           return i;
        }
        
        isize counter = 0;
        for(; table->_data[i].hash > 1; i = (i + 1) & mask)
            assert(counter ++ < table->_capacity);
        
        if(i != (hash & mask) && table->_data[i].hash != 1)
            table->_hash_collisions ++;

        table->_data[i].hash = hash;
        table->_data[i].key = key;
        table->_size ++;
        assert(is_invariant(*table));
        return i;
    }

    hash_int_t insert(Hash_Inline* table, hash_int_t hash, hash_int_t key)
    {
        assert(is_invariant(*table));
        if(hash < 2)
            hash += 2;

        grow_if_overfull(table);
        
        hash_int_t mask = (hash_int_t) table->_capacity - 1;
        hash_int_t i = hash & mask;
        if(table->_data[i].hash == hash)
            table->_is_multiplicit = true;

        isize counter = 0;
        for(;table->_data[i].hash > 1; i = (i + 1) & mask)
            assert(counter ++ < table->_capacity);

        if(i != (hash & mask) && table->_data[i].hash != 1)
            table->_hash_collisions ++;
        
        table->_data[i].hash = hash;
        table->_data[i].key = key;
        table->_size ++;
        assert(is_invariant(*table));
        return i;
    }

    hash_int_t find(Hash_Inline const& table, hash_int_t hash) noexcept
    {
        if(hash < 2)
            hash += 2;

        if(table._capacity == 0)
            return -1;
        
        hash_int_t mask = (hash_int_t) table._capacity - 1;
        isize counter = 0;
        for(hash_int_t i = hash & mask; table._data[i].hash; i = (i + 1) & mask)
        {
            assert(counter ++ < table._capacity);
            if(table._data[i].hash == hash)
                return i;
        }

        return (hash_int_t) -1;
    }
    
    bool remove(Hash_Inline* table, hash_int_t hash) noexcept
    {
        assert(is_invariant(*table));
        isize index = find(*table, hash);
        if(index == -1)
            return false;

        table->_data[index].hash = 1;
        table->_size --;
        table->_removed_count ++;
        assert(is_invariant(*table));
        return true;
    }
}

#if 0
namespace jot::b
{
    struct Hash_Micro
    {
        Array<hash_int_t> _hashes;
        void* _data = nullptr;
        uint32_t _linker_size = 0;
        uint32_t _hash_collisions = 0;
        uint32_t _removed_count = 0;
        bool _is_multiplicit = false;

        ~Hash_Micro()
        {
            if(_data != nullptr)
                _allocator->deallocate(_data, _capacity*sizeof(Hash64_Entry), 8, GET_LINE_INFO());
        }
    };
    
    hash_int_t hash_index(hash_int_t index)
    {
        //return hash_32(index);
        return hash_64(index);
    }

    isize size(Hash_Micro const& hash) noexcept
    {
        return hash._size;
    }
    
    isize capacity(Hash_Micro const& hash) noexcept
    {
        return hash._capacity;
    }

    bool is_invariant(Hash_Micro const& hash) noexcept
    {
        bool is_alloc_valid = (hash._allocator != nullptr);
        bool is_power_of_two_ = is_power_of_two(hash._capacity) || hash._capacity == 0;
        bool is_capacity_valid = hash._capacity >= hash._size && hash._capacity >= hash._removed_count;
        bool is_data_valid = (hash._data == nullptr) == (hash._capacity == 0);
        return is_power_of_two_ && is_alloc_valid && is_capacity_valid && is_data_valid;
    }
    
    bool is_multiplicit(Hash_Micro const& hash) noexcept
    {
        return hash._is_multiplicit;
    }
    
    isize hash_collisions(Hash_Micro const& hash) noexcept
    {
        return hash._hash_collisions;
    }

    isize rehash_failing(Hash_Micro* hash, isize new_capacity) noexcept
    {
        assert(is_invariant(*hash));
        assert(is_power_of_two(new_capacity) || new_capacity == 0);
        assert(new_capacity >= hash->_size && new_capacity >= hash->_capacity);

        const isize ALIGN = 8;
        const isize ENTRY_SIZE = (isize) sizeof(Hash64_Entry);
        
        Hash64_Entry* old_data = hash->_data;
        Hash64_Entry* new_data = (Hash64_Entry*) hash->_allocator->allocate(new_capacity*ENTRY_SIZE, ALIGN, GET_LINE_INFO());
        if(new_data == nullptr)
            return new_capacity*ENTRY_SIZE;

        memset(new_data, 0, new_capacity*ENTRY_SIZE);

        hash_int_t mask = new_capacity - 1;
        uint32_t hash_collisions = 0;
        bool is_multiplicit = false;
        for(isize i = 0; i < hash->_capacity; i++)
        {
            if(old_data[i].hash <= 1)
                continue;

            hash_int_t place_to = old_data[i].hash & mask;
            //If is new slot not empty find appropriate empty slot
            if(new_data[place_to].hash != 0)
            {
                //if contains exactly the hash then it must be duplicit hash
                if(new_data[place_to].hash == old_data[i].hash)
                    is_multiplicit = true;

                //Find empty
                isize counter = 0;
                while(new_data[place_to].hash != 0)
                {
                    assert(counter ++ < hash->_capacity);
                    place_to = (place_to + 1) & mask;
                }

                hash_collisions ++;
            }

            assert(new_data[place_to].hash == 0);
            new_data[place_to].hash = old_data[i].hash;
            new_data[place_to].key = old_data[i].key;
        }

        if(old_data != nullptr)
            hash->_allocator->deallocate(old_data, hash->_capacity*ENTRY_SIZE, ALIGN, GET_LINE_INFO());

        hash->_data = new_data;
        hash->_capacity = (uint32_t) new_capacity;
        hash->_hash_collisions = hash_collisions;
        hash->_is_multiplicit = is_multiplicit;
        hash->_removed_count = 0;

        assert(is_invariant(*hash));
        return 0;
    }
    
    void rehash(Hash_Micro* hash, isize new_capacity)
    {
        isize failed_bytes_requested = rehash_failing(hash, new_capacity);
        if(failed_bytes_requested != 0)
        {
            memory_globals::out_of_memory_hadler()(GET_LINE_INFO(),
                "Hash_Micro allocation failed! Attempted to allocated %t bytes from allocator %p",
                failed_bytes_requested, hash->_allocator);
        }
    }
    
    void grow_if_overfull(Hash_Micro* table)
    {
        if(table->_size * REHASH_AT_FULLNESS >= table->_capacity || table->_removed_count * REHASH_AT_FULLNESS >= table->_capacity)
        {
            isize new_size = 0;
            if(table->_size * REHASH_AT_FULLNESS >= table->_capacity)
                new_size = table->_capacity * 2;

            if(table->_removed_count * REHASH_AT_FULLNESS >= table->_capacity)
                new_size = table->_capacity;

            new_size = max(new_size, 8);
            rehash(table, new_size);
        }
    }

    hash_int_t get(Hash_Micro const& table, hash_int_t hash, hash_int_t if_not_found) noexcept
    {
        assert(is_invariant(table));
        if(hash < 2)
            hash += 2;

        if(table._capacity == 0)
            return if_not_found;
        
        hash_int_t mask = (hash_int_t) table._capacity - 1;
        isize counter = 0;
        for(hash_int_t i = hash & mask; table._data[i].hash > 1; i = (i + 1) & mask)
        {
            assert(counter ++ < table._capacity);
            if(table._data[i].hash == hash)
                return table._data[i].key;
        }

        return if_not_found;
    }

    hash_int_t set(Hash_Micro* table, hash_int_t hash, hash_int_t key)
    {
        assert(is_invariant(*table));
        if(hash < 2)
            hash += 2;
        
        grow_if_overfull(table);

        hash_int_t mask = (hash_int_t) table->_capacity - 1;
        hash_int_t i = hash & mask;
        if(table->_data[i].hash == hash)
        {
           table->_data[i].key = key;
           return i;
        }
        
        isize counter = 0;
        for(; table->_data[i].hash > 1; i = (i + 1) & mask)
            assert(counter ++ < table->_capacity);
        
        if(i != (hash & mask) && table->_data[i].hash != 1)
            table->_hash_collisions ++;

        table->_data[i].hash = hash;
        table->_data[i].key = key;
        table->_size ++;
        assert(is_invariant(*table));
        return i;
    }

    hash_int_t insert(Hash_Micro* table, hash_int_t hash, hash_int_t key)
    {
        assert(is_invariant(*table));
        if(hash < 2)
            hash += 2;

        grow_if_overfull(table);
        
        hash_int_t mask = (hash_int_t) table->_capacity - 1;
        hash_int_t i = hash & mask;
        if(table->_data[i].hash == hash)
            table->_is_multiplicit = true;

        isize counter = 0;
        for(;table->_data[i].hash > 1; i = (i + 1) & mask)
            assert(counter ++ < table->_capacity);

        if(i != (hash & mask) && table->_data[i].hash != 1)
            table->_hash_collisions ++;
        
        table->_data[i].hash = hash;
        table->_data[i].key = key;
        table->_size ++;
        assert(is_invariant(*table));
        return i;
    }

    hash_int_t find(Hash_Micro const& table, hash_int_t hash) noexcept
    {
        if(hash < 2)
            hash += 2;

        if(table._capacity == 0)
            return -1;
        
        hash_int_t mask = (hash_int_t) table._capacity - 1;
        isize counter = 0;
        for(hash_int_t i = hash & mask; table._data[i].hash; i = (i + 1) & mask)
        {
            assert(counter ++ < table._capacity);
            if(table._data[i].hash == hash)
                return i;
        }

        return (hash_int_t) -1;
    }
    
    bool remove(Hash_Micro* table, hash_int_t hash) noexcept
    {
        assert(is_invariant(*table));
        isize index = find(*table, hash);
        if(index == -1)
            return false;

        table->_data[index].hash = 1;
        table->_size --;
        table->_removed_count ++;
        assert(is_invariant(*table));
        return true;
    }
}
#endif
