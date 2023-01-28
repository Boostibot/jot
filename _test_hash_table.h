#pragma once

#include "hash_table.h"
#include "hash_table2.h"
#include "_test.h"
#include "defines.h"

namespace jot::tests::hash_table
{
    template <typename Key, typename Value, typename Hash, template<typename, typename, typename> typename Table> 
    bool value_matches_at(Table<Key, Value, Hash> const& table, no_infer(Key) const& key, no_infer(Value) const& value)
    {
        isize found = find_entry(table, move(&key));
        if(found == -1)
            return false;

        Value const& manual = values(table)[found];
        Value const& obtained = get(table, key, value);
        
        force(manual == obtained);
        return manual == value;
    }

    template <typename Key, typename Value, typename Hash, template<typename, typename, typename> typename Table> 
    bool empty_at(Table<Key, Value, Hash> const& table, no_infer(Key) const& key)
    {
        bool manual = find_entry(table, key) == -1;
        bool obtained = has(table, key) == false;

        force(manual == obtained);
        return manual;
    }
        
    template <typename Table> 
    void test_table_add_find()
    {
        isize alive_before = trackers_alive();
        {
            Table table;

            force(empty_at(table, 1));
            force(empty_at(table, 101));
            force(empty_at(table, 0));

            force(set(&table, 1, 10));
            force(empty_at(table, 1) == false);
            force(value_matches_at(table, 1, 10));
            force(value_matches_at(table, 1, 100) == false);
            
            force(empty_at(table, 101));
            force(empty_at(table, 2));

            force(set(&table, 3, 30));
            force(set(&table, 2, 20));

            force(value_matches_at(table, 1, 10));
            force(empty_at(table, 442120));
            force(value_matches_at(table, 2, 20));
            force(empty_at(table, 654351));
            force(value_matches_at(table, 3, 30));
            force(empty_at(table, 5));

            force(set(&table, 15, 15));
            force(set(&table, 31, 15));
        
            force(set(&table, 0, 100));
            force(value_matches_at(table, 0, 100));
            force(set(&table, 0, 1000));
            force(value_matches_at(table, 0, 1000));
            force(value_matches_at(table, 0, 100) == false);
            force(empty_at(table, 5));
        }
            
        isize alive_after = trackers_alive();
        force(alive_before == alive_after);
    }
        
    template <typename Table> 
    void test_table_mark_remove()
    {
        isize alive_before = trackers_alive();
        {
            Table table;

            force(set(&table, 1, 10));
            force(set(&table, 2, 10));
            force(set(&table, 3, 10));
            force(set(&table, 4, 10));

            force(value_matches_at(table, 1, 10));
            force(value_matches_at(table, 2, 10));
            force(value_matches_at(table, 3, 10));
            force(value_matches_at(table, 4, 10));

            mark_removed(&table, 2);
            force(empty_at(table, 2));
            force(value_matches_at(table, 3, 10));
            force(value_matches_at(table, 1, 10));
            
            mark_removed(&table, 3);
            force(empty_at(table, 3));
            force(value_matches_at(table, 1, 10));
            force(value_matches_at(table, 4, 10));

            force(set(&table, 2, 20));
            force(value_matches_at(table, 2, 20));
            
            force(set(&table, 6, 60));
            force(set(&table, 7, 70));
            force(set(&table, 8, 80));
            force(set(&table, 9, 90));
            force(set(&table, 10, 100));
            
            force(value_matches_at(table, 9, 90));
            force(value_matches_at(table, 4, 10));

            mark_removed(&table, 6);
            mark_removed(&table, 7);
            mark_removed(&table, 8);
            mark_removed(&table, 9);
            mark_removed(&table, 10);
            mark_removed(&table, 10);
            mark_removed(&table, 10);

            force(empty_at(table, 6));
            force(empty_at(table, 7));
            force(empty_at(table, 8));
            force(empty_at(table, 9));
            force(empty_at(table, 10));
            
            force(value_matches_at(table, 1, 10));
            force(value_matches_at(table, 4, 10));
            force(empty_at(table, 3));

            force(set(&table, 10, 100));
            force(value_matches_at(table, 10, 100));
            
            mark_removed(&table, 1);
            force(empty_at(table, 1));
            force(value_matches_at(table, 4, 10));
        }
            
        isize alive_after = trackers_alive();
        force(alive_before == alive_after);
    }
    
