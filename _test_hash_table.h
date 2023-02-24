#pragma once

#include <random>

#include "_test.h"
#include "hash_table.h"
#include "string.h"
#include "defines.h"

namespace jot::tests::hash_table
{
    template <typename Table> 
    bool value_matches_at(Table const& table, typename Table::Key const& key, typename Table::Value const& value)
    {
        isize found = find_entry(table, move(&key));
        if(found == -1)
            return false;

        auto const& manual = values(table)[found];
        auto const& obtained = get(table, key, value);
        
        bool do_manual_match = Key_Comparable<typename Table::Value>::are_equal(manual, obtained);
        bool do_expected_match = Key_Comparable<typename Table::Value>::are_equal(manual, value);
        test(do_manual_match);
        return do_expected_match;
    }

    template <typename Table> 
    bool empty_at(Table const& table, typename Table::Key const& key)
    {
        bool manual = find_entry(table, key) == -1;
        bool obtained = has(table, key) == false;

        test(manual == obtained);
        return manual;
    }
        
    template <typename Table> 
    void test_table_add_find()
    {
        isize alive_before = trackers_alive();
        {
            Table table;

            test(empty_at(table, 1));
            test(empty_at(table, 101));
            test(empty_at(table, 0));

            *set(&table, 1, 10);
            test(empty_at(table, 1) == false);
            test(value_matches_at(table, 1, 10));
            test(value_matches_at(table, 1, 100) == false);
            
            test(empty_at(table, 101));
            test(empty_at(table, 2));

            *set(&table, 3, 30);
            *set(&table, 2, 20);

            test(value_matches_at(table, 1, 10));
            test(empty_at(table, 442120));
            test(value_matches_at(table, 2, 20));
            test(empty_at(table, 654351));
            test(value_matches_at(table, 3, 30));
            test(empty_at(table, 5));

            *set(&table, 15, 15);
            *set(&table, 31, 15);
        
            *set(&table, 0, 100);
            test(value_matches_at(table, 0, 100));
            *set(&table, 0, 1000);
            test(value_matches_at(table, 0, 1000));
            test(value_matches_at(table, 0, 100) == false);
            test(empty_at(table, 5));
        }
            
        isize alive_after = trackers_alive();
        test(alive_before == alive_after);
    }
        
    template <typename Table> 
    void test_table_add_find_any(Array<typename Table::Key, 10> keys, Array<typename Table::Value, 10> values)
    {
        isize alive_before = trackers_alive();
        {
            Table table;

            test(empty_at(table, keys[0]));
            test(empty_at(table, keys[3]));
            test(empty_at(table, keys[5]));

            *set(&table, dup(keys[0]), dup(values[0]));
            test(empty_at(table, keys[0]) == false);
            test(value_matches_at(table, keys[0], values[0]));
            test(value_matches_at(table, keys[0], values[1]) == false);
            
            test(empty_at(table, keys[5]));
            test(empty_at(table, keys[1]));

            *set(&table, dup(keys[2]), dup(values[2]));
            *set(&table, dup(keys[1]), dup(values[1]));

            test(value_matches_at(table, keys[0], values[0]));
            test(empty_at(table, keys[8]));
            test(value_matches_at(table, keys[1], values[1]));
            test(empty_at(table, keys[9]));
            test(value_matches_at(table, keys[2], values[2]));
            test(empty_at(table, keys[4]));

            *set(&table, dup(keys[5]), dup(values[5]));
            *set(&table, dup(keys[7]), dup(values[7]));
        
            *set(&table, dup(keys[0]), dup(values[8]));
            test(value_matches_at(table, keys[0], values[8]));
            *set(&table, dup(keys[0]), dup(values[9]));
            test(value_matches_at(table, keys[0], values[9]));
            test(value_matches_at(table, keys[0], values[8]) == false);
            test(empty_at(table, keys[4]));
        }
 
        isize alive_after = trackers_alive();
        test(alive_before == alive_after);
    }

