#pragma once

#include <random>

#include "array.h"
#include "_test.h"
#include "stack.h"
#include "format.h"

namespace jot::tests::stack
{
    template<typename T>
    void test_push_pop(Array<T, 6> vals)
    {
        i64 before = trackers_alive();
        {
            Stack<T> stack;

            test(size(stack) == 0);
            test(capacity(stack) == 0);

            push(&stack, dup(vals[0]));
            test(size(stack) == 1);
            push(&stack, dup(vals[1]));

            test(size(stack) == 2);
            test(capacity(stack) >= 2);

            test(pop(&stack) == vals[1]);
            test(pop(&stack) == vals[0]);

            test(size(stack) == 0);
            test(capacity(stack) >= 2);

            push(&stack, dup(vals[2]));
            push(&stack, dup(vals[1]));
            push(&stack, dup(vals[0]));

            test(size(stack) == 3);
            test(capacity(stack) >= 3);

            test(stack[0] == vals[2]);
            test(stack[1] == vals[1]);
            test(stack[2] == vals[0]);
            
            test(pop(&stack) == vals[0]);
            test(pop(&stack) == vals[1]);
            test(pop(&stack) == vals[2]);

            test(size(stack) == 0);
        }
        
        {
            Stack<T> stack;
            Array<T, 6> dupped = dup(vals);
            push_multiple_move(&stack, slice(&dupped));
            test(size(stack) == 6);
            
            test(stack[0] == vals[0]);
            test(stack[3] == vals[3]);
            test(stack[4] == vals[4]);
            test(stack[5] == vals[5]);

            pop_multiple(&stack, 2);
            test(size(stack) == 4);
            test(stack[0] == vals[0]);
            test(stack[1] == vals[1]);
            test(stack[3] == vals[3]);
            
            pop_multiple(&stack, 3);
            test(size(stack) == 1);
            test(stack[0] == vals[0]);

            dupped = dup(vals);
            push_multiple_move(&stack, slice(&dupped));
            test(size(stack) == 7);
            test(stack[0] == vals[0]);
            test(stack[1] == vals[0]);
            test(stack[4] == vals[3]);
            test(stack[5] == vals[4]);
            test(stack[6] == vals[5]);

            pop_multiple(&stack, 7);
            test(size(stack) == 0);
        }
        i64 after = trackers_alive();
        test(before == after);
    }

    template<typename T>
    void test_copy(Array<T, 6> vals)
    {
        using Stack = Stack<T>;
        i64 before = trackers_alive();
        {
            Stack stack;
            push(&stack, dup(vals[0]));
            push(&stack, dup(vals[1]));
            push(&stack, dup(vals[2]));
            push(&stack, dup(vals[2]));

            Stack copied = stack;
            test(size(copied) == 4);
            test(capacity(copied) >= 4);

            test(copied[0] == vals[0]);
            test(copied[3] == vals[2]);

            test(stack[1] == vals[1]);
            test(stack[3] == vals[2]);

            //not linked
            push(&stack, dup(vals[1]));
            test(size(stack) == 5);
            test(size(copied) == 4);

            copied = stack;
            test(size(copied) == 5);

            test(copied[0] == vals[0]);
            test(copied[4] == vals[1]);

            test(stack[0] == vals[0]);
            test(stack[4] == vals[1]);

            //from zero filling up
            Stack copied2 = stack;
            test(copied2[0] == vals[0]);
            test(copied2[3] == vals[2]);
            test(copied2[4] == vals[1]);

            
            Stack copied3 = stack;
            push(&copied3, dup(vals[0]));
            push(&copied3, dup(vals[1]));
            push(&copied3, dup(vals[0]));
            push(&copied3, dup(vals[1]));

            test(size(copied3) == 9);

            //copieding to less elems with bigger capacity
            copied3 = stack;
            test(size(copied3) == 5);

            //copieding to more elems with bigger capacity
            pop(&copied3);
            pop(&copied3);
            pop(&copied3);

            copied3 = stack;
            test(size(copied3) == 5);
        }

        {
            //copying to zero elems
            Stack empty;
            Stack stack;
            push(&stack, dup(vals[0]));
            push(&stack, dup(vals[1]));
            push(&stack, dup(vals[2]));
            push(&stack, dup(vals[2]));
            test(size(stack) == 4);
            
            stack = empty;
            test(size(stack) == 0);
        }

        {
            //copy constructing empty
            Stack empty;
            Stack stack;
            
            stack = empty;
            test(size(stack) == 0);
        }
        i64 after = trackers_alive();
        test(before == after);
    }