    template <typename Table> 
    void test_table_remove()
    {
        isize alive_before = trackers_alive();
        {
            Table table;
                
            force(set(&table, 1, 10));
            force(set(&table, 2, 20));
            force(set(&table, 3, 30));
            force(set(&table, 4, 40));

            force(value_matches_at(table, 1, 10));
            force(value_matches_at(table, 2, 20));
            force(value_matches_at(table, 3, 30));
            force(value_matches_at(table, 4, 40));

            auto entry = remove(&table, find(table, 2));
            force(entry.key == 2 && entry.value == 20);
            force(empty_at(table, 2));
            force(value_matches_at(table, 3, 30));
            force(value_matches_at(table, 1, 10));
            
            entry = remove(&table, find(table, 3));
            force(entry.key == 3 && entry.value == 30);
            force(empty_at(table, 3));
            force(value_matches_at(table, 1, 10));
            force(value_matches_at(table, 4, 40));

            force(set(&table, 2, 20));
            force(value_matches_at(table, 2, 20));
            
            force(set(&table, 6, 60));
            force(set(&table, 7, 70));
            force(set(&table, 8, 80));
            force(set(&table, 9, 90));
            force(set(&table, 10, 100));
            
            force(value_matches_at(table, 9, 90));
            force(value_matches_at(table, 4, 40));

            entry = remove(&table, find(table, 6));
            force(entry.key == 6 && entry.value == 60);
            force(remove(&table, 6) == false);

            entry = remove(&table, find(table, 7));
            force(entry.key == 7 && entry.value == 70);
            force(remove(&table, 7) == false);

            entry = remove(&table, find(table, 8));
            force(entry.key == 8 && entry.value == 80);

            entry = remove(&table, find(table, 9));
            force(entry.key == 9 && entry.value == 90);

            entry = remove(&table, find(table, 10));
            force(entry.key == 10 && entry.value == 100);

            force(remove(&table, 7) == false);
            force(remove(&table, 10) == false);
            force(remove(&table, 3) == false);

            force(empty_at(table, 6));
            force(empty_at(table, 7));
            force(empty_at(table, 8));
            force(empty_at(table, 9));
            force(empty_at(table, 10));
            
            force(value_matches_at(table, 1, 10));
            force(value_matches_at(table, 4, 40));
            force(empty_at(table, 3));

            force(set(&table, 10, 100));
            force(value_matches_at(table, 10, 100));
            
            entry = remove(&table, find(table, 1));
            force(empty_at(table, 1));
            force(value_matches_at(table, 4, 40));
        }
            
        isize alive_after = trackers_alive();
        force(alive_before == alive_after);
    }
    
    template <typename Key>
    struct Test_Int_Hash_Functions
    {
        static uint64_t hash(Key const& key) {return cast(hash_t) key;}
        static bool is_equal(Key const& a, Key const& b) {return a == b;}
        static void set_null_state(Key* key) { *key = 0; }
        static bool is_null_state(Key const& key) {return key == 0; }
    };
        
    struct Test_Tracker_Hash_Functions
    {
        static uint64_t hash(Tracker<i32> const& key) {return cast(hash_t) key.val;}
        static bool is_equal(Tracker<i32> const& a, Tracker<i32> const& b) {return a.val == b.val;}
        static void set_null_state(Tracker<i32>* key) { key->val = 0; }
        static bool is_null_state(Tracker<i32> const& key) {return key.val == 0; }
    };

    void test_hash()
    {
        using Trc = Tracker<i32>;
        test_table_add_find<Hash_Table<hash_t, i32, Test_Int_Hash_Functions<hash_t>>>();
        test_table_add_find<Hash_Table<u32, u32, Default_Hash_Functions<u32>>>();
        test_table_add_find<Hash_Table<u32, Trc, Default_Hash_Functions<u32>>>();
        test_table_add_find<Hash_Table<Trc, u32, Test_Tracker_Hash_Functions>>();
        test_table_add_find<Hash_Table<Trc, Trc, Test_Tracker_Hash_Functions>>();

        test_table_add_find<Hash_Table2<hash_t, i32, Test_Int_Hash_Functions<hash_t>>>();
        test_table_add_find<Hash_Table2<u32, u32, Default_Hash_Functions<u32>>>();
        test_table_add_find<Hash_Table2<u32, Trc, Default_Hash_Functions<u32>>>();
        test_table_add_find<Hash_Table2<Trc, u32, Test_Tracker_Hash_Functions>>();
        test_table_add_find<Hash_Table2<Trc, Trc, Test_Tracker_Hash_Functions>>();

        test_table_mark_remove<Hash_Table2<hash_t, i32, Test_Int_Hash_Functions<hash_t>>>();
        test_table_mark_remove<Hash_Table2<u32, u32, Default_Hash_Functions<u32>>>();
        test_table_mark_remove<Hash_Table2<u32, Trc, Default_Hash_Functions<u32>>>();
        test_table_mark_remove<Hash_Table2<Trc, u32, Test_Tracker_Hash_Functions>>();
        test_table_mark_remove<Hash_Table2<Trc, Trc, Test_Tracker_Hash_Functions>>();

        test_table_remove<Hash_Table2<hash_t, i32, Test_Int_Hash_Functions<hash_t>>>();
        test_table_remove<Hash_Table2<u32, u32, Default_Hash_Functions<u32>>>();
        test_table_remove<Hash_Table2<u32, Trc, Default_Hash_Functions<u32>>>();
        test_table_remove<Hash_Table2<Trc, u32, Test_Tracker_Hash_Functions>>();
        test_table_remove<Hash_Table2<Trc, Trc, Test_Tracker_Hash_Functions>>();
    }
}

#include "undefs.h"