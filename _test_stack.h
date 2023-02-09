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

            test(data(stack) == 0);
            test(size(stack) == 0);
            test(capacity(stack) == 0);

            force(push(&stack, dup(vals[0])));
            test(size(stack) == 1);
            force(push(&stack, dup(vals[1])));

            test(size(stack) == 2);
            test(capacity(stack) >= 2);

            force(pop(&stack) == vals[1]);
            force(pop(&stack) == vals[0]);

            test(size(stack) == 0);
            test(capacity(stack) >= 2);

            force(push(&stack, dup(vals[2])));
            force(push(&stack, dup(vals[1])));
            force(push(&stack, dup(vals[0])));

            test(size(stack) == 3);
            test(capacity(stack) >= 3);

            test(stack[0] == vals[2]);
            test(stack[1] == vals[1]);
            test(stack[2] == vals[0]);
            
            force(pop(&stack) == vals[0]);
            force(pop(&stack) == vals[1]);
            force(pop(&stack) == vals[2]);

            test(size(stack) == 0);
        }
        
        {
            Stack<T> stack;
            force(push_multiple(&stack, dup(vals)));
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
            
            force(push_multiple(&stack, dup(vals)));
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
        force(before == after);
    }

    template<typename T>
    void test_copy(Array<T, 6> vals)
    {
        using Stack = Stack<T>;
        i64 before = trackers_alive();
        {
            Stack stack;
            force(push(&stack, dup(vals[0])));
            force(push(&stack, dup(vals[1])));
            force(push(&stack, dup(vals[2])));
            force(push(&stack, dup(vals[2])));

            Stack copied;
            force(copy(&copied, stack));
            force(size(copied) == 4);
            force(capacity(copied) >= 4);

            test(copied[0] == vals[0]);
            test(copied[3] == vals[2]);

            test(stack[1] == vals[1]);
            test(stack[3] == vals[2]);

            //not linked
            force(push(&stack, dup(vals[1])));
            test(size(stack) == 5);
            test(size(copied) == 4);

            force(copy(&copied, stack));
            test(size(copied) == 5);

            test(copied[0] == vals[0]);
            test(copied[4] == vals[1]);

            test(stack[0] == vals[0]);
            test(stack[4] == vals[1]);

            //from zero filling up
            Stack copied2;
            force(copy(&copied2, stack));
            test(copied2[0] == vals[0]);
            test(copied2[3] == vals[2]);
            test(copied2[4] == vals[1]);

            
            Stack copied3;
            force(copy(&copied3, stack));
            force(push(&copied3, dup(vals[0])));
            force(push(&copied3, dup(vals[1])));
            force(push(&copied3, dup(vals[0])));
            force(push(&copied3, dup(vals[1])));

            test(size(copied3) == 9);

            //copieding to less elems with bigger capacity
            force(copy(&copied3, stack));
            test(size(copied3) == 5);

            //copieding to more elems with bigger capacity
            pop(&copied3);
            pop(&copied3);
            pop(&copied3);

            
            force(copy(&copied3, stack));
            force(size(copied3) == 5);
        }

        {
            //copying to zero elems
            Stack empty;
            Stack stack;
            force(push(&stack, dup(vals[0])));
            force(push(&stack, dup(vals[1])));
            force(push(&stack, dup(vals[2])));
            force(push(&stack, dup(vals[2])));
            test(size(stack) == 4);
            
            force(copy(&stack, empty));
            test(size(stack) == 0);
        }

        {
            //copy constructing empty
            Stack empty;
            Stack stack;
            
            force(copy(&stack, empty));
            test(size(stack) == 0);
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

            test(capacity(empty) >= 5);
            test(size(empty) == 0);

            force(reserve(&empty, 13));
            test(capacity(empty) >= 13);
            test(size(empty) == 0);
        }
        {
            Stack stack;
            force(push(&stack, dup(vals[0])));
            force(push(&stack, dup(vals[0])));
            force(push(&stack, dup(vals[0])));
            test(capacity(stack) >= 3);
            test(size(stack) == 3);

            force(reserve(&stack, 7));
            test(capacity(stack) >= 7);
            test(size(stack) == 3);

            pop(&stack);
            force(reserve(&stack, 2));
            test(capacity(stack) >= 7);
            test(size(stack) == 2);
        }
        i64 after = trackers_alive();
        force(before == after);
    }

    template<typename T>
    void test_resize(Array<T, 6> vals)
    {
        using Stack = Stack<T>;
        i64 before = trackers_alive();

        {
            Stack empty;
            force(resize(&empty, 5, vals[0]));

            force(size(empty) == 5);
            test(empty[0] == vals[0]);
            test(empty[2] == vals[0]);
            test(empty[4] == vals[0]);

            force(resize(&empty, 16));
            test(empty[5] == T());
            test(empty[9] == T());
            test(empty[11] == T());
            test(empty[15] == T());
        }

        {
            Stack stack;
            force(resize(&stack, 7, vals[0]));

            force(capacity(stack) >= 7);
            force(size(stack) == 7);
            test(stack[0] == vals[0]);
            test(stack[4] == vals[0]);
            test(stack[6] == vals[0]);

            //growing
            force(resize(&stack, 11, vals[1]));
            force(resize(&stack, 12, vals[2]));
            test(capacity(stack) >= 12);
            test(size(stack) == 12);
            test(stack[7] == vals[1]);
            test(stack[9] == vals[1]);
            test(stack[10] == vals[1]);
            test(stack[11] == vals[2]);

            //shrinking
            force(resize(&stack, 11, vals[1]));
            test(capacity(stack) >= 12);
            test(size(stack) == 11);
            test(stack[0] == vals[0]);
            test(stack[6] == vals[0]);
            test(stack[10] == vals[1]);

            force(push(&stack, dup(vals[2])));

            force(resize(&stack, 7, vals[1]));
            test(capacity(stack) >= 12);
            test(size(stack) == 7);
            test(stack[1] == vals[0]);
            test(stack[3] == vals[0]);
            test(stack[6] == vals[0]);
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

            force(insert(&stack, 2, dup(vals[1])));
            test(capacity(stack) >= 6);
            test(size(stack) == 6);

 
            test(stack[0] == vals[0]);
            test(stack[1] == vals[0]);
            test(stack[2] == vals[1]);
            test(stack[3] == vals[0]);
            test(stack[5] == vals[0]);

            force(insert(&stack, 2, dup(vals[2])));
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
            
            force(insert(&stack, size(stack), dup(vals[3])));
            force(insert(&stack, size(stack), dup(vals[4])));
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
            force(push_multiple(&stack, dup(vals)));
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

            force(unordered_insert(&stack, 0, dup(vals[5])));
            test(size(stack) == 5);
            test(stack[0] == vals[5]);
            test(stack[1] == vals[1]);
            test(stack[2] == vals[2]);
            test(stack[3] == vals[5]);
            test(stack[4] == vals[0]);
            
            force(unordered_insert(&stack, 5, dup(vals[3])));
            test(size(stack) == 6);
            test(stack[3] == vals[5]);
            test(stack[4] == vals[0]);
            test(stack[5] == vals[3]);
        }

        {
            Stack empty;
            force(insert(&empty, 0, dup(vals[0])));
            test(capacity(empty) >= 1);
            test(size(empty) == 1);
            test(last(empty) == vals[0]);

            force(insert(&empty, 1, dup(vals[1])));
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
        force(before == after);
    }
    
    template<typename T>
    void test_splice(Array<T, 6> vals)
    {
        using Stack = Stack<T>;
        i64 before = trackers_alive();

        {
            Array trimmed = {dup(vals[0]), dup(vals[1]), dup(vals[2])};
            Stack stack;
            force(splice(&stack, 0, 0, dup(trimmed)));

            test(size(stack) == 3);
            test(stack[0] == vals[0]);
            test(stack[1] == vals[1]);
            test(stack[2] == vals[2]);

            Slice<T> empty;
            force(splice(&stack, 3, 0, dup(empty)));
            test(size(stack) == 3);
            test(stack[0] == vals[0]);
            test(stack[1] == vals[1]);
            test(stack[2] == vals[2]);

            force(splice(&stack, 3, 0, dup(trimmed)));
            test(size(stack) == 6);
            test(stack[0] == vals[0]);
            test(stack[1] == vals[1]);
            test(stack[2] == vals[2]);
            test(stack[3] == vals[0]);
            test(stack[4] == vals[1]);
            test(stack[5] == vals[2]);

            force(splice(&stack, 3, 2, dup(empty)));
            test(size(stack) == 4);
            test(stack[0] == vals[0]);
            test(stack[1] == vals[1]);
            test(stack[2] == vals[2]);
            test(stack[3] == vals[2]);

            force(splice(&stack, 2, 2, dup(trimmed)));
            test(size(stack) == 5);
            test(stack[0] == vals[0]);
            test(stack[1] == vals[1]);
            test(stack[2] == vals[0]);
            test(stack[3] == vals[1]);
            test(stack[4] == vals[2]);

            force(splice(&stack, 0, 2, dup(trimmed)));
            test(size(stack) == 6);
            test(stack[3] == vals[0]);
            test(stack[4] == vals[1]);
            test(stack[5] == vals[2]);
        }
        
        {
            Array trimmed = {dup(vals[0]), dup(vals[1]), dup(vals[2])};
            Slice single = trim(slice(&trimmed), 1);
            Stack stack;
            
            force(splice(&stack, 0, 0, dup(trimmed)));
            test(size(stack) == 3);
            force(splice(&stack, 1, 0, dup(single)));
            test(size(stack) == 4);
            test(stack[0] == trimmed[0]);
            test(stack[1] == trimmed[0]);
            test(stack[2] == trimmed[1]);
            test(stack[3] == trimmed[2]);
            
            force(splice(&stack, 3, 0, dup(trimmed)));
            test(size(stack) == 7);
            test(stack[0] == trimmed[0]);
            test(stack[1] == trimmed[0]);
            test(stack[2] == trimmed[1]);
            test(stack[3] == trimmed[0]);
            test(stack[4] == trimmed[1]);
            test(stack[5] == trimmed[2]);
            test(stack[6] == trimmed[2]);
            
        }
        i64 after = trackers_alive();
        force(before == after);
    }
    
    template<typename T>
    void test_stack(Array<T, 6> vals)
    {
        test_push_pop<T>(dup(vals));
        test_copy<T>(dup(vals));
        test_resize<T>(dup(vals));
        test_reserve<T>(dup(vals));
        test_insert_remove<T>(dup(vals));
        test_splice<T>(dup(vals));
    }

    void test_stack()
    {
        test_stack(Array{10, 20, 30, 40, 50, 60});
        test_stack(Array{53, -121, 110, 45464, -1313131, 1});
        test_stack(Array{'a', 'b', 'c', 'd', 'e', 'f'});
        test_stack(Array{Tracker{1}, Tracker{2}, Tracker{3}, Tracker{6346}, Tracker{-422}, Tracker{12}});
        test_stack(Array{No_Copy{1}, No_Copy{2}, No_Copy{3}, No_Copy{6346}, No_Copy{-422}, No_Copy{12}});
    }
}
