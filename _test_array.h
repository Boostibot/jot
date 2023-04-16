#pragma once

#include <random>

#include "static_array.h"
#include "_test.h"
#include "array.h"
#include "format.h"

namespace jot::tests::array
{
    template<typename T>
    void test_push_pop(Static_Array<T, 6> vals)
    {
        isize mem_before = default_allocator()->get_stats().bytes_allocated;
        isize mem_after = default_allocator()->get_stats().bytes_allocated;
        i64 before = trackers_alive();
        i64 after = trackers_alive();
        {
            Array<T> arr;

            test(size(arr) == 0);
            test(capacity(arr) == 0);

            push(&arr, dup(vals[0]));
            
            test(size(arr) == 1);
            push(&arr, dup(vals[1]));

            test(size(arr) == 2);
            test(capacity(arr) >= 2);

            test(pop(&arr) == vals[1]);
            test(pop(&arr) == vals[0]);

            test(size(arr) == 0);
            test(capacity(arr) >= 2);

            push(&arr, dup(vals[2]));
            push(&arr, dup(vals[1]));
            push(&arr, dup(vals[0]));

            test(size(arr) == 3);
            test(capacity(arr) >= 3);
            
            test(arr[0] == vals[2]);
            test(arr[1] == vals[1]);
            test(arr[2] == vals[0]);

            push(&arr, dup(vals[2]));
            push(&arr, dup(vals[1]));
            push(&arr, dup(vals[0]));
            push(&arr, dup(vals[2]));
            push(&arr, dup(vals[1]));
            push(&arr, dup(vals[0]));

            test(size(arr) == 9);
            test(capacity(arr) >= 9);

            test(arr[0] == vals[2]);
            test(arr[1] == vals[1]);
            test(arr[2] == vals[0]);
            
            test(arr[6] == vals[2]);
            test(arr[7] == vals[1]);
            test(arr[8] == vals[0]);
            
            test(pop(&arr) == vals[0]);
            test(pop(&arr) == vals[1]);
            test(pop(&arr) == vals[2]);

            test(size(arr) == 6);
            test(arr[0] == vals[2]);
            test(arr[1] == vals[1]);
            test(arr[2] == vals[0]);
        }

        after = trackers_alive();
        mem_after = default_allocator()->get_stats().bytes_allocated;
        test(before == after);
        test(mem_before == mem_after);
        
        {
            Array<T> arr;
            Static_Array<T, 6> dupped = dup(vals);

            grow(&arr, 6);

            push_multiple_move(&arr, slice(&dupped));
            test(size(arr) == 6);
            test(arr[0] == vals[0]);
            test(arr[3] == vals[3]);
            test(arr[4] == vals[4]);
            test(arr[5] == vals[5]);

            pop_multiple(&arr, 2);
            test(size(arr) == 4);
            test(arr[0] == vals[0]);
            test(arr[1] == vals[1]);
            test(arr[3] == vals[3]);
            
            pop_multiple(&arr, 3);
            test(size(arr) == 1);
            test(arr[0] == vals[0]);

            dupped = dup(vals);
            push_multiple_move(&arr, slice(&dupped));
            test(size(arr) == 7);
            
            test(arr[0] == vals[0]);
            test(arr[1] == vals[0]);
            test(arr[4] == vals[3]);
            test(arr[5] == vals[4]);
            test(arr[6] == vals[5]);

            pop_multiple(&arr, 7);
            test(size(arr) == 0);
        }
        after = trackers_alive();
        mem_after = default_allocator()->get_stats().bytes_allocated;
        test(before == after);
        test(mem_before == mem_after);
    }

