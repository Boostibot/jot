#pragma once

#include "array.h"
#include "stack.h"
#include "_test.h"

namespace jot::tests::stack
{
    template<typename T>
    void test_push_pop(Array<T, 6> vals)
    {
        i64 before = trackers_alive();
        {
            Stack<T> stack;

            force(data(stack) == 0);
            force(size(stack) == 0);
            force(capacity(stack) == 0);

            force(push(&stack, vals[0]));
            force(size(stack) == 1);
            force(push(&stack, vals[1]));

            force(size(stack) == 2);
            force(capacity(stack) == 2);

            force(pop(&stack) == vals[1]);
            force(pop(&stack) == vals[0]);

            force(size(stack) == 0);
            force(capacity(stack) == 2);

            force(push(&stack, vals[2]));
            force(push(&stack, vals[1]));
            force(push(&stack, vals[0]));

            force(size(stack) == 3);
            force(capacity(stack) == 4);

            force(stack[0] == vals[2]);
            force(stack[1] == vals[1]);
            force(stack[2] == vals[0]);
        }
        i64 after = trackers_alive();
        force(before == after);
    }

    template<typename T>
    void test_copy(Array<T, 6> vals)
    {
        using Stack = Stack<T>;
        i64 before = trackers_alive();
        {
            Stack stack;
            force(push(&stack, vals[0]));
            force(push(&stack, vals[1]));
            force(push(&stack, vals[2]));
            force(push(&stack, vals[2]));

            Stack copy;
            force(assign(&copy, stack));
            force(size(copy) == 4);
            force(capacity(copy) >= 4);

            force(copy[0] == vals[0]);
            force(copy[3] == vals[2]);

            force(stack[1] == vals[1]);
            force(stack[3] == vals[2]);

            //not linked
            force(push(&stack, vals[1]));
            force(size(stack) == 5);
            force(size(copy) == 4);

            force(assign(&copy, stack));
            force(size(copy) == 5);

            force(copy[0] == vals[0]);
            force(copy[4] == vals[1]);

            force(stack[0] == vals[0]);
            force(stack[4] == vals[1]);

            //from zero filling up
            Stack copy2;
            force(assign(&copy2, stack));
            force(copy2[0] == vals[0]);
            force(copy2[3] == vals[2]);
            force(copy2[4] == vals[1]);

            
            Stack copy3;
            force(assign(&copy3, stack));
            force(push(&copy3, vals[0]));
            force(push(&copy3, vals[1]));
            force(push(&copy3, vals[0]));
            force(push(&copy3, vals[1]));

            force(size(copy3) == 9);

            //copying to less elems with bigger capacity
            force(assign(&copy3, stack));
            force(size(copy3) == 5);

            //copying to more elems with bigger capacity
            pop(&copy3);
            pop(&copy3);
            pop(&copy3);

            
            force(assign(&copy3, stack));
            force(size(copy3) == 5);
        }

        {
            //copying to zero elems
            Stack empty;
            Stack stack;
            force(push(&stack, vals[0]));
            force(push(&stack, vals[1]));
            force(push(&stack, vals[2]));
            force(push(&stack, vals[2]));
            force(size(stack) == 4);
            
            force(assign(&stack, empty));
            force(size(stack) == 0);
        }

        {
            //copy constructing empty
            Stack empty;
            Stack stack;
            
            force(assign(&stack, empty));
            force(size(stack) == 0);
        }
        i64 after = trackers_alive();
        force(before == after);
    }

    template<typename T>
    void test_reserve(Array<T, 6> vals)
    {
        using Stack = Stack<T>;
        i64 before = trackers_alive();

        {
            Stack empty;
            force(reserve(&empty, 5));

            force(capacity(empty) >= 5);
            force(size(empty) == 0);

            force(reserve(&empty, 13));
            force(capacity(empty) >= 13);
            force(size(empty) == 0);
        }
        {
            Stack stack;
            force(push(&stack, vals[0]));
            force(push(&stack, vals[0]));
            force(push(&stack, vals[0]));
            force(capacity(stack) >= 3);
            force(size(stack) == 3);

            force(reserve(&stack, 7));
            force(capacity(stack) >= 7);
            force(size(stack) == 3);

            pop(&stack);
            force(reserve(&stack, 2));
            force(capacity(stack) >= 7);
            force(size(stack) == 2);
        }
        i64 after = trackers_alive();
        force(before == after);
    }

