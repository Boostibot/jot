#pragma once

#include <random>
#include <vector>
#include <unordered_map>

//#define BUCKET_ARRAY_NO_FFS

#include "_test.h"
#include "string_hash.h"
#include "bucket_array.h"
#include "format.h"
#include "benchmark.h"

#define BENCHMARK_ARRAY true
#define BENCHMARK_HASH_TABLE true
#define BENCHMARK_STD false
#define GIVEN_TIME 500
#define DEF_BUCKET_GROWTH Bucket_Array_Growth{256, 3, 2}

namespace jot::benchmarks
{
    //Adds batch size elements to a newly constructed container. Reporst time per add
    static void benchmark_cotainer_add();
    
    //Removes random element from a container containing batch size elements. 
    //The time to construct the container and fill it with values is discarded. Reports time per remove
    static void benchmark_cotainer_remove();
    
    //Ofline fills a container with random values then iterates it summing the values. 
    //Reports time per single element addition
    static void benchmark_cotainer_iterate();

    //Ofline fills a container with random values and makes an array filled with random keys
    // then iterates the array and lookups the random key. Reports time per lookup.
    static void benchmark_cotainer_find();

    //inserts an element, inserts another one then removes the first element inserted. 
    //Arrays (Array and vector) pop instead of removal of the first inserted element as such they are
    // purely for upper bound of whats possible
    //Reporst time per batch (insert + insert + remove)
    static void benchmark_cotainer_push_push_pop();
    
    //Effectively generalized push_push_pop except repeats each operation N times
    //First adds 2*N elements then removes either first or second half (switches every iteration)
    // and repeats. This should simulate a typical workload where we are either inserting or deleting and usually in batches.
    // The insertions are 2x more common because that seems to be a decent aproximation
    //Arrays (Array and vector) pop instead of removal of the first inserted element as such they are
    // purely for upper bound of whats possible
    //Reports time per batch ( (insert + insert + remove)*N )
    static void benchmark_cotainer_insert_remove_sections();
}

