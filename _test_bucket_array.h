#pragma once

#include <random>

//#define BUCKET_ARRAY_NO_FFS
#include <vector>
#include <unordered_map>

#include "_test.h"
#include "string_hash.h"
#include "bucket_array.h"
#include "format.h"
#include "time.h"
#include "benchmark.h"
#include "defines.h"

namespace jot
{
    template <> struct Formattable<Bench_Result>
    {
        nodisc static
        void format(String_Builder* appender, Bench_Result result) noexcept
        {
            format_into(appender, "{ ", result.mean_ms, "ms [", result.deviation_ms, "] (", result.iters, ") }");
        }
    };
}

namespace jot::tests::bucket_array
{
    template <typename T>
    void test_insert_remove(Static_Array<T, 10> const& values)
    {
        isize mem_before = default_allocator()->get_stats().bytes_allocated;
        isize alive_before = trackers_alive();
        {
            Bucket_Array<T> arr(2);

            test(size(arr) == 0);
            test(capacity(arr) == 0);

            isize i0 = insert(&arr, dup(values[0]));
            isize i1 = insert(&arr, dup(values[1]));
            isize i2 = insert(&arr, dup(values[2]));
            
            test(size(arr) == 3);
            test(capacity(arr) >= size(arr));

            test(get(arr, i0) == values[0]);
            test(get(arr, i1) == values[1]);
            test(get(arr, i2) == values[2]);

            T v1 = remove(&arr, i1);
            test(v1 == values[1]);
            test(size(arr) == 2);

            isize i3 = insert(&arr, dup(values[3]));
            isize i4 = insert(&arr, dup(values[4]));

            test(get(arr, i0) == values[0]);
            test(get(arr, i2) == values[2]);
            test(get(arr, i3) == values[3]);
            test(get(arr, i4) == values[4]);
            test(size(arr) == 4);

            T v2 = remove(&arr, i2);
            test(v2 == values[2]);
            test(size(arr) == 3);

            isize i5 = insert(&arr, dup(values[5]));
            isize cap = capacity(arr);
            test(get(arr, i0) == values[0]);
            test(get(arr, i3) == values[3]);
            test(get(arr, i4) == values[4]);
            test(get(arr, i5) == values[5]);
            test(size(arr) == 4);
            test(cap >= size(arr));
            
            isize i6 = insert(&arr, dup(values[6]));
            isize i7 = insert(&arr, dup(values[7]));
            isize i8 = insert(&arr, dup(values[8]));
            isize i9 = insert(&arr, dup(values[9]));
            isize i10 = insert(&arr, dup(values[9]));
            isize i11 = insert(&arr, dup(values[9]));
            isize i12 = insert(&arr, dup(values[9]));
            isize i13 = insert(&arr, dup(values[9]));
            isize i14 = insert(&arr, dup(values[9]));
            isize i15 = insert(&arr, dup(values[9]));
            test(size(arr) == 14);
            
            test(get(arr, i7) == values[7]);
            test(get(arr, i10) == values[9]);
            test(get(arr, i15) == values[9]);
            
            cap = capacity(arr);

            T v0 = remove(&arr, i0);
            T v3 = remove(&arr, i3);
            T v4 = remove(&arr, i4);
            T v5 = remove(&arr, i5);
            
            test(size(arr) == 10);
            test(capacity(arr) == cap && "capacity must not change when shrinking");

            test(v0 == values[0]);
            test(v1 == values[1]);
            test(v2 == values[2]);
            test(v3 == values[3]);
            test(v4 == values[4]);
            test(v5 == values[5]);
        }
        isize mem_after = default_allocator()->get_stats().bytes_allocated;
        isize alive_after = trackers_alive();

        test(alive_after == alive_before);
        test(mem_after == mem_before);
    }
    