    template<typename T>
    void test_copy(Static_Array<T, 6> vals)
    {
        using Array = Array<T>;
        i64 before = trackers_alive();
        isize mem_before = default_allocator()->get_stats().bytes_allocated;
        isize mem_after = default_allocator()->get_stats().bytes_allocated;
        {
            Array arr;
            push(&arr, dup(vals[0]));
            push(&arr, dup(vals[1]));
            push(&arr, dup(vals[2]));
            push(&arr, dup(vals[2]));
            
            Array copied = arr;
            test(size(copied) == 4);
            test(capacity(copied) >= 4);

            test(copied[0] == vals[0]);
            test(copied[3] == vals[2]);

            test(arr[1] == vals[1]);
            test(arr[3] == vals[2]);
            
            push(&arr, dup(vals[1]));
            test(size(arr) == 5);
            test(size(copied) == 4);
            
            copied = arr;
            test(size(copied) == 5);

            test(copied[0] == vals[0]);
            test(copied[4] == vals[1]);

            test(arr[0] == vals[0]);
            test(arr[4] == vals[1]);

            //from zero filling up
            Array copied2 = arr;
            test(copied2[0] == vals[0]);
            test(copied2[3] == vals[2]);
            test(copied2[4] == vals[1]);
            
            Array copied3 = arr;
            push(&copied3, dup(vals[0]));
            push(&copied3, dup(vals[1]));
            push(&copied3, dup(vals[0]));
            push(&copied3, dup(vals[1]));

            test(size(copied3) == 9);
            
            //copieding to less elems with bigger capacity
            copied3 = arr;
            test(size(copied3) == 5);

            //copieding to more elems with bigger capacity
            pop(&copied3);
            pop(&copied3);
            pop(&copied3);

            copied3 = arr;
            test(size(copied3) == 5);
        }

        {
            //copying to zero elems
            Array empty;
            Array arr;
            push(&arr, dup(vals[0]));
            push(&arr, dup(vals[1]));
            push(&arr, dup(vals[2]));
            push(&arr, dup(vals[2]));
            test(size(arr) == 4);
            
            arr = empty;
            test(size(arr) == 0);
        }
        
        {
            Array empty;
            Array arr;
            
            arr = empty;
            test(size(arr) == 0);
        }
        i64 after = trackers_alive();
        test(before == after);
        mem_after = default_allocator()->get_stats().bytes_allocated;
        test(mem_before == mem_after);
    }

    template<typename T>
    void test_reserve(Static_Array<T, 6> vals)
    {
        using Array = Array<T>;
        i64 before = trackers_alive();
        i64 after = trackers_alive();
        isize mem_before = default_allocator()->get_stats().bytes_allocated;
        isize mem_after = default_allocator()->get_stats().bytes_allocated;

        {
            Array empty;
            reserve(&empty, 5);

            test(capacity(empty) >= 5);
            test(size(empty) == 0);
            reserve(&empty, 13);
            test(capacity(empty) >= 13);
            test(size(empty) == 0);
            
            
            push(&empty, dup(vals[0]));
            push(&empty, dup(vals[0]));
            push(&empty, dup(vals[0]));

            test(capacity(empty) >= 13);
            test(size(empty) == 3);
        }

        after = trackers_alive();
        test(before == after);
        mem_after = default_allocator()->get_stats().bytes_allocated;
        test(mem_before == mem_after);

        {
            Array arr;
            push(&arr, dup(vals[0]));
            push(&arr, dup(vals[0]));
            push(&arr, dup(vals[0]));
            test(capacity(arr) >= 3);
            test(size(arr) == 3);

            reserve(&arr, 7);
            test(capacity(arr) >= 7);
            test(size(arr) == 3);

            
            pop(&arr);
            reserve(&arr, 2);
            test(capacity(arr) >= 7);
            test(size(arr) == 2);
            
            push(&arr, dup(vals[1]));
            push(&arr, dup(vals[2]));
            push(&arr, dup(vals[3]));
            push(&arr, dup(vals[4]));
            push(&arr, dup(vals[5]));
            test(size(arr) == 7);
            test(capacity(arr) >= 7);
            
            test(arr[2] == vals[1]);
            test(arr[3] == vals[2]);
            test(arr[4] == vals[3]);

            set_capacity(&arr, 15);

            test(arr[2] == vals[1]);
            test(arr[3] == vals[2]);
            test(arr[4] == vals[3]);
            test(arr[5] == vals[4]);
            test(arr[6] == vals[5]);

            test(size(arr) == 7);
            test(capacity(arr) == 15);
            
            set_capacity(&arr, 5);
            
            test(arr[0] == vals[0]);
            test(arr[1] == vals[0]);
            test(arr[2] == vals[1]);
            test(arr[3] == vals[2]);
            test(arr[4] == vals[3]);
            
            test(size(arr) == 5);
            test(capacity(arr) == 5);

            
            set_capacity(&arr, 0);
            test(size(arr) == 0);
            test(capacity(arr) == 0);
        }

        after = trackers_alive();
        test(before == after);
        mem_after = default_allocator()->get_stats().bytes_allocated;
        test(mem_before == mem_after);
    }

    

