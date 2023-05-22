#pragma once

#include "memory.h"

//A set of functions for creating lookup hashes into tables. These are just the bare jump tables that can be found
// in evey hash table implementation and nothing more. This is great if have wide tables and need to hash by mutilple different colums.
// These hash indeces are ultra fast as well. For more info see example at the end of the file.

namespace jot
{
    template<typename hash_t>
    struct Hash_Index
    {
        hash_t entry;
        hash_t hash;
    };

    template<typename hash_t, typename Fn>
    Hash_Index<hash_t> find_hash(const hash_t* indeces, isize indeces_size, hash_t hash, const Fn compare_at_i) noexcept
    {
        if(indeces_size <= 0)
            return Hash_Index<hash_t>{-1, -1};

        assert(is_power_of_two(indeces_size));
        hash_t mask = (hash_t) indeces_size - 1;
        hash_t i = hash & mask;
        isize counter = 0;
        for(; indeces[i] > 0; i = (i + 1) & mask)
        {
            assert(counter ++ < indeces_size);
            Hash_Index<hash_t> curr = {indeces[i] - 2, i};
            if(compare_at_i(curr))
                return curr;
        }

        return Hash_Index<hash_t>{-1, -1};
    }

    template<typename hash_t, typename Fn>
    Hash_Index<hash_t> find_next_hash(const hash_t* indeces, isize indeces_size, Hash_Index<hash_t> prev, const Fn compare_at_i) noexcept
    {
        return find_hash(indeces, indeces_size, prev.hash + 1, compare_at_i);
    }   

    template<typename hash_t, typename Fn>
    isize rehash(hash_t** indeces, isize indeces_size, isize old_capacity, isize new_capacity , Allocator* alloc, const Fn hash_at_i) noexcept
    {
        const isize ENTRY_SIZE = (isize) sizeof(hash_t);
        
        hash_t* old_data = *indeces;
        hash_t* new_data = nullptr;
        if(new_capacity != 0)
        {
            if(new_capacity <= indeces_size)
                new_capacity = indeces_size + 1;

            if(is_power_of_two(new_capacity) == false)
            {
                isize corrected = 8;
                while(corrected < new_capacity)
                    corrected *= 2;

                new_capacity = corrected;
            }

            new_data = (hash_t*) alloc->allocate(new_capacity*ENTRY_SIZE, 8, GET_LINE_INFO());
            if(new_data == nullptr)
                return new_capacity*ENTRY_SIZE;

            memset(new_data, 0, new_capacity*ENTRY_SIZE);
        }

        hash_t mask = (hash_t) new_capacity - 1;
        for(isize i = 0; i < old_capacity; i++)
        {
            if(old_data[i] <= 1)
                continue;
            
            Hash_Index<hash_t> curr = {indeces[i] - 2, i};
            hash_t hash = hash_at_i(curr);
            hash_t k = hash & mask;
            isize counter = 0;
            for(; indeces[k] > 0; k = (k + 1) & mask)
                assert(counter ++ < old_capacity);

            new_data[k] = old_data[i];
        }

        if(old_data != nullptr)
            alloc->deallocate(old_data, old_capacity*ENTRY_SIZE, 8, GET_LINE_INFO());

        *indeces = new_data;
        return 0;
    }

    isize calculate_hash_growth(isize size, isize capacity) noexcept
    {
        const isize FILLED_DEN = 2;
        const isize FILLED_NUM = 1;
        const isize BASE_SIZE = 8;
        if(size * FILLED_NUM < capacity * FILLED_DEN)
            return capacity;

        isize new_capacity = capacity * 2;
        new_capacity = max(new_capacity, BASE_SIZE);

        return new_capacity;
    }
    

    template<typename hash_t>
    isize insert_hash(hash_t* indeces, isize indeces_size, hash_t hash, hash_t point_to) noexcept
    {
        assert(is_power_of_two(indeces_size));
        hash_t mask = (hash_t) indeces_size - 1;
        hash_t i = hash & mask;
        hash_t counter = 0;
        for(; indeces[i] > 1; i = (i + 1) & mask)
            assert(counter ++ < indeces_size && "must not be completely full!");

        indeces[i] = point_to + 2;
        return i;
    }
    