    template <typename Table> 
    void test_table_mark_remove()
    {
        isize alive_before = trackers_alive();
        {
            Table table;

            *set(&table, 1, 10);
            *set(&table, 2, 10);
            *set(&table, 3, 10);
            *set(&table, 4, 10);

            test(value_matches_at(table, 1, 10));
            test(value_matches_at(table, 2, 10));
            test(value_matches_at(table, 3, 10));
            test(value_matches_at(table, 4, 10));

            mark_removed(&table, 2);
            test(empty_at(table, 2));
            test(value_matches_at(table, 3, 10));
            test(value_matches_at(table, 1, 10));
            
            mark_removed(&table, 3);
            test(empty_at(table, 3));
            test(value_matches_at(table, 1, 10));
            test(value_matches_at(table, 4, 10));

            *set(&table, 2, 20);
            test(value_matches_at(table, 2, 20));
            
            *set(&table, 6, 60);
            *set(&table, 7, 70);
            *set(&table, 8, 80);
            *set(&table, 9, 90);
            *set(&table, 10, 100);
            
            test(value_matches_at(table, 9, 90));
            test(value_matches_at(table, 4, 10));

            mark_removed(&table, 6);
            mark_removed(&table, 7);
            mark_removed(&table, 8);
            mark_removed(&table, 9);
            mark_removed(&table, 10);
            mark_removed(&table, 10);
            mark_removed(&table, 10);

            test(empty_at(table, 6));
            test(empty_at(table, 7));
            test(empty_at(table, 8));
            test(empty_at(table, 9));
            test(empty_at(table, 10));
            
            test(value_matches_at(table, 1, 10));
            test(value_matches_at(table, 4, 10));
            test(empty_at(table, 3));

            *set(&table, 10, 100);
            test(value_matches_at(table, 10, 100));
            
            mark_removed(&table, 1);
            test(empty_at(table, 1));
            test(value_matches_at(table, 4, 10));
        }
            
        isize alive_after = trackers_alive();
        test(alive_before == alive_after);
    }
    
    template <typename Table> 
    void test_table_remove()
    {
        isize alive_before = trackers_alive();
        {
            Table table;
                
            *set(&table, 1, 10);
            *set(&table, 2, 20);
            *set(&table, 3, 30);
            *set(&table, 4, 40);

            test(value_matches_at(table, 1, 10));
            test(value_matches_at(table, 2, 20));
            test(value_matches_at(table, 3, 30));
            test(value_matches_at(table, 4, 40));

            auto entry = remove(&table, find(table, 2));
            test(entry.key == 2 && entry.value == 20);
            test(empty_at(table, 2));
            test(value_matches_at(table, 3, 30));
            test(value_matches_at(table, 1, 10));
            
            entry = remove(&table, find(table, 3));
            test(entry.key == 3 && entry.value == 30);
            test(empty_at(table, 3));
            test(value_matches_at(table, 1, 10));
            test(value_matches_at(table, 4, 40));

            *set(&table, 2, 20);
            test(value_matches_at(table, 2, 20));
            
            *set(&table, 6, 60);
            *set(&table, 7, 70);
            *set(&table, 8, 80);
            *set(&table, 9, 90);
            *set(&table, 10, 100);
            
            test(value_matches_at(table, 9, 90));
            test(value_matches_at(table, 4, 40));

            entry = remove(&table, find(table, 6));
            test(entry.key == 6 && entry.value == 60);
            test(remove(&table, 6) == false);

            entry = remove(&table, find(table, 7));
            test(entry.key == 7 && entry.value == 70);
            test(remove(&table, 7) == false);

            entry = remove(&table, find(table, 8));
            test(entry.key == 8 && entry.value == 80);

            entry = remove(&table, find(table, 9));
            test(entry.key == 9 && entry.value == 90);

            entry = remove(&table, find(table, 10));
            test(entry.key == 10 && entry.value == 100);

            test(remove(&table, 7) == false);
            test(remove(&table, 10) == false);
            test(remove(&table, 3) == false);

            test(empty_at(table, 6));
            test(empty_at(table, 7));
            test(empty_at(table, 8));
            test(empty_at(table, 9));
            test(empty_at(table, 10));
            
            test(value_matches_at(table, 1, 10));
            test(value_matches_at(table, 4, 40));
            test(empty_at(table, 3));

            *set(&table, 10, 100);
            test(value_matches_at(table, 10, 100));
            
            entry = remove(&table, find(table, 1));
            test(empty_at(table, 1));
            test(value_matches_at(table, 4, 40));
        }
            
        isize alive_after = trackers_alive();
        test(alive_before == alive_after);
    }

