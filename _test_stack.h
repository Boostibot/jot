#pragma once

#include "stack.h"
#include "array.h"
#include "_test.h"

namespace jot::tests::stack
{
    template<typename T>
    void test_push_pop(Array<T, 6> vals)
    {
        using Size = size_t;

        {
            Stack_<T> stack;

            force(stack.data == 0);
            force(stack.size == 0);
            force(stack.capacity == 0);

            force(*push(&stack, vals[0]) == vals[0]);

            force(stack.size == 1);

            force(*push(&stack, vals[1]) == vals[1]);

            force(stack.size == 2);
            force(stack.capacity == 2);

            force(pop(&stack) == vals[1]);
            force(pop(&stack) == vals[0]);

            force(stack.size == 0);
            force(stack.capacity == 2);

            force(*push(&stack, vals[2]) == vals[2]);
            force(*push(&stack, vals[1]) == vals[1]);
            force(*push(&stack, vals[0]) == vals[0]);

            force(stack.size == 3);
            force(stack.capacity == 4);

            force(stack[0] == vals[2]);
            force(stack[1] == vals[1]);
            force(stack[2] == vals[0]);
        }
    }

    template<typename T>
    void test_copy(Array<T, 6> vals)
    {
        using Stack = Stack<T>;
        {
            Stack stack;
            push(&stack, vals[0]);
            push(&stack, vals[1]);
            push(&stack, vals[2]);
            push(&stack, vals[2]);

            Stack copy = stack;
            force(copy.size == 4);
            force(copy.capacity >= 4);

            force(copy[0] == vals[0]);
            force(copy[1] == vals[1]);
            force(copy[2] == vals[2]);
            force(copy[3] == vals[2]);

            force(stack[0] == vals[0]);
            force(stack[1] == vals[1]);
            force(stack[2] == vals[2]);
            force(stack[3] == vals[2]);

            //not linked
            push(&stack, vals[1]);
            force(stack.size == 5);
            force(copy.size == 4);

            copy = stack;
            force(copy.size == 5);

            force(copy[0] == vals[0]);
            force(copy[4] == vals[1]);

            force(stack[0] == vals[0]);
            force(stack[4] == vals[1]);

            //from zero filling up
            Stack copy2;
            copy2 = stack;
            force(copy2[0] == vals[0]);
            force(copy2[3] == vals[2]);
            force(copy2[4] == vals[1]);

            
            Stack copy3 = stack;
            push(&copy3, vals[0]);
            push(&copy3, vals[1]);
            push(&copy3, vals[0]);
            push(&copy3, vals[1]);

            //force(copy3.capacity > stack.capacity);
            force(copy3.size == 9);

            //copying to less elems with bigger capacity
            copy3 = stack;

            force(copy3.size == 5);


            //copying to more elems with bigger capacity
            pop(&copy3);
            pop(&copy3);
            pop(&copy3);

            //force(copy3.capacity > stack.capacity);
            
            copy3 = stack;

            force(copy3.size == 5);
        }

        {
            //copying to zero elems
            Stack empty;
            Stack stack;
            push(&stack, vals[0]);
            push(&stack, vals[1]);
            push(&stack, vals[2]);
            push(&stack, vals[2]);
            force(stack.size == 4);

            stack = empty;
            force(stack.size == 0);
        }

        {
            //copy constructing empty
            Stack empty;
            Stack stack(empty);

            force(stack.size == 0);
        }
    }

