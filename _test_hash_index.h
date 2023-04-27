#pragma once

#include <random>
#include "_test.h"
#include "string_hash.h"
#include "hash_index.h"

namespace jot::tests::hash_index
{
    void test_insert_remove_get_set()
    {
        using int_h = hash_int_t;
        isize mem_before = default_allocator()->get_stats().bytes_allocated;

        {
            Hash_Inline hash;
            TEST(size(hash) == 0); 
            TEST(capacity(hash) == 0); 
            TEST(is_multiplicit(hash) == false); 

            int_h i2 = insert(&hash, 2, 2); 
            int_h i3 = insert(&hash, 3, 3); 
            int_h i4 = insert(&hash, 4, 4); 
        
            TEST(size(hash) == 3);
            TEST(capacity(hash) >= 3);
            TEST(is_multiplicit(hash) == false);

            TEST(hash._data[i2].key == 2);
            TEST(hash._data[i3].key == 3);
            TEST(hash._data[i4].key == 4);

            TEST(get(hash, 2, -1) == 2);
            TEST(get(hash, 3, -1) == 3);
            TEST(get(hash, 4, -1) == 4);

            TEST(get(hash, 0, -1) == 2);
            TEST(get(hash, 5, -1) == -1);
            TEST(get(hash, 10931, -1) == -1);
            
            TEST(remove(&hash, 3));
            TEST(get(hash, 2, -1) == 2);
            TEST(get(hash, 3, -1) == -1);
            TEST(get(hash, 4, -1) == 4);
            TEST(is_multiplicit(hash) == false);

            int_h i5 = insert(&hash, 5, 5); 
            int_h i6 = insert(&hash, 6, 6); 
            int_h i7 = insert(&hash, 7, 7); 
            int_h i8 = insert(&hash, 8, 8); 
            int_h i9 = insert(&hash, 9, 9); 
            int_h i10 = insert(&hash, 10, 10);
            TEST(is_multiplicit(hash) == false);
            TEST(size(hash) == 8);
            TEST(capacity(hash) >= 8);

            TEST(get(hash, 2, -1) == 2);
            TEST(get(hash, 3, -1) == -1);
            TEST(get(hash, 4, -1) == 4);
            TEST(get(hash, 7, -1) == 7);
            TEST(get(hash, 9, -1) == 9);
            TEST(get(hash, 10, -1) == 10);

            TEST(remove(&hash, 6));
            TEST(remove(&hash, 7));
            TEST(remove(&hash, 3) == false);
            TEST(remove(&hash, 10));
            TEST(size(hash) == 5);
            TEST(capacity(hash) >= 8);
            
            TEST(i10 == set(&hash, 10, 11));

            TEST(get(hash, 4, -1) == 4);
            TEST(get(hash, 7, -1) == -1);
            TEST(get(hash, 3, -1) == -1);
            TEST(get(hash, 10, -1) == 11);

            TEST(i10 != set(&hash, 3, 12));
            TEST(get(hash, 3, -1) == 12);
        }

        isize mem_after = default_allocator()->get_stats().bytes_allocated;
        TEST(mem_before == mem_after);
    }
}