#pragma once

#include <random>

#include "_test.h"
#include "hash_table.h"
#include "string.h"
#include "format.h"
#include "defines.h"

namespace jot
{
    template <typename T> nodisc
    hash_t tracker_hash(tests::Tracker<T> const& tracker) noexcept
    {
        return default_key_hash(tracker.val);
    }
    
    template <typename A, typename B> nodisc
    tests::Tracker<A> tracker_key_cast(tests::Tracker<B> const& tracker)
    {
        return tests::Tracker<A>{cast(A) tracker.val};
    }
}

namespace jot::tests::hash_table
{
    template <typename Table> 
    bool value_matches_at(Table const& table, typename Table::Stored_Key const& key, typename Table::Value const& value)
    {
        using Val = typename Table::Value;
        isize found = find_entry(table, move(&key));
        if(found == -1)
            return false;

        auto const& manual = values(table)[found];
        auto const& obtained = get(table, key, value);
        
        bool do_manual_match = Table::key_equal(manual, obtained);
        bool do_expected_match = Table::key_equal(manual, value);
        test(do_manual_match);
        return do_expected_match;
    }

    template <typename Table> 
    bool empty_at(Table const& table, typename Table::Stored_Key const& key)
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

            set(&table, 1, 10);
            test(empty_at(table, 1) == false);
            test(value_matches_at(table, 1, 10));
            test(value_matches_at(table, 1, 100) == false);
            
            test(empty_at(table, 101));
            test(empty_at(table, 2));

            set(&table, 3, 30);
            set(&table, 2, 20);

            test(value_matches_at(table, 1, 10));
            test(empty_at(table, 442120));
            test(value_matches_at(table, 2, 20));
            test(empty_at(table, 654351));
            test(value_matches_at(table, 3, 30));
            test(empty_at(table, 5));

            set(&table, 15, 15);
            set(&table, 31, 15);
        