    template<typename T>
    void test_swap(Array<T, 6> vals)
    {
        using Stack = Stack<T>;

        {
            Stack stack1;
            push(&stack1, vals[0]);
            push(&stack1, vals[1]);
            push(&stack1, vals[2]);
            push(&stack1, vals[3]);
            
            {
                Stack stack2;
                push(&stack2, vals[3]);
                push(&stack2, vals[4]);
                push(&stack2, vals[5]);

                std::swap(stack1, stack2);

                force(stack2.size == 4);
                force(stack2.capacity >= 4);
                force(stack2[0] == vals[0]);
                force(stack2[1] == vals[1]);
                force(stack2[2] == vals[2]);
                force(stack2[3] == vals[3]);
            }

            force(stack1.size == 3);
            force(stack1.capacity >= 3);
            force(stack1[0] == vals[3]);
            force(stack1[1] == vals[4]);
            force(stack1[1] == vals[4]);
        }

        {
            Stack stack1;
            push(&stack1, vals[0]);
            push(&stack1, vals[1]);
            push(&stack1, vals[2]);
            push(&stack1, vals[3]);

            {
                Stack stack2;
                std::swap(stack1, stack2);

                force(stack2.size == 4);
                force(stack2.capacity >= 4);

                force(stack2[0] == vals[0]);
                force(stack2[1] == vals[1]);
                force(stack2[2] == vals[2]);
                force(stack2[3] == vals[3]);
            }
            force(stack1.size == 0);
        }

        {
            Stack stack1;
            Stack stack2;
            std::swap(stack1, stack2);

            force(stack1.data == nullptr);
            force(stack1.size == 0);
            force(stack1.capacity >= 0);
            force(stack2.data == nullptr);
            force(stack2.size == 0);
            force(stack2.capacity >= 0);
        }
    }

    template<typename T>
    void test_move(Array<T, 6> vals)
    {
        using Stack = Stack<T>;

        {
            Stack stack1;
            push(&stack1, vals[0]);
            push(&stack1, vals[1]);
            push(&stack1, vals[2]);
            push(&stack1, vals[3]);

            Stack stack2(std::move(stack1));

            force(stack2.size == 4);
            force(stack2.capacity >= 4);
            force(stack2[0] == vals[0]);
            force(stack2[1] == vals[1]);
            force(stack2[2] == vals[2]);
            force(stack2[3] == vals[3]);
        }

        {
            Stack stack1;
            Stack stack2(std::move(stack1));

            force(stack1.data == nullptr);
            force(stack1.size == 0);
            force(stack1.capacity >= 0);
            force(stack2.data == nullptr);
            force(stack2.size == 0);
            force(stack2.capacity >= 0);
        }

        {
            Stack stack1;
            push(&stack1, vals[0]);
            push(&stack1, vals[1]);
            push(&stack1, vals[2]);
            push(&stack1, vals[3]);

            {
                Stack stack2;
                push(&stack2, vals[3]);
                push(&stack2, vals[4]);
                push(&stack2, vals[5]);

                stack1 = std::move(stack2);

                force(stack2.size == 4);
                force(stack2.capacity >= 4);
                force(stack2[0] == vals[0]);
                force(stack2[1] == vals[1]);
                force(stack2[2] == vals[2]);
                force(stack2[3] == vals[3]);
            }

            force(stack1.size == 3);
            force(stack1.capacity >= 3);
            force(stack1[0] == vals[3]);
            force(stack1[1] == vals[4]);
            force(stack1[1] == vals[4]);
        }

        {
            Stack stack1;
            push(&stack1, vals[0]);
            push(&stack1, vals[1]);
            push(&stack1, vals[2]);
            push(&stack1, vals[3]);

            {
                Stack stack2;
                stack1 = std::move(stack2);

                force(stack2.size == 4);
                force(stack2.capacity >= 4);

                force(stack2[0] == vals[0]);
                force(stack2[1] == vals[1]);
                force(stack2[2] == vals[2]);
                force(stack2[3] == vals[3]);
            }
            force(stack1.size == 0);
        }

        {
            Stack stack1;
            Stack stack2;
            stack1 = std::move(stack2);

            force(stack1.data == nullptr);
            force(stack1.size == 0);
            force(stack1.capacity >= 0);
            force(stack2.data == nullptr);
            force(stack2.size == 0);
            force(stack2.capacity >= 0);
        }
    }