    template<typename T>
    void test_reserve(Array<T, 6> vals)
    {
        using Stack = Stack<T>;
        i64 before = trackers_alive();

        {
            Stack empty;
            reserve(&empty, 5);

            test(capacity(empty) >= 5);
            test(size(empty) == 0);

            reserve(&empty, 13);
            test(capacity(empty) >= 13);
            test(size(empty) == 0);
        }
        {
            Stack stack;
            push(&stack, dup(vals[0]));
            push(&stack, dup(vals[0]));
            push(&stack, dup(vals[0]));
            test(capacity(stack) >= 3);
            test(size(stack) == 3);

            reserve(&stack, 7);
            test(capacity(stack) >= 7);
            test(size(stack) == 3);

            pop(&stack);
            reserve(&stack, 2);
            test(capacity(stack) >= 7);
            test(size(stack) == 2);
        }
        i64 after = trackers_alive();
        test(before == after);
    }

    template<typename T>
    void test_resize(Array<T, 6> vals)
    {
        using Stack = Stack<T>;
        i64 before = trackers_alive();

        {
            Stack empty;
            resize(&empty, 5, vals[0]);
            test(size(empty) == 5);
            test(empty[0] == vals[0]);
            test(empty[2] == vals[0]);
            test(empty[4] == vals[0]);

            resize(&empty, 16);
            test(empty[5] == T());
            test(empty[9] == T());
            test(empty[11] == T());
            test(empty[15] == T());
        }

        {
            Stack stack;
            resize(&stack, 7, vals[0]);

            test(capacity(stack) >= 7);
            test(size(stack) == 7);
            test(stack[0] == vals[0]);
            test(stack[4] == vals[0]);
            test(stack[6] == vals[0]);

            //growing
            resize(&stack, 11, vals[1]);
            resize(&stack, 12, vals[2]);
            test(capacity(stack) >= 12);
            test(size(stack) == 12);
            test(stack[7] == vals[1]);
            test(stack[9] == vals[1]);
            test(stack[10] == vals[1]);
            test(stack[11] == vals[2]);

            //shrinking
            resize(&stack, 11, vals[1]);
            test(capacity(stack) >= 12);
            test(size(stack) == 11);
            test(stack[0] == vals[0]);
            test(stack[6] == vals[0]);
            test(stack[10] == vals[1]);

            push(&stack, dup(vals[2]));

            resize(&stack, 7, vals[1]);
            test(capacity(stack) >= 12);
            test(size(stack) == 7);
            test(stack[1] == vals[0]);
            test(stack[3] == vals[0]);
            test(stack[6] == vals[0]);
        }
        i64 after = trackers_alive();
        test(before == after);
    }

