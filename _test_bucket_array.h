#pragma once

#include <random>
#include "hash_table.h"
#include "string_hash.h"
#include "_test.h"
#include "bucket_array.h"

namespace jot
{
namespace tests
{
namespace bucket_array
{
    template <typename T>
    void test_insert_remove(Static_Array<T, 10> const& values)
    {
        isize mem_before = default_allocator()->get_stats().bytes_allocated;
        isize alive_before = trackers_alive();
        {
            Bucket_Array<T> arr;

            TEST(size(arr) == 0);
            TEST(capacity(arr) == 0);

            Handle i0 = insert(&arr, dup(values[0]));
            Handle i1 = insert(&arr, dup(values[1]));
            Handle i2 = insert(&arr, dup(values[2]));
            
            TEST(size(arr) == 3);
            TEST(capacity(arr) >= size(arr));

            TEST(get(arr, i0) == values[0]);
            TEST(get(arr, i1) == values[1]);
            TEST(get(arr, i2) == values[2]);
            
            //#if 0
            T v1 = remove(&arr, i1);
            TEST(v1 == values[1]);
            TEST(size(arr) == 2);

            Handle i3 = insert(&arr, dup(values[3]));
            Handle i4 = insert(&arr, dup(values[4]));

            TEST(get(arr, i0) == values[0]);
            TEST(get(arr, i2) == values[2]);
            TEST(get(arr, i3) == values[3]);
            TEST(get(arr, i4) == values[4]);
            TEST(size(arr) == 4);

            T v2 = remove(&arr, i2);
            TEST(v2 == values[2]);
            TEST(size(arr) == 3);

            Handle i5 = insert(&arr, dup(values[5]));
            isize cap = capacity(arr);
            TEST(get(arr, i0) == values[0]);
            TEST(get(arr, i3) == values[3]);
            TEST(get(arr, i4) == values[4]);
            TEST(get(arr, i5) == values[5]);
            TEST(size(arr) == 4);
            TEST(cap >= size(arr));
            
            Handle i6 = insert(&arr, dup(values[6]));
            Handle i7 = insert(&arr, dup(values[7]));
            Handle i8 = insert(&arr, dup(values[8]));
            Handle i9 = insert(&arr, dup(values[9]));
            Handle i10 = insert(&arr, dup(values[9]));
            Handle i11 = insert(&arr, dup(values[9]));
            Handle i12 = insert(&arr, dup(values[9]));
            Handle i13 = insert(&arr, dup(values[9]));
            Handle i14 = insert(&arr, dup(values[9]));
            Handle i15 = insert(&arr, dup(values[9]));

            TEST(size(arr) == 14);
            
            TEST(get(arr, i6) == values[6]);
            TEST(get(arr, i7) == values[7]);
            TEST(get(arr, i8) == values[8]);
            TEST(get(arr, i9) == values[9]);
            TEST(get(arr, i10) == values[9]);
            TEST(get(arr, i11) == values[9]);
            TEST(get(arr, i12) == values[9]);
            TEST(get(arr, i13) == values[9]);
            TEST(get(arr, i14) == values[9]);
            TEST(get(arr, i15) == values[9]);
            
            cap = capacity(arr);

            T v0 = remove(&arr, i0);
            T v3 = remove(&arr, i3);
            T v4 = remove(&arr, i4);
            T v5 = remove(&arr, i5);
            
            TEST(size(arr) == 10);
            TEST(capacity(arr) == cap && "capacity must not change when shrinking");

            TEST(v0 == values[0]);
            TEST(v1 == values[1]);
            TEST(v2 == values[2]);
            TEST(v3 == values[3]);
            TEST(v4 == values[4]);
            TEST(v5 == values[5]);
            //#endif
        }
        isize mem_after = default_allocator()->get_stats().bytes_allocated;
        isize alive_after = trackers_alive();

        TEST(alive_after == alive_before);
        TEST(mem_after == mem_before);
    }
    
    static
    void stress_test(bool print = true)
    {
        using Seed = std::random_device::result_type;

        if(print) println("  test_stress()");

        std::random_device rd;
        
        static const isize OP_INSERT = 0;
        static const isize OP_REMOVE = 1;

        std::discrete_distribution<unsigned> op_distribution({75, 25});
        std::uniform_int_distribution<unsigned> index_distribution(0);

        std::mt19937 gen;
        const auto test_batch = [&](isize block_size, isize j){
            i64 trackers_before = trackers_alive();
            isize memory_before = default_allocator()->get_stats().bytes_allocated;

            {
                Hash_Table<isize, Handle, int_hash<isize>> truth;
                Bucket_Array<isize> bucket_array;

                reserve(&truth, block_size);

                for(isize i = 0; i < block_size; i++)
                {
                    isize op = (isize) op_distribution(gen);
                    
                    bool skipped = false;
                    switch(op)
                    {
                        case OP_INSERT: {
                            Handle index = insert(&bucket_array, i);
                            set(&truth, i, index);
                            break;
                        }

                        case OP_REMOVE: {
                            Slice<const isize> truth_vals = keys(truth);
                            Slice<Handle> truth_indices = values(&truth);
                            if(truth_indices.size == 0)
                            {   
                                skipped = true;
                                break;
                            }

                            isize selected_index = (isize) index_distribution(gen) % truth_indices.size;
                            Handle removed_index = truth_indices[selected_index];
                            isize removed_value = truth_vals[selected_index];

                            isize just_removed = remove(&bucket_array, removed_index);
                            TEST(just_removed == removed_value);
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
                    Slice<Handle> truth_indices = values(&truth);
                    isize used_size = size(bucket_array);
                    TEST(used_size == truth_vals.size);

                    for(isize k = 0; k < truth_indices.size; k++)
                    {
                        Handle index = truth_indices[k];
                        isize retrieved = get(bucket_array, index);
                        TEST(retrieved == truth_vals[k]);
                    }
                }
            
                if(print) println("    i: {}\t batch: {}\t final_size: {}", j, block_size, size(bucket_array));
            }

            i64 trackers_after = trackers_alive();
            isize memory_after = default_allocator()->get_stats().bytes_allocated;
            TEST(trackers_before == trackers_after);
            TEST(memory_before == memory_after);
        };
        
        //3145023208
        //Seed seed = 3145023208;
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
}
}