    static
    void stress_test(bool print = true)
    {
        using Seed = std::random_device::result_type;

        if(print) println("test_stress()");

        std::random_device rd;
        
        constexpr isize OP_INSERT = 0;
        constexpr isize OP_REMOVE = 1;

        std::discrete_distribution<unsigned> op_distribution({75, 25});
        std::uniform_int_distribution<unsigned> index_distribution(0);

        std::mt19937 gen;
        const auto test_batch = [&](isize block_size, isize j){
            i64 trackers_before = trackers_alive();
            isize memory_before = default_allocator()->get_stats().bytes_allocated;

            {
                Hash_Table<isize, Bucket_Index, int_hash<isize>> truth;
                Bucket_Array<isize> bucket_array;

                reserve(&truth, block_size);

                for(isize i = 0; i < block_size; i++)
                {
                    isize op = (isize) op_distribution(gen);
                    
                    bool skipped = false;
                    switch(op)
                    {
                        case OP_INSERT: {
                            Bucket_Index index = insert_bucket_index(&bucket_array, i);
                            set(&truth, i, index);
                            break;
                        }

                        case OP_REMOVE: {
                            Slice<const isize> truth_vals = keys(truth);
                            Slice<Bucket_Index> truth_indices = values(&truth);
                            if(truth_indices.size == 0)
                            {   
                                skipped = true;
                                break;
                            }

                            isize selected_index = (isize) index_distribution(gen) % truth_indices.size;
                            Bucket_Index removed_index = truth_indices[selected_index];
                            isize removed_value = truth_vals[selected_index];

                            isize just_removed = remove(&bucket_array, removed_index);
                            test(just_removed == removed_value);
                            remove(&truth, removed_value);
                            break;
                        }

                        default: break;
                    }
                    
                    if(skipped)
                    {
                        i--;
                        continue;
                    }

                    Slice<const isize> truth_vals = keys(truth);
                    Slice<Bucket_Index> truth_indices = values(&truth);
                    isize used_size = size(bucket_array);
                    test(used_size == truth_vals.size);

                    for(isize k = 0; k < truth_indices.size; k++)
                    {
                        Bucket_Index index = truth_indices[k];
                        isize retrieved = get(bucket_array, index);
                        test(retrieved == truth_vals[k]);
                    }
                }
            
                if(print) println("  i: {}\t batch: {}\t final_size: {}", j, block_size, size(bucket_array));
            }

            i64 trackers_after = trackers_alive();
            isize memory_after = default_allocator()->get_stats().bytes_allocated;
            test(trackers_before == trackers_after);
            test(memory_before == memory_after);
        };
        
        Seed seed = rd();
        gen = std::mt19937(seed);
        for(isize i = 0; i < 10; i++)
        {
            test_batch(10, i);
            test_batch(10, i);
            test_batch(40, i);
            test_batch(160, i);
            test_batch(640, i);
            test_batch(640*4, i);
            test_batch(640*16, i);
        }
    }
    
    static
    void test_bucket_array(u32 flags)
    {
        bool print = !(flags & Test_Flags::SILENT);

        Static_Array<i32, 10>          arr1 = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        Static_Array<Test_String, 10>  arr2 = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};
        Static_Array<Tracker<i32>, 10> arr3 = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        
        if(print) println("\ntest_bucket_array()");
        if(print) println("  type: i32");
        test_insert_remove(arr1);

        if(print) println("  type: Test_String");
        test_insert_remove(arr2);
        
        if(print) println("  type: Tracker<i32>");
        test_insert_remove(arr3);

        if(flags & Test_Flags::STRESS)
            stress_test(print);
    }
}

namespace jot::benchmarks
{
    static
    void benchmark_bucket_array_vs_hash_table()
    {
        const auto bench_adds = [&](u64 batch_size)
        {
            constexpr isize GIVEN_TIME = 500;
            constexpr Bucket_Array_Growth growth = {256, 0, 3, 2};
            println("\nADD ", batch_size);
        
            bool test_std = true;

            Bench_Result res_add_array = benchmark(GIVEN_TIME, [&]{
                Array<u64> array;
                for(u64 i = 0; i < batch_size; i++)
                {
                    push(&array, i);
                    do_no_optimize(array);
                    read_write_barrier();
                }
                return true;
            });
            
            Bench_Result res_add_hash_table = benchmark(GIVEN_TIME, [&]{
                Hash_Table<u32, u64, int_hash<u32>> hash_table;
                for(u64 i = 0; i < batch_size; i++)
                {
                    set(&hash_table, cast(u32) i, i);
                    do_no_optimize(hash_table);
                    read_write_barrier();
                }
                return true;
            });
            
            Bench_Result res_add_bucket_array = benchmark(GIVEN_TIME, [&]{
                Bucket_Array<u64> bucket_array(8);
                for(u64 i = 0; i < batch_size; i++)
                {
                    cast(void) insert_bucket_index(&bucket_array, i, growth);
                    do_no_optimize(bucket_array);
                    read_write_barrier();
                }
                return true;
            });

            Bench_Result res_add_vector;
            Bench_Result res_add_unordered_map;

            if(test_std)
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
                });
            