    template<typename T>
    void test_insert_remove(Array<T, 6> vals)
    {
        using Stack = Stack<T>;
        i64 before = trackers_alive();

        {
            Stack stack;
            resize(&stack, 5, vals[0]);

            insert(&stack, 2, dup(vals[1]));
            test(capacity(stack) >= 6);
            test(size(stack) == 6);

 
            test(stack[0] == vals[0]);
            test(stack[1] == vals[0]);
            test(stack[2] == vals[1]);
            test(stack[3] == vals[0]);
            test(stack[5] == vals[0]);

            insert(&stack, 2, dup(vals[2]));
            test(capacity(stack) >= 7);
            test(size(stack) == 7);
            test(stack[0] == vals[0]);
            test(stack[1] == vals[0]);
            test(stack[2] == vals[2]);
            test(stack[3] == vals[1]);
            test(stack[4] == vals[0]);
            test(stack[6] == vals[0]);

            test(remove(&stack, 2) == vals[2]);
            test(capacity(stack) >= 7);
            test(size(stack) == 6);
            test(stack[0] == vals[0]);
            test(stack[1] == vals[0]);
            test(stack[2] == vals[1]);
            test(stack[3] == vals[0]);
            test(stack[5] == vals[0]);

            test(remove(&stack, 0) == vals[0]);
            test(capacity(stack) >= 7);
            test(size(stack) == 5);
            test(stack[0] == vals[0]);
            test(stack[1] == vals[1]);
            test(stack[2] == vals[0]);
            test(stack[4] == vals[0]);
            
            insert(&stack, size(stack), dup(vals[3]));
            insert(&stack, size(stack), dup(vals[4]));
            test(size(stack) == 7);
            test(stack[2] == vals[0]);
            test(stack[4] == vals[0]);
            test(stack[5] == vals[3]);
            test(stack[6] == vals[4]);

            test(remove(&stack, size(stack) - 2) == vals[3]);
            test(remove(&stack, size(stack) - 1) == vals[4]);
        }
        
        //unordered insert remove
        {
            Stack stack;
            auto dupped = dup(vals);
            push_multiple_move(&stack, slice(&dupped));
            test(size(stack) == 6);

            test(unordered_remove(&stack, 3) == vals[3]);
            test(size(stack) == 5);
            test(stack[0] == vals[0]);
            test(stack[2] == vals[2]);
            test(stack[3] == vals[5]);
            test(stack[4] == vals[4]);

            test(unordered_remove(&stack, 4) == vals[4]);
            test(size(stack) == 4);
            test(stack[0] == vals[0]);
            test(stack[2] == vals[2]);
            test(stack[3] == vals[5]);

            unordered_insert(&stack, 0, dup(vals[5]));
            test(size(stack) == 5);
            test(stack[0] == vals[5]);
            test(stack[1] == vals[1]);
            test(stack[2] == vals[2]);
            test(stack[3] == vals[5]);
            test(stack[4] == vals[0]);
            
            unordered_insert(&stack, 5, dup(vals[3]));
            test(size(stack) == 6);
            test(stack[3] == vals[5]);
            test(stack[4] == vals[0]);
            test(stack[5] == vals[3]);
        }

        {
            Stack empty;
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

        std::uniform_int_distribution<unsigned> op_distribution(0, 9);
        std::uniform_int_distribution<unsigned> index_distribution(0);
        std::uniform_int_distribution<unsigned> val_distribution(0);

        isize max_size = 1000;
        const auto test_batch = [&](isize block_size, isize k){
            i64 before = trackers_alive();

            {
                Array<Track, 30> to_insert = {
                    1, 2, 3, 4, 5, 
                    6, 7, 8, 9, 10,

                    1, 2, 3, 4, 5, 
                    6, 7, 8, 9, 10,

                    1, 2, 3, 4, 5, 
                    6, 7, 8, 9, 10,
                };

                Stack<Track> stack;
                for(isize i = 0; i < block_size; i++)
                {
                    isize op = (isize) op_distribution(gen);
                    isize index = (isize) index_distribution(gen);
                    isize size = jot::size(stack);
                    isize size_incr = size + 1;
                    isize val = val_distribution(gen);

                    switch(op)
                    {
                        case OP_PUSH1: 
                        case OP_PUSH2: 
                        case OP_PUSH3:
                        {
                            push(&stack, Track{val});
                            break;
                        }

                        case OP_POP: 
                        {
                            if(size != 0)
                                pop(&stack); 
                            break;
                        }

                        case OP_RESERVE:
                        {
                            reserve(&stack, index % max_size);
                            break;
                        }
                       
                        case OP_INSERT:
                        {
                            insert(&stack, index % size_incr, Track{val});
                            break;
                        }

                        case OP_REMOVE:
                        {
                            if(size != 0)
                                remove(&stack, index % size);
                            break;
                        }

                        case OP_INSERT_UNORDERED:
                        {
                            unordered_insert(&stack, index % size_incr, Track{val});
                            break;
                        }

                        case OP_REMOVE_UNORDERED:
                        {
                            if(size != 0)
                                unordered_remove(&stack, index % size);
                            break;
                        }
                        
                        //Doesnt exist anymore
                        case OP_SPLICE:
                        {
                            break;
                        }

                        default: break;
                    }

                    test(is_invariant(stack));
                }
                
                if(print) println("  i: {}\t batch: {}\t final_size: {}", k, block_size, size(stack));
            }
            
            i64 after = trackers_alive();
            test(before == after);
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
    void test_stack(Array<T, 6> vals)
    {
        isize mem_before = default_allocator()->bytes_allocated();

        test_push_pop<T>(dup(vals));
        test_copy<T>(dup(vals));
        test_resize<T>(dup(vals));
        test_reserve<T>(dup(vals));
        test_insert_remove<T>(dup(vals));
        
        isize mem_after = default_allocator()->bytes_allocated();
        test(mem_before == mem_after);
    }
    
    static
    void test_stack(u32 flags)
    {
        bool print = !(flags & Test_Flags::SILENT);

        Array<i32, 6>           arr1 = {10, 20, 30, 40, 50, 60};
        Array<char, 6>          arr2 = {'a', 'b', 'c', 'd', 'e', 'f'};
        Array<Test_String, 6>   arr3 = {"a", "b", "c", "d", "e", "some longer string..."};
        Array<Tracker<i32>, 6>  arr4 = {10, 20, 30, 40, 50, 60};

        if(print) println("\ntest_stack()");
        if(print) println("  type: i32");
        test_stack(arr1);
        
        if(print) println("  type: char");
        test_stack(arr2);
        
        if(print) println("  type: Test_String");
        test_stack(arr3);

        if(print) println("  type: Tracker<i32>");
        test_stack(arr4);

        if(flags & Test_Flags::STRESS)
            test_stress(print);
    }
}
