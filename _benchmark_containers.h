#pragma once

#define TEST_STD true
#define GIVEN_TIME 500
#define DEF_BUCKET_GROWTH Bucket_Array_Growth{256, 3, 2}

#include <random>

//#define BUCKET_ARRAY_NO_FFS
#if TEST_STD
    #include <vector>
    #include <unordered_map>
#endif

#include "_test.h"
#include "string_hash.h"
#include "simple_bucket_array.h"
#include "format.h"
#include "benchmark.h"

namespace jot
{
    template <> struct Formattable<Bench_Result>
    {
        static
        void format(String_Builder* appender, Bench_Result result) noexcept
        {
            format_into(appender, "{ ", result.mean_ms, "ms [", result.deviation_ms, "] (", result.iters, ") }");
        }
    };
}

namespace jot::benchmarks
{
    using Random_Generator = std::mt19937;
    using Random_Device = std::random_device;

    template<typename T>
    void shuffle(Slice<T> items, Random_Generator* random) 
    {    
        ::std::uniform_int_distribution<unsigned long long> uniform_dist(0);
        
        for (isize i = 0; i < items.size - 1; i++)
        {
            isize j = uniform_dist(*random) % (items.size - i);
            swap(&items[i], &items[i + j]);
        }
    }

    static void benchmark_cotainer_add()
    {
        const auto bench_adds = [&](u64 batch_size)
        {
            println("\nADD ", batch_size);
                    
            Bench_Result res_add_array = benchmark(GIVEN_TIME, [&]{
                Array<u64> array;
                for(u64 i = 0; i < batch_size; i++)
                {
                    push(&array, i);
                    do_no_optimize(array);
                    read_write_barrier();
                }
                return true;
            }, batch_size);
            
            Bench_Result res_add_hash_table = benchmark(GIVEN_TIME, [&]{
                Hash_Table<u32, u64, int_hash<u32>> hash_table;
                for(u64 i = 0; i < batch_size; i++)
                {
                    set(&hash_table, (u32) i, i);
                    do_no_optimize(hash_table);
                    read_write_barrier();
                }
                return true;
            }, batch_size);
            
            Bench_Result res_add_bucket_array = benchmark(GIVEN_TIME, [&]{
                Bucket_Array<u64> bucket_array;
                for(u64 i = 0; i < batch_size; i++)
                {
                    (void) insert_bucket_index(&bucket_array, i, DEF_BUCKET_GROWTH);
                    do_no_optimize(bucket_array);
                    read_write_barrier();
                }
                return true;
            }, batch_size);

            Bench_Result res_add_vector;
            Bench_Result res_add_unordered_map;

            if(TEST_STD)
            {
                res_add_vector = benchmark(GIVEN_TIME, [&]{
                    std::vector<u64> vec;
                    for(u64 i = 0; i < batch_size; i++)
                    {
                        vec.push_back(i);
                        do_no_optimize(vec);
                        read_write_barrier();
                    }
                    return true;
                }, batch_size);
            
                res_add_unordered_map = benchmark(GIVEN_TIME, [&]{
                    std::unordered_map<u32, u64> map;
                    for(u64 i = 0; i < batch_size; i++)
                    {
                        map.insert_or_assign((u32) i, i);
                        do_no_optimize(map);
                        read_write_barrier();
                    }
                    return true;
                }, batch_size);
            }

            if(TEST_STD)
            {
                println("vector:       ", res_add_vector);
                println("unordered_map:", res_add_unordered_map);
            }

            println("stack:        ", res_add_array);
            println("hash_table:   ", res_add_hash_table);
            println("bucket_array: ", res_add_bucket_array);
        };
        
        bench_adds(1000);
        println("=== ignore above ===\n");
        bench_adds(10);
        bench_adds(100);
        bench_adds(1000);
        bench_adds(10000);
        bench_adds(100000);
    }
    