    template<typename hash_t>
    bool remove_hash(hash_t* indeces, isize indeces_size, hash_t hash, hash_t index) noexcept
    {
        if(indeces_size <= 0)
            return false;

        assert(is_power_of_two(indeces_size));
        hash_t mask = (hash_t) indeces_size - 1;
        hash_t i = hash & mask;
        hash_t counter = 0;
        for(; indeces[i] > 0; i = (i + 1) & mask)
        {
            assert(counter ++ < indeces_size && "must not be completely full!");
            if(indeces[i] - 2 == index)
            {
                indeces[i] = 1;
                return true;
            }
        }

        return false;
    }


    #ifdef HASH_INDEX_EXAMPLE
    struct Row
    {
        int a = 0;
        int b = 0;
        int c = 0;
        String_Builder d;
    };

    struct Table
    {
        Slot_Array<Row> rows;
        hash_t* a_hash = nullptr;
        hash_t* b_hash = nullptr;
        hash_t* c_hash = nullptr;
        isize hashes_capacity = 0;

        ~Table() noexcept;
    };

    Handle table_insert(Table* into, Row row)
    {
        isize new_cap = calculate_hash_growth(size(into->rows), into->hashes_capacity);
        if(new_cap != into->hashes_capacity)
        {
            Allocator* alloc = allocator(into->rows);
            isize size = jot::size(into->rows);
  
            isize failed = rehash(&into->a_hash, size, into->hashes_capacity, new_cap, alloc, [&](isize i){
                Row const& row = get(into->rows, Handle{(hash_t) i});
                return hash(row.a);
            });
    
            failed += rehash(&into->b_hash, size, into->hashes_capacity, new_cap, alloc, [&](isize i){
                Row const& row = get(into->rows, Handle{(hash_t) i});
                return hash(row.b);
            });
            
            failed += rehash(&into->c_hash, size, into->hashes_capacity, new_cap, alloc, [&](isize i){
                Row const& row = get(into->rows, Handle{(hash_t) i});
                return hash(row.c);
            });

            if(failed != 0)
                memory_globals::out_of_memory_hadler()(GET_LINE_INFO(), "%t %p", failed, allocator(into->rows));

            into->hashes_capacity = new_cap;
        }
    
        auto a_ = hash(row.a);
        auto b_ = hash(row.b);
        auto c_ = hash(row.c);
        //auto d_ = int_array_hash(row.d, 0);

        Handle h = insert(&into->rows, (Row&&) row);
        insert_hash(into->a_hash, into->hashes_capacity, a_, h.index);
        insert_hash(into->b_hash, into->hashes_capacity, b_, h.index);
        insert_hash(into->c_hash, into->hashes_capacity, c_, h.index);
        //insert(&into->d_hash, d_, h.index);

        return h;
    }

    Table::~Table() noexcept
    {
        Allocator* alloc = allocator(rows);
        isize size = jot::size(rows);
        const auto idle = [](isize){return false;};

        rehash(&a_hash, size, hashes_capacity, 0, alloc, idle);
        rehash(&b_hash, size, hashes_capacity, 0, alloc, idle);
        rehash(&c_hash, size, hashes_capacity, 0, alloc, idle);
    }

    Row table_remove(Table* into, Handle row)
    {
        Row removed = remove(&into->rows, row);

        auto a_ = hash(removed.a);
        auto b_ = hash(removed.b);
        auto c_ = hash(removed.c);

        remove_hash(into->a_hash, into->hashes_capacity, a_, row.index);
        remove_hash(into->b_hash, into->hashes_capacity, b_, row.index);
        remove_hash(into->c_hash, into->hashes_capacity, c_, row.index);

        return removed;
    }

    Hash_Index<hash_t> table_get_by_a(Table const& table, int a)
    {
        Hash_Index<hash_t> found = find_hash(table.a_hash, table.hashes_capacity, hash(a), [&](isize i){
            Row const& row = get(table.rows, Handle{(hash_t) i});
            return a == row.a;
        });

        return found;
    }
    
    //The same...
    Hash_Index<hash_t> table_get_by_b(Table const& table, int b);
    Hash_Index<hash_t> table_get_by_c(Table const& table, int c);
    #endif
}