                res_add_unordered_map = benchmark(GIVEN_TIME, [&]{
                    std::unordered_map<u32, u64> map;
                    for(u64 i = 0; i < batch_size; i++)
                    {
                        map.insert_or_assign(cast(u32) i, i);
                        do_no_optimize(map);
                        read_write_barrier();
                    }
                    return true;
                });
            }

            if(test_std)
            {
                println("vector:       ", res_add_vector);
                println("unordered_map:", res_add_unordered_map);
            }

            println("stack:        ", res_add_array);
            println("hash_table:   ", res_add_hash_table);
            println("bucket_array: ", res_add_bucket_array);
        };
        
        const auto bench_remove = [&](u64 batch_size)
        {
            constexpr isize GIVEN_TIME = 500;
            constexpr Bucket_Array_Growth growth = {256, 0, 3, 2};
            println("\nREMOVE ", batch_size);
        
            bool test_std = true;

            Array<u64> array;
            Hash_Table<u32, u64, int_hash<u32>> hash_table;
            Bucket_Array<u64> bucket_array(8);
            std::vector<u64> vec;
            std::unordered_map<u32, u64> map;

            u64 removed_i = 0; 

            Bench_Result res_remove_array = benchmark(GIVEN_TIME, [&]{
                if(size(array) == 0)
                {
                    for(u64 i = 0; i < batch_size; i++)
                        push(&array, i);
                
                    removed_i = size(array);
                    return false;
                }

                pop(&array);
                do_no_optimize(array);
                read_write_barrier();
                return true;
            });
            
            Bench_Result res_remove_hash_table = benchmark(GIVEN_TIME, [&]{
                if(size(hash_table) == 0)
                {
                    for(u64 i = 0; i < batch_size; i++)
                        set(&hash_table, cast(u32) i, i);
                
                    removed_i = size(hash_table);
                    return false;
                }

                remove(&hash_table, cast(u32) --removed_i);
                do_no_optimize(hash_table);
                read_write_barrier();
                return true;
            });
            
            Bench_Result res_remove_mark_hash_table = benchmark(GIVEN_TIME, [&]{
                if(size(hash_table) == 0)
                {
                    for(u64 i = 0; i < batch_size; i++)
                        set(&hash_table, cast(u32) i, i);
                
                    removed_i = size(hash_table);
                    return false;
                }

                mark_removed(&hash_table, cast(u32) --removed_i);
                do_no_optimize(hash_table);
                read_write_barrier();
                return true;
            });
            
            Bench_Result res_remove_bucket_array = benchmark(GIVEN_TIME, [&]{
                if(size(bucket_array) == 0)
                {
                    for(u64 i = 0; i < batch_size; i++)
                        cast(void) insert_bucket_index(&bucket_array, i, growth);
                
                    removed_i = size(bucket_array);
                    return false;
                }

                remove(&bucket_array, --removed_i);
                do_no_optimize(bucket_array);
                read_write_barrier();
                return true;
            });

            Bench_Result res_remove_vector;
            Bench_Result res_remove_unordered_map;

            if(test_std)
            {
                res_remove_vector = benchmark(GIVEN_TIME, [&]{
                    if(vec.size() == 0)
                    {
                        for(u64 i = 0; i < batch_size; i++)
                            vec.push_back(i);
                
                        removed_i = vec.size();
                        return false;
                    }

                    vec.pop_back();
                    do_no_optimize(bucket_array);
                    read_write_barrier();
                    return true;
                });
            
                res_remove_unordered_map = benchmark(GIVEN_TIME, [&]{
                    if(map.size() == 0)
                    {
                        for(u64 i = 0; i < batch_size; i++)
                            map.insert_or_assign(cast(u32) i, i);
                
                        removed_i = map.size();
                        return false;
                    }

                    map.extract(cast(u32) --removed_i);
                    do_no_optimize(hash_table);
                    read_write_barrier();
                    return true;
                });
            }

            if(test_std)
            {
            println("vector:            ", res_remove_vector);
            println("unordered_map:     ", res_remove_unordered_map);
            }

            println("stack:             ", res_remove_array);
            println("hash_table:        ", res_remove_hash_table);
            println("hash_table mark:   ", res_remove_mark_hash_table);
            println("bucket_array:      ", res_remove_bucket_array);
        };
        
        bench_adds(1000);
        println("=== ignore above ===\n");
        bench_adds(10);
        bench_adds(100);
        bench_adds(1000);
        bench_adds(10000);
        bench_adds(100000);

        bench_remove(1000);
        println("=== ignore above ===\n");
        bench_remove(10);
        bench_remove(100);
        bench_remove(1000);
        bench_remove(10000);
        bench_remove(100000);

    }
}

#include "undefs.h"