    static void benchmark_cotainer_remove()
    {
        const auto bench_remove = [&](isize batch_size)
        {
            println("\nREMOVE ", batch_size);
        
            Array<u64> array;
            Hash_Table<u32, u64, int_hash<u32>> hash_table;
            Bucket_Array<u64> bucket_array;
            std::vector<u64> vec;
            std::unordered_map<u32, u64> map;

            isize removed_i = 0;
            Random_Device random_device;
            Random_Generator gen(random_device());

            Array<u32> added_keys;
            resize(&added_keys, batch_size);

            Bench_Result res_remove_array = benchmark(GIVEN_TIME, [&]{
                if(size(array) == 0)
                {
                    for(u64 i = 0; i < batch_size; i++)
                    {
                        push(&array, i);
                        added_keys[i] = i;
                    }
                
                    shuffle(slice(&added_keys), &gen);
                    return false;
                }

                pop(&array);
                do_no_optimize(array);
                read_write_barrier();
                return true;
            });
            
            removed_i = 0;
            Bench_Result res_remove_hash_table = benchmark(GIVEN_TIME, [&]{
                if(removed_i == 0)
                {
                    for(u64 i = 0; i < batch_size; i++)
                    {
                        set(&hash_table, (u32) i, i);
                        added_keys[i] = i;
                    }
                
                    shuffle(slice(&added_keys), &gen);
                
                    removed_i = batch_size;
                    return false;
                }

                u32 removed = added_keys[--removed_i];
                remove(&hash_table, removed);
                do_no_optimize(hash_table);
                read_write_barrier();
                return true;
            });
            
            removed_i = 0;
            Bench_Result res_remove_mark_hash_table = benchmark(GIVEN_TIME, [&]{
                if(removed_i == 0)
                {
                    for(u64 i = 0; i < batch_size; i++)
                    {
                        set(&hash_table, (u32) i, i);
                        added_keys[i] = i;
                    }
                
                    shuffle(slice(&added_keys), &gen);
                
                    removed_i = batch_size;
                    return false;
                }
            
                u32 removed = added_keys[--removed_i];
                mark_removed(&hash_table, removed);
                do_no_optimize(hash_table);
                read_write_barrier();
                return true;
            });
            
            removed_i = 0;
            Bench_Result res_remove_bucket_array = benchmark(GIVEN_TIME, [&]{
                if(removed_i == 0)
                {
                    for(u64 i = 0; i < batch_size; i++)
                    {
                        u32 added = (u32) insert(&bucket_array, i, DEF_BUCKET_GROWTH);
                        added_keys[i] = added;
                    }
                
                    shuffle(slice(&added_keys), &gen);
                
                    removed_i = batch_size;
                    return false;
                }
            
                u32 removed = added_keys[--removed_i];
                remove(&bucket_array, removed);
                do_no_optimize(bucket_array);
                read_write_barrier();
                return true;
            });

            Bench_Result res_remove_vector;
            Bench_Result res_remove_unordered_map;

            if(TEST_STD)
            {
                res_remove_vector = benchmark(GIVEN_TIME, [&]{
                    if(vec.size() == 0)
                    {
                        for(u64 i = 0; i < batch_size; i++)
                        {
                            vec.push_back(i);
                            added_keys[i] = i;
                        }
                
                        shuffle(slice(&added_keys), &gen);
                        return false;
                    }

                    vec.pop_back();
                    do_no_optimize(vec);
                    read_write_barrier();
                    return true;
                });
            
                removed_i = 0;
                res_remove_unordered_map = benchmark(GIVEN_TIME, [&]{
                    if(removed_i == 0)
                    {
                        for(u64 i = 0; i < batch_size; i++)
                        {
                            map.insert_or_assign((u32) i, i);
                            added_keys[i] = i;
                        }
                
                        shuffle(slice(&added_keys), &gen);
                
                        removed_i = batch_size;
                        return false;
                    }
                
                    u32 removed = added_keys[--removed_i];
                    map.extract(removed);
                    do_no_optimize(map);
                    read_write_barrier();
                    return true;
                });
            }

            if(TEST_STD)
            {
            println("vector:            ", res_remove_vector);
            println("unordered_map:     ", res_remove_unordered_map);
            }

            println("stack:             ", res_remove_array);
            println("hash_table:        ", res_remove_hash_table);
            println("hash_table mark:   ", res_remove_mark_hash_table);
            println("bucket_array:      ", res_remove_bucket_array);
        };
        
        bench_remove(1000);
        println("=== ignore above ===\n");
        bench_remove(10);
        bench_remove(100);
        bench_remove(1000);
        bench_remove(10000);
        bench_remove(100000);
    }