    template<typename T>
    void test_resize(Static_Array<T, 6> vals)
    {
        using Array = Array<T>;
        i64 before = trackers_alive();
        isize mem_before = default_allocator()->get_stats().bytes_allocated;
        isize mem_after = default_allocator()->get_stats().bytes_allocated;

        {
            Array arr;
            resize(&arr, 5, vals[0]);
            test(size(arr) == 5);
            test(arr[0] == vals[0]);
            test(arr[2] == vals[0]);
            test(arr[4] == vals[0]);

            resize(&arr, 16);
            test(arr[0] == vals[0]);
            test(arr[2] == vals[0]);
            test(arr[4] == vals[0]);
            test(arr[5] == T());
            test(arr[9] == T());
            test(arr[11] == T());
            test(arr[15] == T());
        }

        {
            Array arr;
            resize(&arr, 7, vals[0]);

            test(capacity(arr) >= 7);
            test(size(arr) == 7);
            test(arr[0] == vals[0]);
            test(arr[4] == vals[0]);
            test(arr[6] == vals[0]);

            //growing
            resize(&arr, 11, vals[1]);
            resize(&arr, 12, vals[2]);
            test(capacity(arr) >= 12);
            test(size(arr) == 12);
            test(arr[7] == vals[1]);
            test(arr[9] == vals[1]);
            test(arr[10] == vals[1]);
            test(arr[11] == vals[2]);

            //shrinking
            resize(&arr, 11, vals[1]);
            test(capacity(arr) >= 12);
            test(size(arr) == 11);
            test(arr[0] == vals[0]);
            test(arr[6] == vals[0]);
            test(arr[10] == vals[1]);

            push(&arr, dup(vals[2]));

            resize(&arr, 7, vals[1]);
            test(capacity(arr) >= 12);
            test(size(arr) == 7);
            test(arr[1] == vals[0]);
            test(arr[3] == vals[0]);
            test(arr[6] == vals[0]);
        }
        i64 after = trackers_alive();
        test(before == after);
        mem_after = default_allocator()->get_stats().bytes_allocated;
        test(mem_before == mem_after);
    }