    template <typename Key>
    struct Test_Int_Hash_Functions
    {
        static uint64_t hash(Key const& key) {return cast(hash_t) key;}
    };
        
    struct Test_Tracker_Hash_Functions
    {
        static uint64_t hash(Tracker<i32> const& key) {return cast(hash_t) key.val;}
    };
    
    void test_many_add()
    {
        
        using Track = Tracker<i32>;
        using Key = Track;
        using Val = Track;
        using Table = Hash_Table<Key, Val, Test_Tracker_Hash_Functions>;

        i64 before = trackers_alive();
        {
            Table table;
            *set(&table, Key{1}, Val{1});
            *set(&table, Key{2}, Val{2});
            *set(&table, Key{3}, Val{3});
            *set(&table, Key{4}, Val{4});
            *set(&table, Key{5}, Val{5});
            *set(&table, Key{6}, Val{6});
            *set(&table, Key{7}, Val{7});
            *set(&table, Key{8}, Val{8});
            *set(&table, Key{9}, Val{9});
            *set(&table, Key{10}, Val{10});
            
            *set(&table, Key{11}, Val{11});
            *set(&table, Key{12}, Val{12});
            *set(&table, Key{13}, Val{13});
            *set(&table, Key{14}, Val{14});
            *set(&table, Key{15}, Val{15});
            *set(&table, Key{16}, Val{16});
            *set(&table, Key{17}, Val{17});
            *set(&table, Key{18}, Val{18});
            *set(&table, Key{19}, Val{19});
            *set(&table, Key{20}, Val{20});

            *set(&table, Key{21}, Val{21});
            *set(&table, Key{22}, Val{22});
            *set(&table, Key{23}, Val{23});
            *set(&table, Key{24}, Val{24});
            *set(&table, Key{25}, Val{25});
            *set(&table, Key{26}, Val{26});
            *set(&table, Key{27}, Val{27});
            *set(&table, Key{28}, Val{28});
            *set(&table, Key{29}, Val{29});
            *set(&table, Key{30}, Val{30});
        }
        i64 after = trackers_alive();
        test(before == after);
    }

    void test_stress()
    {
        using Track = Tracker<i32>;
        using Key = Track;
        using Val = Track;
        using Table = Hash_Table<Key, Val, Test_Tracker_Hash_Functions>;
        using Seed = std::random_device::result_type;

        std::random_device rd;
        
        constexpr isize OP_SET1 = 0;
        constexpr isize OP_SET2 = 1;
        constexpr isize OP_SET3 = 2;
        constexpr isize OP_REMOVE = 3;
        constexpr isize OP_MARK_REMOVED = 4;
        constexpr isize OP_RESERVE_ENTRIES = 5;
        constexpr isize OP_RESERVE_JUMP_TABLE = 6;

        std::uniform_int_distribution<unsigned> op_distribution(0, 6);
        std::uniform_int_distribution<unsigned> index_distribution(0);
        std::uniform_int_distribution<unsigned> val_distribution(0);

        isize max_size = 500;
        
        std::mt19937 gen;
        const auto test_batch = [&](isize block_size, bool do_mark_removed){
            i64 before = trackers_alive();

            {
                Table table;
                for(isize i = 0; i < block_size; i++)
                {
                    isize op = (isize) op_distribution(gen);
                    isize index = (isize) index_distribution(gen);
                    
                    Slice<const Key> keys = jot::keys(table);
                    Slice<const Val> values = jot::values(table);

                    isize val = val_distribution(gen);
                    
                    switch(op)
                    {
                        case OP_SET1:
                        case OP_SET2:
                        case OP_SET3:
                            *set(&table, Key{cast(i32) i}, Val{cast(i32) i});
                            break;

                        case OP_REMOVE: 
                        {
                            if(keys.size != 0)
                            {
                                //select a random key from the entries and remove it
                                Key key = keys[index % keys.size];
                                bool was_found = remove(&table, key); 

                                //the key can be not found if we allow OP_MARK_REMOVED
                                test(was_found || do_mark_removed);
                            }
                            break;
                        }

                        case OP_MARK_REMOVED: 
                        {
                            if(keys.size != 0 && do_mark_removed)
                            {
                                //select a random key from the entries and remove it
                                Key key = keys[index % keys.size];
                                mark_removed(&table, key); 
                            }
                            break;
                        }

                        case OP_RESERVE_ENTRIES: 
                            *reserve_entries(&table, index % max_size);
                            break;
                        
                        case OP_RESERVE_JUMP_TABLE: 
                            *reserve_jump_table(&table, index % max_size);
                            break;

                        default: break;
                    }
                    
                    keys = jot::keys(table);
                    values = jot::values(table);

                    //test integrity at random
                    if(keys.size != 0 && do_mark_removed == false)
                    {
                        isize at = index % keys.size; 
                        Key key = keys[at];
                        
                        Hash_Found found = find(table, key);
                        test(found.entry_index != -1 && "key must be present");
                        
                        Val val = values[found.entry_index];
                        test(val.val == key.val && "key values must form a pair");
                    }

                    test(is_invariant(table));
                }
            }

            i64 after = trackers_alive();
            test(before == after);
        };
        
        Seed seed = 3566690314;
        gen = std::mt19937(seed);
        for(int i = 0; i < 100; i++)
        {
            //we test 10 batch size 2x more often
            test_batch(10, false);
            test_batch(10, false);
            test_batch(40, false);
            test_batch(160, false);
            test_batch(640, false);
        
            test_batch(10, true);
            test_batch(10, true);
            test_batch(160, true);
            test_batch(640, true);
        }
    }