    template<typename T>
    void test_resize(Array<T, 6> vals)
    {
        using Stack = Stack<T>;
        i64 before = trackers_alive();

        //resizing empty stack
        {
            Stack empty;
            force(resize(&empty, 5, vals[0]));

            force(size(empty) == 5);
            force(empty[0] == vals[0]);
            force(empty[2] == vals[0]);
            force(empty[4] == vals[0]);
        }

        {
            Stack stack;
            force(resize(&stack, 7, vals[0]));

            force(capacity(stack) == 12);
            force(size(stack) == 7);
            force(stack[0] == vals[0]);
            force(stack[4] == vals[0]);
            force(stack[6] == vals[0]);

            //growing
            force(resize(&stack, 11, vals[1]));
            force(resize(&stack, 12, vals[2]));
            force(capacity(stack) == 12);
            force(size(stack) == 12);
            force(stack[7] == vals[1]);
            force(stack[9] == vals[1]);
            force(stack[10] == vals[1]);
            force(stack[11] == vals[2]);

            //shrinking
            force(resize(&stack, 11, vals[1]));
            force(capacity(stack) == 12);
            force(size(stack) == 11);
            force(stack[0] == vals[0]);
            force(stack[6] == vals[0]);
            force(stack[10] == vals[1]);

            force(push(&stack, vals[2]));

            force(resize(&stack, 7, vals[1]));
            force(capacity(stack) == 12);
            force(size(stack) == 7);
            force(stack[1] == vals[0]);
            force(stack[3] == vals[0]);
            force(stack[6] == vals[0]);
        }
        i64 after = trackers_alive();
        force(before == after);
    }

    template<typename T>
    void test_insert_remove(Array<T, 6> vals)
    {
        using Stack = Stack<T>;
        i64 before = trackers_alive();

        {
            Stack stack;
            force(resize(&stack, 5, vals[0]));

            force(insert(&stack, 2, vals[1]));
            force(capacity(stack) >= 6);
            force(size(stack) == 6);
            force(stack[0] == vals[0]);
            force(stack[1] == vals[0]);
            force(stack[2] == vals[1]);
            force(stack[3] == vals[0]);
            force(stack[5] == vals[0]);

            force(insert(&stack, 2, vals[2]));
            force(capacity(stack) >= 7);
            force(size(stack) == 7);
            force(stack[0] == vals[0]);
            force(stack[1] == vals[0]);
            force(stack[2] == vals[2]);
            force(stack[3] == vals[1]);
            force(stack[4] == vals[0]);
            force(stack[6] == vals[0]);

            //@TODO check return val
            remove(&stack, 2);
            force(capacity(stack) >= 7);
            force(size(stack) == 6);
            force(stack[0] == vals[0]);
            force(stack[1] == vals[0]);
            force(stack[2] == vals[1]);
            force(stack[3] == vals[0]);
            force(stack[5] == vals[0]);

            remove(&stack, 0);
            force(capacity(stack) >= 7);
            force(size(stack) == 5);
            force(stack[0] == vals[0]);
            force(stack[1] == vals[1]);
            force(stack[2] == vals[0]);
            force(stack[4] == vals[0]);
        }

        {
            Stack empty;
            force(insert(&empty, 0, vals[0]));
            force(capacity(empty) >= 1);
            force(size(empty) == 1);
            force(last(empty) == vals[0]);

            force(insert(&empty, 1, vals[1]));
            force(capacity(empty) >= 2);
            force(size(empty) == 2);
            force(last(empty) == vals[1]);

            remove(&empty, 1);
            force(capacity(empty) >= 2);
            force(size(empty) == 1);
            force(last(empty) == vals[0]);

            remove(&empty, 0);
            force(capacity(empty) >= 2);
            force(size(empty) == 0);
        }
        i64 after = trackers_alive();
        force(before == after);
    }
    
