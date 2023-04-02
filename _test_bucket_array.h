#pragma once

#include <random>

//#define BUCKET_ARRAY_NO_FFS
#include <vector>
#include <unordered_map>

#include "_test.h"
#include "hash_table.h"
#include "bucket_array.h"
#include "format.h"
#include "time.h"
#include "defines.h"

namespace jot
{
    template <> struct Formattable<Bench_Result>
    {
        nodisc static
        void format(String_Appender* appender, Bench_Result result) noexcept
        {
            format_into(appender, "{ ", result.mean_ms, "ms [", result.deviation_ms, "] (", result.iters, ") }");
        }
    };
}

namespace jot::tests::bucket_array
{
    template <typename T>
    void test_insert_remove(Array<T, 10> const& values)
    {
        isize mem_before = default_allocator()->bytes_allocated();
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
        isize mem_after = default_allocator()->bytes_allocated();
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

        isize max_size = 10000;

        std::mt19937 gen;
        const auto test_batch = [&](isize block_size, isize i){
            i64 before = trackers_alive();
            isize mem_before = default_allocator()->bytes_allocated();

            {
                Map<isize, Bucket_Index> truth;
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
            
                if(print) println("  i: {}\t batch: {}\t final_size: {}", i, block_size, size(bucket_array));
            }

            i64 after = trackers_alive();
            isize mem_after = default_allocator()->bytes_allocated();
            test(before == after);
            test(mem_after == mem_before);
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

        Array<i32, 10>          arr1 = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        Array<Test_String, 10>  arr2 = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};
        Array<Tracker<i32>, 10> arr3 = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        
        if(print) println("\test_bucket_array()");
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
        const auto run = [&](u64 batch_size)
        {
            constexpr isize GIVEN_TIME = 500;
            constexpr Bucket_Array_Growth growth = {256, 0, 3, 2};
            println("\nADD ", batch_size);
        
            bool test_std = false;

            Bench_Result res_add_stack = benchmark(GIVEN_TIME, [&]{
                Stack<u64> stack;
                for(u64 i = 0; i < batch_size; i++)
                {
                    push(&stack, i);
                    do_no_optimize(stack);
                    read_write_barrier();
                }
            });
            
            Bench_Result res_add_hash_table = benchmark(GIVEN_TIME, [&]{
                Map<u32, u64> hash_table;
                for(u64 i = 0; i < batch_size; i++)
                {
                    set(&hash_table, cast(u32) i, i);
                    do_no_optimize(hash_table);
                    read_write_barrier();
                }
            });
            
            Bench_Result res_add_bucket_array = benchmark(GIVEN_TIME, [&]{
                Bucket_Array<u64> bucket_array(8);
                for(u64 i = 0; i < batch_size; i++)
                {
                    cast(void) insert_bucket_index(&bucket_array, i, growth);
                    do_no_optimize(bucket_array);
                    read_write_barrier();
                }
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
                });
            
                res_add_unordered_map = benchmark(GIVEN_TIME, [&]{
                    std::unordered_map<u32, u64> map;
                    for(u64 i = 0; i < batch_size; i++)
                    {
                        map.insert_or_assign(cast(u32) i, i);
                        do_no_optimize(map);
                        read_write_barrier();
                    }
                });
            }

            if(test_std)
            {
                println("vector:       ", res_add_vector);
                println("unordered_map:", res_add_unordered_map);
            }

            println("stack:        ", res_add_stack);
            println("hash_table:   ", res_add_hash_table);
            println("bucket_array: ", res_add_bucket_array);
        };
        
        run(1000);
        println("=== ignore above ===\n");
        run(10);
        run(100);
        run(1000);
        run(10000);
        run(100000);
        //run(1000000);

    }
}

#include "undefs.h"