    void test_hash_table()
    {
        isize memory_before = memory_globals::default_allocator()->bytes_allocated();

        {
            test_many_add();

            using Trc = Tracker<i32>;
            test_table_add_find<Hash_Table<hash_t, i32, Test_Int_Hash_Functions<hash_t>>>();
            test_table_add_find<Hash_Table<u32, u32>>();
            test_table_add_find<Hash_Table<u32, Trc>>();
            test_table_add_find<Hash_Table<Trc, u32, Test_Tracker_Hash_Functions>>();
            test_table_add_find<Hash_Table<Trc, Trc, Test_Tracker_Hash_Functions>>();

            const auto str = [](cstring cstr){
                String_Builder builder;
                *copy(&builder, String(cstr));
                return builder;
            };

            Array<String_Builder, 10> builders = {{str("1"), str("2"), str("3"), str("4"), str("5"), str("6"), str("7"), str("8"), str("9"), str("10")}};
            Array<String, 10> strings = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};

            test_table_add_find_any<Hash_Table<String, String_Builder>>(dup(strings), dup(builders)); 
            test_table_add_find_any<Hash_Table<String_Builder, String>>(dup(builders), dup(strings)); 
            test_table_add_find_any<Hash_Table<String_Builder, String_Builder>>(dup(builders), dup(builders)); 

            test_table_mark_remove<Hash_Table<hash_t, i32, Test_Int_Hash_Functions<hash_t>>>();
            test_table_mark_remove<Hash_Table<u32, u32>>();
            test_table_mark_remove<Hash_Table<u32, Trc>>();
            test_table_mark_remove<Hash_Table<Trc, u32, Test_Tracker_Hash_Functions>>();
            test_table_mark_remove<Hash_Table<Trc, Trc, Test_Tracker_Hash_Functions>>();

            test_table_remove<Hash_Table<hash_t, i32, Test_Int_Hash_Functions<hash_t>>>();
            test_table_remove<Hash_Table<u32, u32>>();
            test_table_remove<Hash_Table<u32, Trc>>();
            test_table_remove<Hash_Table<Trc, u32, Test_Tracker_Hash_Functions>>();
            test_table_remove<Hash_Table<Trc, Trc, Test_Tracker_Hash_Functions>>();

            test_stress();
        }


        isize memory_after = memory_globals::default_allocator()->bytes_allocated();
        test(memory_before == memory_after);
    }
}

#include "undefs.h"