namespace jot
{
    template <> struct Formattable<Bench_Result>
    {
        static
        void format(String_Builder* appender, Bench_Result result) noexcept
        {
            format_into(appender, "{ ", CFormat_Float{result.mean_ms, "%.8lf"}, "ms ", result.deviation_ms, " δ ", to_padded_format(result.iters, 9, ' '), " i }");
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
        ::std::uniform_int_distribution<long long> uniform_dist(0);
        
        for (isize i = 0; i < items.size - 1; i++)
        {
            isize j = uniform_dist(*random) % (items.size - i);
            swap(&items[i], &items[i + j]);
        }
    }

    static void benchmark_cotainer_add()
    {
        const auto bench = [&](isize batch_size)
        {
            println("\nADD ", batch_size);
                    
            Bench_Result res_array = benchmark(GIVEN_TIME, [&]{
                Array<isize> array;
                for(isize i = 0; i < batch_size; i++)
                {
                    push(&array, i);
                    do_no_optimize(array);
                    read_write_barrier();
                }
                return true;
            }, batch_size);
            
            Bench_Result res_hash_table = benchmark(GIVEN_TIME, [&]{
                Hash_Table<isize, isize, int_hash<isize>> hash_table;
                for(isize i = 0; i < batch_size; i++)
                {
                    set(&hash_table, i, i);
                    do_no_optimize(hash_table);
                    read_write_barrier();
                }
                return true;
            }, batch_size);
            
            Bench_Result res_bucket_array = benchmark(GIVEN_TIME, [&]{
                Bucket_Array<isize> bucket_array;
                for(isize i = 0; i < batch_size; i++)
                {
                    (void) insert_bucket_index(&bucket_array, i, DEF_BUCKET_GROWTH);
                    do_no_optimize(bucket_array);
                    read_write_barrier();
                }
                return true;
            }, batch_size);

            
            Bench_Result res_slot_array = benchmark(GIVEN_TIME, [&]{
                Slot_Array<isize> slot_array;
                for(isize i = 0; i < batch_size; i++)
                {
                    (void) insert(&slot_array, i);
                    do_no_optimize(slot_array);
                    read_write_barrier();
                }
                return true;
            }, batch_size);

            Bench_Result res_vector;
            Bench_Result res_unordered_map;

            if(BENCHMARK_STD)
            {
                res_vector = benchmark(GIVEN_TIME, [&]{
                    std::vector<isize> vec;
                    for(isize i = 0; i < batch_size; i++)
                    {
                        vec.push_back(i);
                        do_no_optimize(vec);
                        read_write_barrier();
                    }
                    return true;
                }, batch_size);
            
                res_unordered_map = benchmark(GIVEN_TIME, [&]{
                    std::unordered_map<isize, isize> map;
                    for(isize i = 0; i < batch_size; i++)
                    {
                        map.insert_or_assign(i, i);
                        do_no_optimize(map);
                        read_write_barrier();
                    }
                    return true;
                }, batch_size);
            }

            if(BENCHMARK_STD)
            {
                println("vector:       ", res_vector);
                println("unordered_map:", res_unordered_map);
            }

            println("array:        ", res_array);
            println("hash_table:   ", res_hash_table);
            println("bucket_array: ", res_bucket_array);
            println("slot_array:   ", res_slot_array);
        };
        
        println("\n=== ignore below ===");
        bench(1000);
        println("=== ignore above ===\n");
        bench(10);
        bench(100);
        bench(1000);
        bench(10000);
        bench(100000);
    }

    static void benchmark_cotainer_remove()
    {
        const auto bench = [&](isize batch_size)
        {
            println("\nREMOVE ", batch_size);
        
            Array<isize> array;
            Hash_Table<isize, isize, int_hash<isize>> hash_table;
            Bucket_Array<isize> bucket_array;
            Slot_Array<isize> slot_array;
            std::vector<isize> vec;
            std::unordered_map<isize, isize> map;

            isize removed_i = 0;
            Random_Device random_device;
            Random_Generator gen(random_device());

            Array<isize> added_keys;
            resize(&added_keys, batch_size);

            Bench_Result res_array = benchmark(GIVEN_TIME, [&]{
                if(size(array) == 0)
                {
                    for(isize i = 0; i < batch_size; i++)
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
            Bench_Result res_hash_table = benchmark(GIVEN_TIME, [&]{
                if(removed_i == 0)
                {
                    for(isize i = 0; i < batch_size; i++)
                    {
                        set(&hash_table, i, i);
                        added_keys[i] = i;
                    }
                
                    shuffle(slice(&added_keys), &gen);
                
                    removed_i = batch_size;
                    return false;
                }

                isize removed = added_keys[--removed_i];
                remove(&hash_table, removed);
                do_no_optimize(hash_table);
                read_write_barrier();
                return true;
            });
            
            removed_i = 0;
            Bench_Result res_mark_hash_table = benchmark(GIVEN_TIME, [&]{
                if(removed_i == 0)
                {
                    for(isize i = 0; i < batch_size; i++)
                    {
                        set(&hash_table, i, i);
                        added_keys[i] = i;
                    }
                
                    shuffle(slice(&added_keys), &gen);
                
                    removed_i = batch_size;
                    return false;
                }
            
                isize removed = added_keys[--removed_i];
                mark_removed(&hash_table, removed);
                do_no_optimize(hash_table);
                read_write_barrier();
                return true;
            });
            
            removed_i = 0;
            Bench_Result res_bucket_array = benchmark(GIVEN_TIME, [&]{
                if(removed_i == 0)
                {
                    for(isize i = 0; i < batch_size; i++)
                        added_keys[i] = insert(&bucket_array, i, DEF_BUCKET_GROWTH);
                
                    shuffle(slice(&added_keys), &gen);
                
                    removed_i = batch_size;
                    return false;
                }
            
                isize removed = added_keys[--removed_i];
                remove(&bucket_array, removed);
                do_no_optimize(bucket_array);
                read_write_barrier();
                return true;
            });

            
            removed_i = 0;
            Bench_Result res_slot_array = benchmark(GIVEN_TIME, [&]{
                if(removed_i == 0)
                {
                    for(isize i = 0; i < batch_size; i++)
                        added_keys[i] = (isize) insert(&slot_array, i);
                
                    shuffle(slice(&added_keys), &gen);
                
                    removed_i = batch_size;
                    return false;
                }
            
                isize removed = added_keys[--removed_i];
                remove(&slot_array, (Slot) removed);
                do_no_optimize(slot_array);
                read_write_barrier();
                return true;
            });

            Bench_Result res_vector;
            Bench_Result res_unordered_map;

            if(BENCHMARK_STD)
            {
                res_vector = benchmark(GIVEN_TIME, [&]{
                    if(vec.size() == 0)
                    {
                        for(isize i = 0; i < batch_size; i++)
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
                res_unordered_map = benchmark(GIVEN_TIME, [&]{
                    if(removed_i == 0)
                    {
                        for(isize i = 0; i < batch_size; i++)
                        {
                            map.insert_or_assign(i, i);
                            added_keys[i] = i;
                        }
                
                        shuffle(slice(&added_keys), &gen);
                
                        removed_i = batch_size;
                        return false;
                    }
                
                    isize removed = added_keys[--removed_i];
                    map.extract(removed);
                    do_no_optimize(map);
                    read_write_barrier();
                    return true;
                });
            }

            if(BENCHMARK_STD)
            {
            println("vector:            ", res_vector);
            println("unordered_map:     ", res_unordered_map);
            }

            println("array:             ", res_array);
            println("hash_table:        ", res_hash_table);
            println("hash_table mark:   ", res_mark_hash_table);
            println("bucket_array:      ", res_bucket_array);
            println("slot_array:        ", res_slot_array);
        };
        
        println("\n=== ignore below ===");
        bench(1000);
        println("=== ignore above ===\n");
        bench(10);
        bench(100);
        bench(1000);
        bench(10000);
        bench(100000);
    }
    
    static void benchmark_cotainer_iterate()
    {
        const auto bench = [&](isize batch_size)
        {
            println("\nITERATE ", batch_size);
        
            Array<isize> array;
            Hash_Table<isize, isize, int_hash<isize>> hash_table;
            Bucket_Array<isize> bucket_array;
            Slot_Array<isize> slot_array;
            std::vector<isize> vec;
            std::unordered_map<isize, isize> map;

            for(isize i = 0; i < batch_size; i++)
            {
                push(&array, i);
                set(&hash_table, i, i);
                map.insert_or_assign(i, i);
                vec.push_back(i);
                (void) insert(&bucket_array, i, DEF_BUCKET_GROWTH);
                (void) insert(&slot_array, i);
            }

            isize sum = 0;
            do_no_optimize(sum);

            Bench_Result res_array = benchmark(GIVEN_TIME, [&]{
                for(isize i = 0; i < batch_size; i++)
                    sum += array[i];

                do_no_optimize(array);
                read_write_barrier();
                return true;
            }, batch_size);

            Bench_Result res_hash_table = benchmark(GIVEN_TIME, [&]{
                auto vals = values(hash_table);
                for(isize i = 0; i < batch_size; i++)
                    sum += vals[i];

                do_no_optimize(hash_table);
                read_write_barrier();
                return true;
            }, batch_size);
            
            Bench_Result res_bucket_array = benchmark(GIVEN_TIME, [&]{
            
                map_mutate(&bucket_array, [&](isize* item, isize, isize){
                    sum += *item;
                });

                do_no_optimize(hash_table);
                read_write_barrier();

                return true;
            }, batch_size);
            
            Bench_Result res_slot_array = benchmark(GIVEN_TIME, [&]{
                auto vals = slice(slot_array);
                for(isize i = 0; i < batch_size; i++)
                    sum += vals[i];

                do_no_optimize(hash_table);
                read_write_barrier();

                return true;
            }, batch_size);

            Bench_Result res_vector;
            Bench_Result res_unordered_map;

            if(BENCHMARK_STD)
            {
                res_vector = benchmark(GIVEN_TIME, [&]{
                    for(isize i = 0; i < batch_size; i++)
                        sum += vec[(size_t) i];
                        
                    do_no_optimize(vec);
                    read_write_barrier();
                    return true;
                }, batch_size);
            
                res_unordered_map = benchmark(GIVEN_TIME, [&]{
                    for(auto [key, val] : map)
                        sum += val;
                    do_no_optimize(map);
                    read_write_barrier();
                
                    return true;
                }, batch_size);
            }
            
            if(BENCHMARK_STD)
            {
            println("vector:            ", res_vector);
            println("unordered_map:     ", res_unordered_map);
            }

            println("array:             ", res_array);
            println("hash_table:        ", res_hash_table);
            println("bucket_array:      ", res_bucket_array);
            println("slot_array:        ", res_slot_array);
        };
        
        println("\n=== ignore below ===");
        bench(1000);
        println("=== ignore above ===\n");
        bench(10);
        bench(100);
        bench(1000);
        bench(10000);
        bench(100000);
    }

    static void benchmark_cotainer_find()
    {
        const auto bench = [&](isize batch_size)
        {
            println("\nFIND ", batch_size);
        
            Array<isize> array;
            Hash_Table<isize, isize, int_hash<isize>> hash_table;
            Bucket_Array<isize> bucket_array;
            Slot_Array<isize> slot_array;
            std::vector<isize> vec;
            std::unordered_map<isize, isize> map;

            Array<isize> added_keys;
            Array<isize> added_bucket_keys;
            Array<uint32_t> added_slot_keys;
            resize(&added_keys, batch_size);
            resize(&added_bucket_keys, batch_size);
            resize(&added_slot_keys, batch_size);

            for(isize i = 0; i < batch_size; i++)
            {
                push(&array, i);
                set(&hash_table, i, i);
                map.insert_or_assign(i, i);
                vec.push_back(i);
                added_bucket_keys[i] = insert(&bucket_array, i, DEF_BUCKET_GROWTH);
                added_slot_keys[i] = insert(&slot_array, i);
                added_keys[i] = i;
            }
            
            Random_Device random_device;
            auto seed = random_device();

            //so that the arrays are shuffled the same way
            Random_Generator gen1(seed);
            Random_Generator gen2(seed);

            shuffle(slice(&added_keys), &gen1);
            shuffle(slice(&added_bucket_keys), &gen2);

            isize sum = 0;
            do_no_optimize(sum);

            isize i = 0;
            Bench_Result res_array = benchmark(GIVEN_TIME, [&]{
                sum += array[added_keys[i]];
                do_no_optimize(array);
                read_write_barrier();

                if(++i >= batch_size)
                    i = 0;

                return true;
            });

            i = 0;
            Bench_Result res_hash_table = benchmark(GIVEN_TIME, [&]{
                sum += get(hash_table, added_keys[i], 0);
                do_no_optimize(hash_table);
                read_write_barrier();
                
                if(++i >= batch_size)
                    i = 0;

                return true;
            });
            
            i = 0;
            Bench_Result res_bucket_array = benchmark(GIVEN_TIME, [&]{
                sum += get(bucket_array, added_bucket_keys[i]);
                do_no_optimize(hash_table);
                read_write_barrier();
                
                if(++i >= batch_size)
                    i = 0;

                return true;
            });

            
            i = 0;
            Bench_Result res_slot_array = benchmark(GIVEN_TIME, [&]{
                sum += get(bucket_array, added_slot_keys[i]);
                do_no_optimize(hash_table);
                read_write_barrier();
                
                if(++i >= batch_size)
                    i = 0;

                return true;
            });

            Bench_Result res_vector;
            Bench_Result res_unordered_map;

            if(BENCHMARK_STD)
            {
                i = 0;
                res_vector = benchmark(GIVEN_TIME, [&]{
                    sum += vec[(size_t) added_keys[i]];
                    do_no_optimize(vec);
                    read_write_barrier();

                    if(++i >= batch_size)
                        i = 0;

                    return true;
                });
            
                i = 0;
                res_unordered_map = benchmark(GIVEN_TIME, [&]{
                    sum += map.at(added_keys[i]);
                    do_no_optimize(map);
                    read_write_barrier();
                
                    if(++i >= batch_size)
                        i = 0;

                    return true;
                });
            }

            if(BENCHMARK_STD)
            {
            println("vector:            ", res_vector);
            println("unordered_map:     ", res_unordered_map);
            }

            println("array:             ", res_array);
            println("hash_table:        ", res_hash_table);
            println("bucket_array:      ", res_bucket_array);
            println("slot_array:        ", res_slot_array);
        };
        
        println("\n=== ignore below ===");
        bench(1000);
        println("=== ignore above ===\n");
        bench(10);
        bench(100);
        bench(1000);
        bench(10000);
        bench(100000);
    }

    static void benchmark_cotainer_push_push_pop()
    {
        const auto bench = [&](isize batch_size)
        {
            println("\nPUSH PUSH POP ", batch_size);

            isize counter = 0;
            Bench_Result res_array = benchmark(GIVEN_TIME, [&]{
                
                Array<isize> array;
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

            Bench_Result res_hash_table = benchmark(GIVEN_TIME, [&]{
                
                Hash_Table<isize, isize, int_hash<isize>> hash_table;
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
            
            Bench_Result res_mark_hash_table = benchmark(GIVEN_TIME, [&]{
                
                Hash_Table<isize, isize, int_hash<isize>> hash_table;
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
            
            Bench_Result res_bucket_array = benchmark(GIVEN_TIME, [&]{
            
                Bucket_Array<isize> bucket_array;
                for(isize i = 0; i < batch_size; i++)
                {
                    isize added = insert(&bucket_array, counter++, DEF_BUCKET_GROWTH);
                    (void) insert(&bucket_array, counter++, DEF_BUCKET_GROWTH);
                    remove(&bucket_array, added);
                }
                do_no_optimize(bucket_array);
                read_write_barrier();
                return true;
            }, batch_size);
            
            Bench_Result res_slot_array = benchmark(GIVEN_TIME, [&]{
            
                Slot_Array<isize> slot_array;
                for(isize i = 0; i < batch_size; i++)
                {
                    Slot added = insert(&slot_array, counter++);
                    (void) insert(&slot_array, counter++);
                    remove(&slot_array, added);
                }
                do_no_optimize(slot_array);
                read_write_barrier();
                return true;
            }, batch_size);

            Bench_Result res_vector;
            Bench_Result res_unordered_map;

            if(BENCHMARK_STD)
            {
                res_vector = benchmark(GIVEN_TIME, [&]{
                    std::vector<isize> vec;
                    for(isize i = 0; i < batch_size; i++)
                    {
                        vec.push_back(counter++);
                        vec.push_back(counter++);
                        vec.pop_back();
                    }
                    do_no_optimize(vec);
                    read_write_barrier();
                    return true;
                }, batch_size);
            
                res_unordered_map = benchmark(GIVEN_TIME, [&]{
                    std::unordered_map<isize, isize> map;
                    for(isize i = 0; i < batch_size; i++)
                    {
                        map.insert_or_assign(counter, counter);
                        map.insert_or_assign(counter + 1, counter + 1);
                        map.extract(counter);
                        counter += 2;
                    }
                    do_no_optimize(map);
                    read_write_barrier();
                    return true;
                }, batch_size);
            }

            if(BENCHMARK_STD)
            {
            println("vector:            ", res_vector);
            println("unordered_map:     ", res_unordered_map);
            }

            println("array:             ", res_array);
            println("hash_table:        ", res_hash_table);
            println("hash_table mark:   ", res_mark_hash_table);
            println("bucket_array:      ", res_bucket_array);
            println("slot_array:        ", res_slot_array);
        };
        
        println("\n=== ignore below ===");
        bench(1000);
        println("=== ignore above ===\n");
        bench(10);
        bench(100);
        bench(1000);
        bench(10000);
        bench(100000);
    }

    static void benchmark_cotainer_insert_remove_sections()
    {
        isize LOCAL_GIVEN_TIME = GIVEN_TIME * 2;

        const auto bench = [&](isize batch_size, isize section_size)
        {
            println("\nINSERT REMOVE SECTIONS {} section: {}", batch_size, section_size);

            Array<isize> key_array;
            resize(&key_array, 2*section_size);

            isize counter = 0;
            isize effective_batch_size = div_round_up(batch_size, section_size);
            Bench_Result res_array = benchmark(LOCAL_GIVEN_TIME, [&]{
                
                Array<isize> array;
                for(isize i = 0; i < effective_batch_size; i++)
                {
                    for(isize j = 0; j < section_size; j++)
                    {
                        key_array[j] = j;
                        push(&array, size(array));
                    }
                        
                    for(isize j = 0; j < section_size; j++)
                    {
                        key_array[j + section_size] = j + section_size;
                        push(&array, size(array));
                    }

                    if(i % 2 == 0)
                        counter += 3;
                    else
                        counter += 2;

                    do_no_optimize(counter);

                    for(isize j = 0; j < section_size; j++)
                        pop(&array);
                }

                do_no_optimize(array);
                read_write_barrier();
                return true;
            });

            Bench_Result res_hash_table = benchmark(LOCAL_GIVEN_TIME, [&]{
                
                Hash_Table<isize, isize, int_hash<isize>> hash_table;
                for(isize i = 0; i < effective_batch_size; i++)
                {
                    isize first_half = (2*i + 0)*section_size;
                    isize second_half = (2*i + 1)*section_size;
                    
                    for(isize j = 0; j < section_size; j++)
                    {
                        key_array[j] = j + first_half;
                        set(&hash_table, j + first_half, counter);
                    }
                        
                    for(isize j = 0; j < section_size; j++)
                    {
                        key_array[j] = j + second_half;
                        set(&hash_table, j + second_half, counter);
                    }
                    
                    isize from = i % 2 == 0 ? 0 : section_size; 
                    for(isize j = 0; j < section_size; j++)
                        remove(&hash_table, key_array[j + from]);

                    counter += 2;
                }

                do_no_optimize(hash_table);
                read_write_barrier();
                return true;
            });
            
            
            Bench_Result res_mark_hash_table = benchmark(LOCAL_GIVEN_TIME, [&]{
                
                Hash_Table<isize, isize, int_hash<isize>> hash_table;
                for(isize i = 0; i < effective_batch_size; i++)
                {
                    isize first_half = (2*i + 0)*section_size;
                    isize second_half = (2*i + 1)*section_size;
                    
                    for(isize j = 0; j < section_size; j++)
                    {
                        key_array[j] = j + first_half;
                        set(&hash_table, j + first_half, counter);
                    }
                        
                    for(isize j = 0; j < section_size; j++)
                    {
                        key_array[j + section_size] = j + second_half;
                        set(&hash_table, j + second_half, counter);
                    }
                    
                    isize from = i % 2 == 0 ? 0 : section_size; 
                    for(isize j = 0; j < section_size; j++)
                        mark_removed(&hash_table, key_array[j + from]);

                    counter += 2;
                }

                do_no_optimize(hash_table);
                read_write_barrier();
                return true;
            });
            
            Bench_Result res_bucket_array = benchmark(LOCAL_GIVEN_TIME, [&]{
                
                Bucket_Array<isize> bucket_array;
                for(isize i = 0; i < effective_batch_size; i++)
                {
                    for(isize j = 0; j < section_size; j++)
                        key_array[j] = insert(&bucket_array, counter++, DEF_BUCKET_GROWTH);
                        
                    for(isize j = 0; j < section_size; j++)
                        key_array[j + section_size] = insert(&bucket_array, counter++, DEF_BUCKET_GROWTH);
                    
                    isize from = i % 2 == 0 ? 0 : section_size; 
                    for(isize j = 0; j < section_size; j++)
                        remove(&bucket_array, key_array[j + from]);

                    counter += 2;
                }

                do_no_optimize(bucket_array);
                read_write_barrier();
                return true;
            });
            
            Bench_Result res_slot_array = benchmark(LOCAL_GIVEN_TIME, [&]{
                
                Slot_Array<isize> slot_array;
                for(isize i = 0; i < effective_batch_size; i++)
                {
                    for(isize j = 0; j < section_size; j++)
                        key_array[j] = insert(&slot_array, counter++);
                        
                    for(isize j = 0; j < section_size; j++)
                        key_array[j + section_size] = insert(&slot_array, counter++);
                    
                    isize from = i % 2 == 0 ? 0 : section_size; 
                    for(isize j = 0; j < section_size; j++)
                        remove(&slot_array, (Slot) key_array[j + from]);

                    counter += 2;
                }

                do_no_optimize(slot_array);
                read_write_barrier();
                return true;
            });

            Bench_Result res_vector;
            Bench_Result res_unordered_map;

            if(BENCHMARK_STD)
            {
                res_vector = benchmark(LOCAL_GIVEN_TIME, [&]{
                
                    std::vector<isize> vec;
                    for(isize i = 0; i < effective_batch_size; i++)
                    {
                        for(isize j = 0; j < section_size; j++)
                        {
                            key_array[j] = j;
                            vec.push_back(counter++);
                        }
                        
                        for(isize j = 0; j < section_size; j++)
                        {
                            key_array[j + section_size] = j + section_size;
                            vec.push_back(counter++);
                        }

                        if(i % 2 == 0)
                            counter += 3;
                        else
                            counter += 2;

                        do_no_optimize(counter);

                        for(isize j = 0; j < section_size; j++)
                            vec.pop_back();
                    }

                    do_no_optimize(vec);
                    read_write_barrier();
                    return true;
                });
            
                res_unordered_map = benchmark(LOCAL_GIVEN_TIME, [&]{
                
                    std::unordered_map<isize, isize> map;
                    for(isize i = 0; i < effective_batch_size; i++)
                    {
                        isize first_half = (2*i + 0)*section_size;
                        isize second_half = (2*i + 1)*section_size;
                    
                        for(isize j = 0; j < section_size; j++)
                        {
                            key_array[j] = j + first_half;
                            map.insert_or_assign(j + first_half, counter);
                        }
                        
                        for(isize j = 0; j < section_size; j++)
                        {
                            key_array[j] = j + second_half;
                            map.insert_or_assign(j + second_half, counter);
                        }
                    
                        isize from = i % 2 == 0 ? 0 : section_size; 
                        for(isize j = 0; j < section_size; j++)
                            map.extract(key_array[j + from]);

                        counter += 2;
                    }

                    do_no_optimize(map);
                    read_write_barrier();
                    return true;
                });
            }

            if(BENCHMARK_STD)
            {
            println("vector:            ", res_vector);
            println("unordered_map:     ", res_unordered_map);
            }

            println("array:             ", res_array);
            println("hash_table:        ", res_hash_table);
            println("hash_table mark:   ", res_mark_hash_table);
            println("bucket_array:      ", res_bucket_array);
            println("slot_array:        ", res_slot_array);
        };
        
        println("\n=== ignore below ===");
        bench(1000, 10);
        println("=== ignore above ===\n");
        bench(30, 3);
        bench(100, 10);
        bench(1000, 10);
        bench(10000, 100);
        bench(100000, 100);
    }
}