    template<typename T>
    void test_reserve(Array<T, 6> vals)
    {
        using Stack = Stack<T>;

        {
            Stack empty;
            reserve(&empty, 5);

            force(empty.capacity >= 5);
            force(empty.size == 0);

            reserve(&empty, 13);

            force(empty.capacity >= 13);
            force(empty.size == 0);
        }

        {
            Stack empty;
            reserve(&empty, 7);

            force(empty.capacity >= 7);
            force(empty.size == 0);


            reserve(&empty, 2);
            force(empty.capacity >= 7);
            force(empty.size == 0);
        }

        {
            Stack stack;
            push(&stack, vals[0]);
            push(&stack, vals[0]);
            push(&stack, vals[0]);
            force(stack.capacity >= 3);
            force(stack.size == 3);

            reserve(&stack, 7);
            force(stack.capacity >= 7);
            force(stack.size == 3);

            pop(&stack);

            reserve(&stack, 2);
            force(stack.capacity >= 7);
            force(stack.size == 2);
        }
    }

    template<typename T>
    void test_resize(Array<T, 6> vals)
    {
        using Stack = Stack<T>;

        //resizing empty stack
        {
            Stack empty;
            resize(&empty, 5, vals[0]);

            force(empty.size == 5);
            force(empty[0] == vals[0]);
            force(empty[1] == vals[0]);
            force(empty[2] == vals[0]);
            force(empty[3] == vals[0]);
            force(empty[4] == vals[0]);
        }

        {
            Stack stack;
            resize(&stack, 7, vals[0]);

            force(stack.capacity == 12);
            force(stack.size == 7);
            force(stack[0] == vals[0]);
            force(stack[2] == vals[0]);
            force(stack[4] == vals[0]);
            force(stack[6] == vals[0]);

            //growing
            resize(&stack, 11, vals[1]);
            resize(&stack, 12, vals[2]);
            force(stack.capacity == 12);
            force(stack.size == 12);
            force(stack[7] == vals[1]);
            force(stack[9] == vals[1]);
            force(stack[10] == vals[1]);
            force(stack[11] == vals[2]);

            //shrinking
            resize(&stack, 11, vals[1]);
            force(stack.capacity == 12);
            force(stack.size == 11);
            force(stack[0] == vals[0]);
            force(stack[6] == vals[0]);
            force(stack[10] == vals[1]);

            push(&stack, vals[2]);

            resize(&stack, 7, vals[1]);
            force(stack.capacity == 12);
            force(stack.size == 7);
            force(stack[1] == vals[0]);
            force(stack[3] == vals[0]);
            force(stack[5] == vals[0]);
            force(stack[6] == vals[0]);
        }
    }

    template<typename T>
    void test_insert_remove(Array<T, 6> vals)
    {
        using Stack = Stack<T>;

        {
            Stack stack;
            resize(&stack, 5, vals[0]);

            insert(&stack, 2, vals[1]);
            force(stack.capacity >= 6);
            force(stack.size == 6);
            force(stack[0] == vals[0]);
            force(stack[1] == vals[0]);
            force(stack[2] == vals[1]);
            force(stack[3] == vals[0]);
            force(stack[5] == vals[0]);

            insert(&stack, 2, vals[2]);
            force(stack.capacity >= 7);
            force(stack.size == 7);
            force(stack[0] == vals[0]);
            force(stack[1] == vals[0]);
            force(stack[2] == vals[2]);
            force(stack[3] == vals[1]);
            force(stack[4] == vals[0]);
            force(stack[6] == vals[0]);

            remove(&stack, 2);
            force(stack.capacity >= 7);
            force(stack.size == 6);
            force(stack[0] == vals[0]);
            force(stack[1] == vals[0]);
            force(stack[2] == vals[1]);
            force(stack[3] == vals[0]);
            force(stack[5] == vals[0]);

            remove(&stack, 0);
            force(stack.capacity >= 7);
            force(stack.size == 5);
            force(stack[0] == vals[0]);
            force(stack[1] == vals[1]);
            force(stack[2] == vals[0]);
            force(stack[4] == vals[0]);
        }

        {
            Stack empty;
            insert(&empty, 0, vals[0]);
            force(empty.capacity >= 1);
            force(empty.size == 1);
            force(back(empty) == vals[0]);

            insert(&empty, 1, vals[1]);
            force(empty.capacity >= 2);
            force(empty.size == 2);
            force(back(empty) == vals[1]);

            remove(&empty, 1);
            force(empty.capacity >= 2);
            force(empty.size == 1);
            force(back(empty) == vals[0]);

            remove(&empty, 0);
            force(empty.capacity >= 2);
            force(empty.size == 0);
        }
    }
    
