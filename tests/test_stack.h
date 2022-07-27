#pragma once

#include <exception>
#include <vector>
#include "tester_utils.h"

#include "../lib/jot/stack.h"
#include "../lib/jot/stack_settings.h"
#include "../lib/jot/defines.h"


namespace jot::tests
{
    
    struct Res_Stats
    {
        i64 own_constr = 0;
        i64 copy_constr = 0;
        i64 move_constr = 0;
        i64 destructed = 0;
        
        //just for info
        i64 copy_assign = 0;
        i64 move_assign = 0;
    };

    static thread_local Res_Stats res_watch = {};

    template<typename T>
    struct Test_Res
    {
        T val;
        Res_Stats* stats;

        constexpr Test_Res(T val, Res_Stats* stats) : val(val), stats(stats)          
            { stats->own_constr++; };
        constexpr Test_Res(Test_Res const& other) : val(other.val), stats(other.stats)  
            { stats->copy_constr++; };
        constexpr Test_Res(Test_Res && other) : val(std::move(other.val)), stats(other.stats)
            { stats->move_constr++; };

        constexpr Test_Res& operator =(Test_Res const& other) 
        { 
            this->val = other.val;
            this->stats = other.stats;
            stats->copy_assign++;
            return *this;
        }

        constexpr Test_Res& operator =(Test_Res && other) 
        { 
            this->val = std::move(other.val);
            this->stats = other.stats;
            stats->move_assign++;
            return *this;
        }

        constexpr ~Test_Res() { stats->destructed++; }

        constexpr operator T() const {return val;};
    };


    template<typename T>
    struct Res_Watch
    {
        Res_Stats stats;

        proc make(T val) {return Test_Res<T>(val, &stats);}

        template<typename... Ts>
        proc make_arr(Ts... args) 
        {
            return Array<Test_Res<T>, sizeof...(args)>{make(args)...};
        }

        proc total_constructed() 
            {return stats.move_constr + stats.copy_constr + stats.own_constr;}

        proc total_desctructed() 
            {return stats.destructed;}

        proc ok()
            { return total_constructed() == total_desctructed(); }
    };


    template<typename T, size_t N>
    struct Test_Alloc
    {
        T alloc_space[N];
        T* alloc_ptrs[N];
        size_t alloc_sizes[N];

        size_t alloced_elems = 0;
        size_t allocs = 0;
        size_t frees = 0;

        constexpr T* allocate(size_t bytes)
        {
            size_t rounded_up = (bytes + sizeof(T) - 1) / sizeof(T);
            if(bytes % sizeof(T) != 0)
                throw std::exception("Bad requested memory");

            if(alloced_elems + rounded_up > N || allocs >= N)
                throw std::exception("Out of space");
        
            T* res = alloc_space + alloced_elems;
            
            alloc_sizes[allocs] = bytes;
            alloc_ptrs[allocs] = res;

            alloced_elems += rounded_up;
            allocs ++;
            return res;
        }


        constexpr void deallocate(T* ptr, size_t bytes)
        {
            bool found = false;
            for(size_t i = 0; i < allocs; i++)
            {
                if(alloc_ptrs[i] == ptr)
                {
                    if(alloc_sizes[i] != bytes)
                        throw std::exception("Bad dealloced size");
                    found = true;
                }
            }

            if(!found)
                throw std::exception("Bad dealloced ptr");

            frees++;
        }

        //constexpr Test_Alloc(T* data, size_t capacity) : alloc_space(data), capacity(capacity) {}

        //constexpr ~Test_Alloc() noexcept(false)
        //{
            //if(allocs != frees)
                //throw std::exception("Not matching number of allocs and frees");
        //}
    };

    template<typename T>
    test test_push_pop(Array<T, 3> vals)
    {
        using Grow = Def_Grow<2, 0, 6>;
        using Alloc = Def_Alloc<T>;
        using Size = size_t;

        {
            Stack<T, 2, Size, Alloc, Grow> stack;

            runtime_assert(stack.data == 0);
            runtime_assert(stack.size == 0);
            runtime_assert(stack.capacity == 0);

            runtime_assert(*push(&stack, vals[0]) == vals[0]);

            runtime_assert(stack.data == stack.static_data());
            runtime_assert(stack.size == 1);
            runtime_assert(stack.capacity == stack.static_capacity);

            runtime_assert(*push(&stack, vals[1]) == vals[1]);

            runtime_assert(stack.data == stack.static_data());
            runtime_assert(stack.size == 2);
            runtime_assert(stack.capacity == 2);

            runtime_assert(pop(&stack) == vals[1]);
            runtime_assert(pop(&stack) == vals[0]);

            runtime_assert(stack.size == 0);
            runtime_assert(stack.capacity == 2);

            runtime_assert(*push(&stack, vals[2]) == vals[2]);
            runtime_assert(*push(&stack, vals[1]) == vals[1]);
            runtime_assert(*push(&stack, vals[0]) == vals[0]);

            runtime_assert(stack.size == 3);
            runtime_assert(stack.capacity == 4);

            runtime_assert(stack[0] == vals[2]);
            runtime_assert(stack[1] == vals[1]);
            runtime_assert(stack[2] == vals[0]);
        }
    }