    static void benchmark_cotainer_push_push_pop()
    {
        const auto bench_remove = [&](isize batch_size)
        {
            println("\nPUSH PUSH POP ", batch_size);

            isize counter = 0;
            Bench_Result res_remove_array = benchmark(GIVEN_TIME, [&]{
                
                Array<u64> array;
                for(isize i = 0; i < batch_size; i++)
                {
                    push(&array, size(array));
                    push(&array, size(array));
                    pop(&array);
                }

                do_no_optimize(array);
                read_write_barrier();
                return true;
            }, batch_size);

            Bench_Result res_remove_hash_table = benchmark(GIVEN_TIME, [&]{
                
                Hash_Table<u32, u64, int_hash<u32>> hash_table;
                for(isize i = 0; i < batch_size; i++)
                {
                    set(&hash_table, counter, counter);
                    set(&hash_table, counter + 1, counter + 1);
                    remove(&hash_table, counter);
                    counter += 2;
                }

                do_no_optimize(hash_table);
                read_write_barrier();
                return true;
            }, batch_size);
            
            Bench_Result res_remove_mark_hash_table = benchmark(GIVEN_TIME, [&]{
                
                Hash_Table<u32, u64, int_hash<u32>> hash_table;
                for(isize i = 0; i < batch_size; i++)
                {
                    set(&hash_table, counter, counter);
                    set(&hash_table, counter + 1, counter + 1);
                    mark_removed(&hash_table, counter);
                    counter += 2;
                }

                do_no_optimize(hash_table);
                read_write_barrier();
                return true;
            }, batch_size);
            
            Bench_Result res_remove_bucket_array = benchmark(GIVEN_TIME, [&]{
            
                Bucket_Array<u64> bucket_array;
                for(isize i = 0; i < batch_size; i++)
                {
                    u32 added = (u32) insert(&bucket_array, (u64) counter++, DEF_BUCKET_GROWTH);
                    (void) insert(&bucket_array, (u64) counter++, DEF_BUCKET_GROWTH);
                    remove(&bucket_array, added);
                }
                do_no_optimize(bucket_array);
                read_write_barrier();
                return true;
            }, batch_size);

            Bench_Result res_remove_vector;
            Bench_Result res_remove_unordered_map;

            if(TEST_STD)
            {
                res_remove_vector = benchmark(GIVEN_TIME, [&]{
                    std::vector<u64> vec;
                    for(isize i = 0; i < batch_size; i++)
                    {
                        vec.push_back(vec.size());
                        vec.push_back(vec.size());
                        vec.pop_back();
                    }
                    do_no_optimize(vec);
                    read_write_barrier();
                    return true;
                }, batch_size);
            
                res_remove_unordered_map = benchmark(GIVEN_TIME, [&]{
                    std::unordered_map<u32, u64> map;
                    for(isize i = 0; i < batch_size; i++)
                    {
                        map.insert_or_assign((u32) counter, counter);
                        map.insert_or_assign((u32) counter + 1, counter + 1);
                        map.extract(counter);
                        counter += 2;
                    }
                    do_no_optimize(map);
                    read_write_barrier();
                    return true;
                }, batch_size);
            }

            if(TEST_STD)
            {
            println("vector:            ", res_remove_vector);
            println("unordered_map:     ", res_remove_unordered_map);
            }

            println("stack:             ", res_remove_array);
            println("hash_table:        ", res_remove_hash_table);
            println("hash_table mark:   ", res_remove_mark_hash_table);
            println("bucket_array:      ", res_remove_bucket_array);
        };
        
        bench_remove(1000);
        println("=== ignore above ===\n");
        bench_remove(10);
        bench_remove(100);
        bench_remove(1000);
        bench_remove(10000);
        bench_remove(100000);
    }