            set(&table, 0, 100);
            test(value_matches_at(table, 0, 100));
            set(&table, 0, 1000);
            test(value_matches_at(table, 0, 1000));
            test(value_matches_at(table, 0, 100) == false);
            test(empty_at(table, 5));
        }
            
        isize alive_after = trackers_alive();
        test(alive_before == alive_after);
    }
        
    template <typename Table> 
    void test_table_add_find_any(Array<typename Table::Stored_Key, 10> keys, Array<typename Table::Value, 10> values)
    {
        isize alive_before = trackers_alive();
        {
            Table table;

            test(empty_at(table, keys[0]));
            test(empty_at(table, keys[3]));
            test(empty_at(table, keys[5]));

            set(&table, (keys[0]), (values[0]));
            test(empty_at(table, keys[0]) == false);
            test(value_matches_at(table, keys[0], values[0]));
            test(value_matches_at(table, keys[0], values[1]) == false);
            
            test(empty_at(table, keys[5]));
            test(empty_at(table, keys[1]));

            set(&table, (keys[2]), (values[2]));
            set(&table, (keys[1]), (values[1]));

            test(value_matches_at(table, keys[0], values[0]));
            test(empty_at(table, keys[8]));
            test(value_matches_at(table, keys[1], values[1]));
            test(empty_at(table, keys[9]));
            test(value_matches_at(table, keys[2], values[2]));
            test(empty_at(table, keys[4]));

            set(&table, (keys[5]), (values[5]));
            set(&table, (keys[7]), (values[7]));
        
            set(&table, (keys[0]), (values[8]));
            test(value_matches_at(table, keys[0], values[8]));
            set(&table, (keys[0]), (values[9]));
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

            set(&table, 1, 10);
            set(&table, 2, 10);
            set(&table, 3, 10);
            set(&table, 4, 10);

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

            set(&table, 2, 20);
            test(value_matches_at(table, 2, 20));
            
            set(&table, 6, 60);
            set(&table, 7, 70);
            set(&table, 8, 80);
            set(&table, 9, 90);
            set(&table, 10, 100);
            
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

            set(&table, 10, 100);
            test(value_matches_at(table, 10, 100));
            
            mark_removed(&table, 1);
            test(empty_at(table, 1));
            test(value_matches_at(table, 4, 10));
        }
            
        isize alive_after = trackers_alive();
        test(alive_before == alive_after);
    }
    
    template <typename Table> 
    String_Builder format_linker(Table const& table)
    {
        using Link = typename Table::Link;
        Slice<Link> linker = {table._linker, table._linker_size};

        String_Builder builder;
        reserve(&builder, linker.size * 6);
        format_into(&builder, '[');

        isize gravestone_count = 0;
        isize alive_count = 0;
        for(isize i = 0; i < linker.size; i++)
        {
            if(i != 0)
                format_into(&builder, ", ");

            if(linker[i] == cast(Link) hash_table_internal::EMPTY_LINK)
                format_into(&builder, '-');
            else if(linker[i] == cast(Link) hash_table_internal::GRAVESTONE_LINK)
            {
                gravestone_count ++;
                format_into(&builder, 'R');
            }
            else
            {
                alive_count ++;
                format_into(&builder, cast(i64) linker[i]);
            }
        }
        format_into(&builder, "] #A: {} #R: {}", alive_count, gravestone_count);

        return builder;
    }
    
    template <typename Table> 
    void print_table(Table const& table)
    {
        println("\nkeys:   {} #{}", keys(table), keys(table).size);
        println("values: {} #{}", values(table), values(table).size);
        println("linker: {}", format_linker(table));
    }

    template <typename Table> 
    void test_table_remove()
    {
        isize alive_before = trackers_alive();
        {
            Table table;
               
            set(&table, 1, 10);
            set(&table, 2, 20);
            set(&table, 3, 30);
            set(&table, 4, 40);
            
            test(value_matches_at(table, 1, 10));
            test(value_matches_at(table, 2, 20));
            test(value_matches_at(table, 3, 30));
            test(value_matches_at(table, 4, 40));

            auto entry = remove(&table, find(table, 2));
            test(entry.key == 2 && entry.value == 20);
            test(empty_at(table, 2));
            test(value_matches_at(table, 3, 30));
            test(value_matches_at(table, 1, 10));
            
            test(value_matches_at(table, 4, 40));

            entry = remove(&table, find(table, 3));
            test(entry.key == 3 && entry.value == 30);
            test(empty_at(table, 3));
            test(value_matches_at(table, 1, 10));
            test(value_matches_at(table, 4, 40));

            set(&table, 2, 20);
            test(value_matches_at(table, 2, 20));
            
            set(&table, 6, 60);
            set(&table, 7, 70);
            set(&table, 8, 80);
            set(&table, 9, 90);
            rehash(&table, {});

            set(&table, 10, 100);
            
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

            set(&table, 10, 100);
            test(value_matches_at(table, 10, 100));
            
            entry = remove(&table, find(table, 1));
            test(empty_at(table, 1));
            test(value_matches_at(table, 4, 40));
        }
            
        isize alive_after = trackers_alive();
        test(alive_before == alive_after);
    }

    template <typename Key>     
    static uint64_t test_int_hash(Key const& key) 
    {
        return cast(hash_t) key;
    }
    static uint64_t test_tracker_hash(Tracker<i32> const& key) 
    {
        return cast(hash_t) key.val;
    }   
    
    static
    void test_many_add()
    {
        
        using Track = Tracker<i32>;
        using Cast_Track = Tracker<i64>;
        using Key = Track;
        using Val = Track;
        using Table = Hash_Table<Key, Val, test_tracker_hash>;

        i64 before = trackers_alive();
        {
            Table table;
            set(&table, Key{1}, Val{1});
            set(&table, Key{2}, Val{2});
            set(&table, Key{3}, Val{3});
            set(&table, Key{4}, Val{4});
            set(&table, Key{5}, Val{5});
            set(&table, Key{6}, Val{6});
            set(&table, Key{7}, Val{7});
            set(&table, Key{8}, Val{8});
            set(&table, Key{9}, Val{9});
            set(&table, Key{10}, Val{10});
            
            set(&table, Key{11}, Val{11});
            set(&table, Key{12}, Val{12});
            set(&table, Key{13}, Val{13});
            set(&table, Key{14}, Val{14});
            set(&table, Key{15}, Val{15});
            set(&table, Key{16}, Val{16});
            set(&table, Key{17}, Val{17});
            set(&table, Key{18}, Val{18});
            set(&table, Key{19}, Val{19});
            set(&table, Key{20}, Val{20});

            set(&table, Key{21}, Val{21});
            set(&table, Key{22}, Val{22});
            set(&table, Key{23}, Val{23});
            set(&table, Key{24}, Val{24});
            set(&table, Key{25}, Val{25});
            set(&table, Key{26}, Val{26});
            set(&table, Key{27}, Val{27});
            set(&table, Key{28}, Val{28});
            set(&table, Key{29}, Val{29});
            set(&table, Key{30}, Val{30});
        }
        i64 after = trackers_alive();
        test(before == after);
    }

    template<bool do_cast>
    void test_stress()
    {
        using Stored_Key = std::conditional_t<do_cast, Tracker<i64>, Tracker<i32>>;
        using Val = Stored_Key;
        using Key = Tracker<i32>;

        using Table = Hash_Table_Access<Stored_Key, Val, Key, test_tracker_hash>;
        using Count_Table = Hash_Table_Access<Stored_Key, i32, Key, test_tracker_hash>;
        using Seed = std::random_device::result_type;

        static_assert(std::is_same_v<typename Table::Stored_Key, Stored_Key>, "!");
        static_assert(std::is_same_v<typename Table::Key, Key>, "!");
        std::random_device rd;
        
        static constexpr isize OP_SET = 0;
        static constexpr isize OP_REMOVE = 1;
        static constexpr isize OP_MARK_REMOVED = 2;
        static constexpr isize OP_RESERVE_ENTRIES = 3;
        static constexpr isize OP_RESERVE_JUMP_TABLE = 4;
        static constexpr isize OP_REHASH = 5;
        static constexpr isize OP_MULTIADD = 6;

        std::discrete_distribution<unsigned> op_distribution({50, 15, 15, 5, 5, 15, 40});
        std::uniform_int_distribution<unsigned> index_distribution(0);

        isize max_size = 500;
        
        enum Do_Ops : u32
        {   
            DO_BASE = 0,
            DO_REMOVE = 1,
            DO_MARK_REMOVED = 2,
            DO_MULTIADD = 4,
        };
        
        const auto cast_key = [](Stored_Key const& key) -> Key {
            return tracker_key_cast<Key, Stored_Key>(key);
        };

        const auto incr_count_table = [&](Count_Table* count_table, Stored_Key const& key){
            Key access = cast_key(key);
            i32 count = get(*count_table, access, 0);
            set(count_table, key, count + 1);
            
            return count + 1;
        };

        const auto decr_count_table = [&](Count_Table* count_table, Stored_Key const& key){
            Key access = cast_key(key);
            i32 count = get(*count_table, access, 0);
            if(count <= 1)
                remove(count_table, access);
            else
                set(count_table, key, count - 1);

            return max(count, 0);
        };

        std::mt19937 gen;
        const auto test_batch = [&](isize block_size, u32 do_ops){
            i64 before = trackers_alive();

            {
                bool do_remove = do_ops & DO_REMOVE;
                bool do_mark_removed = do_ops & DO_MARK_REMOVED;
                bool do_multiadd = do_ops & DO_MULTIADD;

                Table table;
                Count_Table count_table;
                i32 added_i = 0;
                for(isize i = 0; i < block_size; i++)
                {
                    isize op = (isize) op_distribution(gen);
                    isize index = (isize) index_distribution(gen);
                    
                    Slice<const Stored_Key> keys = jot::keys(table);
                    
                    bool skipped = false;
                    switch(op)
                    {
                        case OP_SET: {
                            Stored_Key key = Stored_Key{added_i};
                            Val val = Val{added_i};

                            set(&table, key, val);
                            incr_count_table(&count_table, key);

                            added_i++;
                            break;
                        }

                        case OP_MULTIADD: 
                            if(keys.size == 0 || do_multiadd == false)
                                skipped = true;
                            else
                            {
                                Stored_Key key = Stored_Key{added_i - 1};

                                incr_count_table(&count_table, key);
                                multi::add_another(&table, key, key);
                            }
                                
                            break;

                        case OP_REMOVE: 
                            if(keys.size == 0 || do_remove == false)
                                skipped = true;
                            else
                            {
                                //select a random key from the entries and remove it
                                Stored_Key key = keys[index % keys.size];
                                Key acess = cast_key(key);
                                bool was_found = remove(&table, acess); 
                                if(was_found)
                                    decr_count_table(&count_table, key);

                                //the key can be not found if we allow OP_MARK_REMOVED
                                test(was_found || do_mark_removed);
                            }
                            break;

                        case OP_MARK_REMOVED: 
                            if(keys.size == 0 || do_mark_removed == false)
                                skipped = true;
                            else
                            {
                                //select a random key from the entries and remove it
                                Stored_Key key = keys[index % keys.size];
                                isize entry_i = mark_removed(&table, cast_key(key)); 
                                if(entry_i != -1)
                                    decr_count_table(&count_table, key);
                            }
                            break;

                        case OP_RESERVE_ENTRIES: 
                            reserve_entries(&table, index % max_size);
                            break;
                        
                        case OP_RESERVE_JUMP_TABLE: 
                            reserve_jump_table(&table, index % max_size);
                            break;
                            
                        case OP_REHASH: 
                            rehash(&table);
                            break;

                        default: break;
                    }
                    
                    //if nothing happened try again
                    if(skipped == true)
                    {
                        i--;
                        continue;
                    }
                    
                    //test integrity of all key value pairs
                    // - we cant perform this check if we used mark_removed
                    //   because then the entries might contain entries which are
                    //   deleted and thus cannot be found via the find function
                    if(do_mark_removed == false)
                    {
                        Slice<const Stored_Key> count_table_keys = jot::keys(count_table);
                        Slice<const Val> table_values = jot::values(table);
                        Slice<const Stored_Key> table_keys = jot::keys(table);

                        //all count_table keys keys need to be correct
                        for(isize k = 0; k < count_table_keys.size; k++)
                        {
                            Stored_Key key = count_table_keys[k];
                            Key acess = cast_key(key);
                            i32 generation_size = get(count_table, acess, -1);
                            test(generation_size > 0 && "count must be present");

                            Hash_Found found = find(table, acess);
                            i32 found_generation_size = 0;
                            
                            while(found.entry_index != -1)
                            {
                                Val value = table_values[found.entry_index];
                                test(value.val == key.val && "key values must form a pair");

                                found = multi::find_next(table, acess, found);
                                found_generation_size ++;
                            }

                            test(found_generation_size == generation_size && "there must be exactly generation_size entries and no more");
                        }

                        for(isize k = 0; k < table_keys.size; k++)
                        {
                            Stored_Key key = table_keys[k];
                            Key acess = cast_key(key);
                            bool is_found = has(table, acess);
                            test(is_found && "all table keys need to be in count_table (otherwise the above test wouldnt be exhaustive)");
                        }
                    }

                    test(is_invariant(table));
                }
            }

            i64 after = trackers_alive();
            test(before == after);
        };
        
        Seed seed = rd();
        gen = std::mt19937(seed);
        for(int i = 0; i < 15; i++)
        {
            //we test 10 batch size 2x more often becasue 
            // such small size makes edge cases more likely
            test_batch(10,  DO_REMOVE);
            test_batch(10,  DO_REMOVE);
            test_batch(40,  DO_REMOVE);
            test_batch(160, DO_REMOVE);
            test_batch(640, DO_REMOVE);

            test_batch(10,  DO_REMOVE | DO_MARK_REMOVED);
            test_batch(10,  DO_REMOVE | DO_MARK_REMOVED);
            test_batch(40,  DO_REMOVE | DO_MARK_REMOVED);
            test_batch(160, DO_REMOVE | DO_MARK_REMOVED);
            test_batch(640, DO_REMOVE | DO_MARK_REMOVED);
            
            test_batch(640, DO_MULTIADD | DO_MARK_REMOVED);
            test_batch(640, DO_MULTIADD | DO_REMOVE);
            test_batch(640, DO_MULTIADD | DO_REMOVE | DO_MARK_REMOVED);
        }
    }
    
    static
    void test_hash_table(u32 flags)
    {
        bool print = !(flags & Test_Flags::SILENT);
        isize memory_before = memory_globals::default_allocator()->bytes_allocated();

        {
            if(print) println("\ntest_hash_table()");

            using Trc = Tracker<i32>;
            test_table_add_find<Hash_Table<hash_t, i32, test_int_hash<hash_t>>>();
            test_table_add_find<Hash_Table<u32, u32>>();
            test_table_add_find<Hash_Table<u32, Trc>>();
            test_table_add_find<Hash_Table<Trc, u32, test_tracker_hash>>();
            test_table_add_find<Hash_Table<Trc, Trc, test_tracker_hash>>();
            
            if(print) println("  test_table_add_find() type: Hash_Table<hash_t, i32, test_int_hash<hash_t>>");
            if(print) println("  test_table_add_find() type: Hash_Table<u32, u32>");
            if(print) println("  test_table_add_find() type: Hash_Table<u32, Trc>");
            if(print) println("  test_table_add_find() type: Hash_Table<Trc, u32, test_tracker_hash>");
            if(print) println("  test_table_add_find() type: Hash_Table<Trc, Trc, test_tracker_hash>");
            
            if(print) println("  test_many_add()");
            test_many_add();

            const auto own = [](const char* cstr){
                String_Builder builder;
                copy(&builder, String(cstr));
                return builder;
            };

            Array<String_Builder, 10> builders = {{own("1"), own("2"), own("3"), own("4"), own("5"), own("6"), own("7"), own("8"), own("9"), own("10")}};
            Array<String, 10> strings = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};

            test_table_add_find_any<Hash_Table<String, String_Builder>>(strings, builders); 
            test_table_add_find_any<Hash_Table<String_Builder, String>>(builders, strings); 
            test_table_add_find_any<Hash_Table<String_Builder, String_Builder>>(builders, builders); 
            
            if(print) println("  test_table_add_find_any() type: Hash_Table<String, String_Builder>");
            if(print) println("  test_table_add_find_any() type: Hash_Table<String_Builder, String>");
            if(print) println("  test_table_add_find_any() type: Hash_Table<String_Builder, String_Builder>");

            test_table_mark_remove<Hash_Table<hash_t, i32, test_int_hash<hash_t>>>();
            test_table_mark_remove<Hash_Table<u32, u32>>();
            test_table_mark_remove<Hash_Table<u32, Trc>>();
            test_table_mark_remove<Hash_Table<Trc, u32, test_tracker_hash>>();
            test_table_mark_remove<Hash_Table<Trc, Trc, test_tracker_hash>>();
            
            if(print) println("  test_table_mark_remove() type: Hash_Table<hash_t, i32, test_int_hash<hash_t>>");
            if(print) println("  test_table_mark_remove() type: Hash_Table<u32, u32>");
            if(print) println("  test_table_mark_remove() type: Hash_Table<u32, Trc>");
            if(print) println("  test_table_mark_remove() type: Hash_Table<Trc, u32, test_tracker_hash>");
            if(print) println("  test_table_mark_remove() type: Hash_Table<Trc, Trc, test_tracker_hash>");

            test_table_remove<Hash_Table<hash_t, i32, test_int_hash<hash_t>>>();
            test_table_remove<Hash_Table<u32, u32>>();
            test_table_remove<Hash_Table<u32, Trc>>();
            test_table_remove<Hash_Table<Trc, u32, test_tracker_hash>>();
            test_table_remove<Hash_Table<Trc, Trc, test_tracker_hash>>();
            
            if(print) println("  test_table_remove() type: Hash_Table<hash_t, i32, test_int_hash<hash_t>>");
            if(print) println("  test_table_remove() type: Hash_Table<u32, u32>");
            if(print) println("  test_table_remove() type: Hash_Table<u32, Trc>");
            if(print) println("  test_table_remove() type: Hash_Table<Trc, u32, test_tracker_hash>");
            if(print) println("  test_table_remove() type: Hash_Table<Trc, Trc, test_tracker_hash>");
            
            if(flags & Test_Flags::STRESS)
            {
                test_stress<false>();
                test_stress<true>();
            }
        }

        
        isize memory_after = memory_globals::default_allocator()->bytes_allocated();
        test(memory_before == memory_after);
    }
}

#include "undefs.h"