    template<typename T>
    void test_splice(Array<T, 6> vals)
    {
        using Stack = Stack<T>;

        Array expaned = {vals[0], vals[1], vals[2], vals[1], vals[0], vals[1], vals[2]};
        Slice single = trim(slice(expaned), 1);

        {
            Stack stack;
            splice(&stack, 0, 0, move(vals));

            force(stack.size == 3);
            force(stack[0] == vals[0]);
            force(stack[1] == vals[1]);
            force(stack[2] == vals[2]);

            Slice<T> empty;
            splice(&stack, 3, 0, move(empty));
            force(stack.size == 3);
            force(stack[0] == vals[0]);
            force(stack[1] == vals[1]);
            force(stack[2] == vals[2]);

            splice(&stack, 3, 0, move(vals));
            force(stack.size == 6);
            force(stack[0] == vals[0]);
            force(stack[1] == vals[1]);
            force(stack[2] == vals[2]);
            force(stack[3] == vals[0]);
            force(stack[4] == vals[1]);
            force(stack[5] == vals[2]);

            splice(&stack, 3, 2, move(empty));
            force(stack.size == 4);
            force(stack[0] == vals[0]);
            force(stack[1] == vals[1]);
            force(stack[2] == vals[2]);
            force(stack[3] == vals[2]);

            splice(&stack, 2, 2, move(vals));
            force(stack.size == 5);
            force(stack[0] == vals[0]);
            force(stack[1] == vals[1]);
            force(stack[2] == vals[0]);
            force(stack[3] == vals[1]);
            force(stack[4] == vals[2]);

            splice(&stack, 2, 3);
            force(stack.size == 2);

            splice(&stack, 0, 2, move(expaned));
            force(stack.size == 7);
            force(stack[4] == vals[0]);
            force(stack[5] == vals[1]);
            force(stack[6] == vals[2]);
        }


        {
            
            Stack stack;
            resize(&stack, 3, [&](size_t i){return vals[i];});
            force(stack.size == 3);
            force(stack[0] == vals[0]);
            force(stack[1] == vals[1]);
            force(stack[2] == vals[2]);
            
            splice(&stack, 1, 0, move(single));
            force(stack.size == 4);
            force(stack[0] == vals[0]);
            force(stack[1] == vals[0]);
            force(stack[2] == vals[1]);
            force(stack[3] == vals[2]);
            
            splice(&stack, 3, 0, move(vals));
            force(stack.size == 7);
            force(stack[0] == vals[0]);
            force(stack[1] == vals[0]);
            force(stack[2] == vals[1]);
            force(stack[3] == vals[0]);
            force(stack[4] == vals[1]);
            force(stack[5] == vals[2]);
            force(stack[6] == vals[2]);
            
        }
    }
    
    template<typename T, typename Fn>
    void test_stack(Fn make_values)
    {
        test_push_pop<T>(make_values());
        test_swap<T>(make_values());
        test_copy<T>(make_values());
        test_move<T>(make_values());
        test_reserve_resize<T>(make_values());
        test_insert_remove<T>(make_values());
        test_splice<T>(make_values());
    }
    
    
    template<typename... Ts>
    auto make_arr(Ts... args) 
    { 
        return Array<Tracker<T>, sizeof...(args)>{make(args)...}; 
    }

    void test_stack()
    {
        Tracker_Stats stats;

        const auto make_values_1 = [&]{
            const auto arr1 = Array(10, 20, 30, 40, 50, 60);
            return arr1;
        };
        
        const auto make_values_2 = [&]{
            const auto arr2 = Array(1.0, 2.0, 3.0, 4.0, 5.0, 6.0);
            return arr2;
        };
        
        const auto make_values_3 = []{
            const auto arr3 = Array{Not_Copyable{1}, Not_Copyable{2}, Not_Copyable{3}, Not_Copyable{6346}, Not_Copyable{-422}, Not_Copyable{12}};
            return arr3;
        };
        
        test_stack<i32>(make_values_1);
        test_stack<f64>(make_values_2);
        test_stack<Not_Copyable>(make_values_3);

        force(are_matching(stats));
    }
}