    #if 0
    static void benchmark_cotainer_insert_remove_sections()
    {
        const auto bench_remove = [&](isize batch_size)
        {
            println("\nPUSH PUSH POP ", batch_size);

            isize counter = 0;
            isize section_size = 4;
            Bench_Result res_remove_array = benchmark(GIVEN_TIME, [&]{
                
                Array<u64> array;
                for(isize i = 0; i < batch_size; i++)
                {
                    push(&array, size(array));
                    push(&array, size(array));
                    pop(&array);
                }

                do_no_optimize(array);
                read_write_barrier();
                iter ++;
                return true;
            }, batch_size);

            Bench_Result res_remove_hash_table = benchmark(GIVEN_TIME, [&]{
                
                Hash_Table<u32, u64, int_hash<u32>> hash_table;
                for(isize i = 0; i < batch_size; i++)
                {
                    set(&hash_table, counter, counter);
                    set(&hash_table, counter + 1, counter + 1);
                    remove(&hash_table, counter);
                    counter += 2;
                }

                do_no_optimize(hash_table);
                read_write_barrier();
                return true;
            }, batch_size);
            
            Bench_Result res_remove_mark_hash_table = benchmark(GIVEN_TIME, [&]{
                
                Hash_Table<u32, u64, int_hash<u32>> hash_table;
                for(isize i = 0; i < batch_size; i++)
                {
                    set(&hash_table, counter, counter);
                    set(&hash_table, counter + 1, counter + 1);
                    mark_removed(&hash_table, counter);
                    counter += 2;
                }

                do_no_optimize(hash_table);
                read_write_barrier();
                return true;
            }, batch_size);
            
            Bench_Result res_remove_bucket_array = benchmark(GIVEN_TIME, [&]{
            
                Bucket_Array<u64> bucket_array;
                for(isize i = 0; i < batch_size; i++)
                {
                    u32 added = (u32) insert(&bucket_array, (u64) counter++, DEF_BUCKET_GROWTH);
                    (void) insert(&bucket_array, (u64) counter++, DEF_BUCKET_GROWTH);
                    remove(&bucket_array, added);
                }
                do_no_optimize(bucket_array);
                read_write_barrier();
                return true;
            }, batch_size);

            Bench_Result res_remove_vector;
            Bench_Result res_remove_unordered_map;

            if(TEST_STD)
            {
                res_remove_vector = benchmark(GIVEN_TIME, [&]{
                    std::vector<u64> vec;
                    for(isize i = 0; i < batch_size; i++)
                    {
                        vec.push_back(vec.size());
                        vec.push_back(vec.size());
                        vec.pop_back();
                    }
                    do_no_optimize(vec);
                    read_write_barrier();
                    return true;
                }, batch_size);
            
                res_remove_unordered_map = benchmark(GIVEN_TIME, [&]{
                    std::unordered_map<u32, u64> map;
                    for(isize i = 0; i < batch_size; i++)
                    {
                        map.insert_or_assign((u32) counter, counter);
                        map.insert_or_assign((u32) counter + 1, counter + 1);
                        map.extract(counter);
                        counter += 2;
                    }
                    do_no_optimize(map);
                    read_write_barrier();
                    return true;
                }, batch_size);
            }

            if(TEST_STD)
            {
            println("vector:            ", res_remove_vector);
            println("unordered_map:     ", res_remove_unordered_map);
            }

            println("stack:             ", res_remove_array);
            println("hash_table:        ", res_remove_hash_table);
            println("hash_table mark:   ", res_remove_mark_hash_table);
            println("bucket_array:      ", res_remove_bucket_array);
        };
        
        bench_remove(1000);
        println("=== ignore above ===\n");
        bench_remove(10);
        bench_remove(100);
        bench_remove(1000);
        bench_remove(10000);
        bench_remove(100000);
    }
    #endif
}