    template<typename T>
    void test_insert_remove(Static_Array<T, 6> vals)
    {
        using Array = Array<T>;
        i64 before = trackers_alive();
        isize mem_before = default_allocator()->get_stats().bytes_allocated;
        isize mem_after = default_allocator()->get_stats().bytes_allocated;

        {
            Array arr;
            resize(&arr, 5, vals[0]);

            insert(&arr, 2, dup(vals[1]));
            test(capacity(arr) >= 6);
            test(size(arr) == 6);

 
            test(arr[0] == vals[0]);
            test(arr[1] == vals[0]);
            test(arr[2] == vals[1]);
            test(arr[3] == vals[0]);
            test(arr[5] == vals[0]);

            insert(&arr, 2, dup(vals[2]));
            test(capacity(arr) >= 7);
            test(size(arr) == 7);
            test(arr[0] == vals[0]);
            test(arr[1] == vals[0]);
            test(arr[2] == vals[2]);
            test(arr[3] == vals[1]);
            test(arr[4] == vals[0]);
            test(arr[6] == vals[0]);

            test(remove(&arr, 2) == vals[2]);
            test(capacity(arr) >= 7);
            test(size(arr) == 6);
            test(arr[0] == vals[0]);
            test(arr[1] == vals[0]);
            test(arr[2] == vals[1]);
            test(arr[3] == vals[0]);
            test(arr[5] == vals[0]);

            test(remove(&arr, 0) == vals[0]);
            test(capacity(arr) >= 7);
            test(size(arr) == 5);
            test(arr[0] == vals[0]);
            test(arr[1] == vals[1]);
            test(arr[2] == vals[0]);
            test(arr[4] == vals[0]);
            
            insert(&arr, size(arr), dup(vals[3]));
            insert(&arr, size(arr), dup(vals[4]));
            test(size(arr) == 7);
            test(arr[2] == vals[0]);
            test(arr[4] == vals[0]);
            test(arr[5] == vals[3]);
            test(arr[6] == vals[4]);

            test(remove(&arr, size(arr) - 2) == vals[3]);
            test(remove(&arr, size(arr) - 1) == vals[4]);
        }

        mem_after = default_allocator()->get_stats().bytes_allocated;
        test(mem_before == mem_after);
        //unordered insert remove
        {
            Array arr;
            auto dupped = dup(vals);
            push_multiple_move(&arr, slice(&dupped));
            test(size(arr) == 6);

            test(unordered_remove(&arr, 3) == vals[3]);
            test(size(arr) == 5);
            test(arr[0] == vals[0]);
            test(arr[2] == vals[2]);
            test(arr[3] == vals[5]);
            test(arr[4] == vals[4]);

            test(unordered_remove(&arr, 4) == vals[4]);
            test(size(arr) == 4);
            test(arr[0] == vals[0]);
            test(arr[2] == vals[2]);
            test(arr[3] == vals[5]);

            unordered_insert(&arr, 0, dup(vals[5]));
            test(size(arr) == 5);
            test(arr[0] == vals[5]);
            test(arr[1] == vals[1]);
            test(arr[2] == vals[2]);
            test(arr[3] == vals[5]);
            test(arr[4] == vals[0]);
            
            unordered_insert(&arr, 5, dup(vals[3]));
            test(size(arr) == 6);
            test(arr[3] == vals[5]);
            test(arr[4] == vals[0]);
            test(arr[5] == vals[3]);
        }
            test(mem_before == mem_after);

        {
            Array empty;
            insert(&empty, 0, dup(vals[0]));
            test(capacity(empty) >= 1);
            test(size(empty) == 1);
            test(last(empty) == vals[0]);

            insert(&empty, 1, dup(vals[1]));
            test(capacity(empty) >= 2);
            test(size(empty) == 2);
            test(last(empty) == vals[1]);

            remove(&empty, 1);
            test(capacity(empty) >= 2);
            test(size(empty) == 1);
            test(last(empty) == vals[0]);

            remove(&empty, 0);
            test(capacity(empty) >= 2);
            test(size(empty) == 0);
        }
        i64 after = trackers_alive();
        test(before == after);
        mem_after = default_allocator()->get_stats().bytes_allocated;
        test(mem_before == mem_after);
    }
    