    template<typename T>
    void test_splice(Array<T, 6> vals)
    {
        using Stack = Stack<T>;
        i64 before = trackers_alive();

        Array expaned = {vals[0], vals[1], vals[2], vals[1], vals[0], vals[1], vals[2]};
        Slice single = trim(slice(expaned), 1);

        {
            Stack stack;
            force(splice(&stack, 0, 0, move(&vals)));

            force(size(stack) == 3);
            force(stack[0] == vals[0]);
            force(stack[1] == vals[1]);
            force(stack[2] == vals[2]);

            Slice<T> empty;
            force(splice(&stack, 3, 0, move(&empty)));
            force(size(stack) == 3);
            force(stack[0] == vals[0]);
            force(stack[1] == vals[1]);
            force(stack[2] == vals[2]);

            force(splice(&stack, 3, 0, move(&vals)));
            force(size(stack) == 6);
            force(stack[0] == vals[0]);
            force(stack[1] == vals[1]);
            force(stack[2] == vals[2]);
            force(stack[3] == vals[0]);
            force(stack[4] == vals[1]);
            force(stack[5] == vals[2]);

            force(splice(&stack, 3, 2, move(&empty)));
            force(size(stack) == 4);
            force(stack[0] == vals[0]);
            force(stack[1] == vals[1]);
            force(stack[2] == vals[2]);
            force(stack[3] == vals[2]);

            force(splice(&stack, 2, 2, move(&vals)));
            force(size(stack) == 5);
            force(stack[0] == vals[0]);
            force(stack[1] == vals[1]);
            force(stack[2] == vals[0]);
            force(stack[3] == vals[1]);
            force(stack[4] == vals[2]);

            //@TODO: Maybe broken
            //splice(&stack, 2, 3);
            //force(size(stack) == 2);

            force(splice(&stack, 0, 2, move(&expaned)));
            force(size(stack) == 7);
            force(stack[4] == vals[0]);
            force(stack[5] == vals[1]);
            force(stack[6] == vals[2]);
        }


        {
            Stack stack;
            force(resize(&stack, 3, vals[0]));
            force(size(stack) == 3);
            force(stack[0] == vals[0]);
            force(stack[1] == vals[0]);
            force(stack[2] == vals[0]);
            
            force(splice(&stack, 1, 0, move(&single)));
            force(size(stack) == 4);
            force(stack[0] == vals[0]);
            force(stack[1] == vals[0]);
            force(stack[2] == vals[1]);
            force(stack[3] == vals[2]);
            
            force(splice(&stack, 3, 0, move(&vals)));
            force(size(stack) == 7);
            force(stack[0] == vals[0]);
            force(stack[1] == vals[0]);
            force(stack[2] == vals[1]);
            force(stack[3] == vals[0]);
            force(stack[4] == vals[1]);
            force(stack[5] == vals[2]);
            force(stack[6] == vals[2]);
            
        }
        i64 after = trackers_alive();
        force(before == after);
    }
    
    template<typename T, typename Fn>
    void test_stack(Fn make_values)
    {
        test_push_pop<T>(make_values());
        test_copy<T>(make_values());
        test_resize<T>(make_values());
        test_reserve<T>(make_values());
        test_insert_remove<T>(make_values());
        test_splice<T>(make_values());
    }
    
    template<typename... Ts>
    auto make_arr(Ts... args) 
    { 
        return Array{make(args)...}; 
    }

    void test_stack()
    {
        const auto make_values_1 = [&]{
            const auto arr1 = Array{10, 20, 30, 40, 50, 60};
            return arr1;
        };
        
        const auto make_values_2 = [&]{
            const auto arr2 = Array{1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
            return arr2;
        };
        
        const auto make_values_3 = []{
            return Array{Not_Copyable{1}, Not_Copyable{2}, Not_Copyable{3}, Not_Copyable{6346}, Not_Copyable{-422}, Not_Copyable{12}};
            //return arr3;
        };
        
        test_stack<i32>(make_values_1);
        test_stack<f64>(make_values_2);
        //test_stack<Not_Copyable<int>>(make_values_3);
    }
}