    template<typename T>
    test test_push_pop_constexpr(Array<T, 3> vals)
    {
        using Grow = Def_Grow<2, 0, 6>;
        using Alloc = Def_Alloc<T>;
        //using Alloc = Test_Alloc<T, 100>;
        using Size = size_t;

        {
            Stack<T, 0, Size, Alloc, Grow> stack;

            runtime_assert(stack.data == 0);
            runtime_assert(stack.size == 0);
            runtime_assert(stack.capacity == 0);

            runtime_assert(*push(&stack, vals[0]) == vals[0]);

            runtime_assert(stack.data != nullptr);
            runtime_assert(stack.size == 1);
            runtime_assert(stack.capacity == 8);

            runtime_assert(*push(&stack, vals[1]) == vals[1]);

            runtime_assert(stack.data == stack.static_data());
            runtime_assert(stack.size == 2);
            runtime_assert(stack.capacity == 2);

            runtime_assert(pop(&stack) == vals[1]);
            runtime_assert(pop(&stack) == vals[0]);

            runtime_assert(stack.size == 0);
            runtime_assert(stack.capacity == 8);

            runtime_assert(*push(&stack, vals[2]) == vals[2]);
            runtime_assert(*push(&stack, vals[1]) == vals[1]);
            runtime_assert(*push(&stack, vals[0]) == vals[0]);
            runtime_assert(*push(&stack, vals[0]) == vals[0]);
            runtime_assert(*push(&stack, vals[0]) == vals[0]);
            runtime_assert(*push(&stack, vals[0]) == vals[0]);
            runtime_assert(*push(&stack, vals[0]) == vals[0]);

            runtime_assert(stack.size == 7);
            runtime_assert(stack.capacity == 12);

            runtime_assert(stack[0] == vals[2]);
            runtime_assert(stack[1] == vals[1]);
            runtime_assert(stack[2] == vals[0]);
        }
    }

    struct Test_No_Construct
    {
        int val;
        constexpr Test_No_Construct() = delete;
        constexpr Test_No_Construct(int val) : val(val) {}
    };

    struct Allocer : std::allocator<Test_No_Construct>
    {
        Test_No_Construct* ptr = nullptr;

    };

    constexpr void alloc(Allocer* alloc)
    {
        alloc->ptr = alloc->allocate(sizeof(Test_No_Construct));
        new (alloc->ptr) Test_No_Construct(5);
    }


    constexpr void dealloc(Allocer* alloc)
    {
        alloc->deallocate(alloc->ptr, sizeof(Test_No_Construct));
    }

    test test_push_pop()
    {
        Res_Watch<i32> w1;
        Res_Watch<f64> w2;
        test_push_pop<Test_Res<i32>>(w1.make_arr(10, 20, 30));
        test_push_pop<Test_Res<f64>>(w2.make_arr(1.0, 2.0, 3.0));
        test_push_pop<i32>({10, 20, 30});

        runtime_assert(w1.ok());
        runtime_assert(w2.ok());

        constexpr auto test_constexpr = []{
            //test_push_pop_constexpr<i32>({10, 20, 30});
            //test_push_pop_constexpr<f32>({1.0, 2.0, 3.0});

            return true;
        }();
    }

    test test_stack()
    {
        constexpr let test_constexpr = []{
            std::allocator<Test_No_Construct> alloc;

            auto* ptr = alloc.allocate(sizeof(Test_No_Construct));
            //new (ptr) Test_No_Construct(5);
            std::construct_at(ptr, 5);
            alloc.deallocate(ptr, sizeof(Test_No_Construct));

            return true;
        }();

        test_push_pop();
    }

    run_test(test_stack);
}


#include "../lib/jot/undefs.h"