    static
    void test_stress(bool print)
    {
        using Track = Tracker<isize>;
        std::mt19937 gen;

        if(print) println("test_stress()");

        constexpr isize OP_PUSH1 = 0;
        constexpr isize OP_PUSH2 = 1;
        constexpr isize OP_PUSH3 = 2;
        constexpr isize OP_POP = 3;
        constexpr isize OP_RESERVE = 4;
        constexpr isize OP_SPLICE = 5;
        constexpr isize OP_INSERT = 6;
        constexpr isize OP_REMOVE = 7;
        constexpr isize OP_INSERT_UNORDERED = 8;
        constexpr isize OP_REMOVE_UNORDERED = 9;
        constexpr isize OP_SET_CAPACITY = 10;

        std::uniform_int_distribution<unsigned> op_distribution(0, 10);
        std::uniform_int_distribution<unsigned> index_distribution(0);
        std::uniform_int_distribution<unsigned> val_distribution(0);

        isize max_size = 1000;
        const auto test_batch = [&](isize block_size, isize k){
            i64 before = trackers_alive();
            isize mem_before = default_allocator()->get_stats().bytes_allocated;

            {
                Static_Array<Track, 30> to_insert = {
                    1, 2, 3, 4, 5, 
                    6, 7, 8, 9, 10,

                    1, 2, 3, 4, 5, 
                    6, 7, 8, 9, 10,

                    1, 2, 3, 4, 5, 
                    6, 7, 8, 9, 10,
                };

                Array<Track> arr;
                for(isize i = 0; i < block_size; i++)
                {
                    isize op = (isize) op_distribution(gen);
                    isize index = (isize) index_distribution(gen);
                    isize size = jot::size(arr);
                    isize size_incr = size + 1;
                    isize val = val_distribution(gen);

                    switch(op)
                    {
                        case OP_PUSH1: 
                        case OP_PUSH2: 
                        case OP_PUSH3:
                        {
                            push(&arr, Track{val});
                            break;
                        }

                        case OP_POP: 
                        {
                            if(size != 0)
                                pop(&arr); 
                            break;
                        }

                        case OP_RESERVE:
                        {
                            reserve(&arr, index % max_size);
                            break;
                        }
                       
                        case OP_INSERT:
                        {
                            insert(&arr, index % size_incr, Track{val});
                            break;
                        }

                        case OP_REMOVE:
                        {
                            if(size != 0)
                                remove(&arr, index % size);
                            break;
                        }

                        case OP_INSERT_UNORDERED:
                        {
                            unordered_insert(&arr, index % size_incr, Track{val});
                            break;
                        }

                        case OP_REMOVE_UNORDERED:
                        {
                            if(size != 0)
                                unordered_remove(&arr, index % size);
                            break;
                        }
                        
                        //Doesnt exist anymore
                        case OP_SPLICE:
                        {
                            break;
                        }

                        case OP_SET_CAPACITY:
                        {
                            isize set_to = index % max_size;
                            set_capacity(&arr, set_to);
                            break;
                        }

                        default: break;
                    }

                    test(is_invariant(arr));
                }
                
                if(print) println("  i: {}\t batch: {}\t final_size: {}", k, block_size, size(arr));
            }
            
            i64 after = trackers_alive();
            isize mem_after = default_allocator()->get_stats().bytes_allocated;
            test(before == after);
            test(mem_before == mem_after);
        };

        for(isize i = 0; i < 100; i++)
        {
            test_batch(10, i);
            test_batch(40, i);
            test_batch(160, i);
            test_batch(640, i);
        }
    }

    template<typename T>
    void test_array(Static_Array<T, 6> vals)
    {
        test_push_pop<T>(dup(vals));
        test_copy<T>(dup(vals));
        test_resize<T>(dup(vals));
        test_reserve<T>(dup(vals));
        test_insert_remove<T>(dup(vals));
    }
    

    static
    void test_array(u32 flags)
    {
        bool print = !(flags & Test_Flags::SILENT);

        Static_Array<i32, 6>           arr1 = {10, 20, 30, 40, 50, 60};
        Static_Array<char, 6>          arr2 = {'a', 'b', 'c', 'd', 'e', 'f'};
        Static_Array<Tracker<i32>, 6>  arr4 = {10, 20, 30, 40, 50, 60};

        if(print) println("\ntest_array()");
        if(print) println("  type: i32");
        test_array(arr1);
        
        if(print) println("  type: char");
        test_array(arr2);
        test_array(arr4);
        
        Static_Array<Test_String, 6>   arr3 = {"a", "b", "c", "d", "e", "some longer string..."};
        if(print) println("  type: Test_String");
        test_array(arr3);

        if(print) println("  type: Tracker<i32>");


        if(flags & Test_Flags::STRESS)
            test_stress(print);
    }
}
