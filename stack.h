#pragma once

#include <ranges>

#include "utils.h"
#include "types.h"
#include "slice.h"
#include "memory.h"
#include "defines.h"

namespace jot
{
    namespace stdr = std::ranges;
    namespace stdv = std::views; 

    template <typename T>
    struct Stack
    {   
        T* data = nullptr;
        isize size = 0;
        isize capacity = 0;
        Allocator* allocator = memory_globals::default_allocator();

        Stack() noexcept = default;
        Stack(Allocator* allocator) noexcept
            : allocator{allocator} {}

        Stack(Stack && other) noexcept;

        Stack(T* data, isize size, isize capacity, Allocator* allocator) noexcept
            : data{data}, size{size}, capacity{capacity}, allocator{allocator} {}

        ~Stack() noexcept;
        Stack& operator=(Stack && other) noexcept;

        Stack(Stack const& other) noexcept = delete;
        Stack& operator=(Stack const& other) noexcept = delete;

        #include "slice_op_text.h"
    };


    template<class T> nodisc
    Slice<const T> slice(Stack<T> const& stack) 
    {
        return Slice<const T>{stack.data, stack.size};
    }

    template<class T> nodisc
    Slice<T> slice(Stack<T>* stack) 
    {
        return Slice<T>{stack->data, stack->size};
    }

    template<class T> nodisc
    Slice<const T> capacity_slice(Stack<T> const& stack) 
    {
        return Slice<const T>{stack.data, stack.capacity};
    }

    template<class T> nodisc
    Slice<T> capacity_slice(Stack<T>* stack) 
    {
        return Slice<T>{stack->data, stack->capacity};
    }

    template<class T> nodisc
    const Allocator* allocator(Stack<T> const& stack) 
    { 
        return &stack.allocator; 
    }

    template<class T> nodisc
    Allocator* allocator(Stack<T>* stack) 
    { 
        return &stack.allocator; 
    }

    template<class T> nodisc
    bool is_invariant(Stack<T> const& stack)
    {
        const bool size_inv = stack.capacity >= stack.size;
        const bool data_inv = (stack.capacity == 0) == (stack.data == nullptr);
        const bool capa_inv = stack.capacity >= 0; 

        return size_inv && capa_inv && data_inv;
    }

    template<class T> nodisc
    isize calculate_growth(Stack<T> const& stack, isize to_fit)
    {
        isize size = stack.size;
        while(size < to_fit)
            size = size * 3 / 2 + 8; //for small sizes grows fatser than classic factor of 2 for big slower

        return size;
    }

    template <typename T>
    concept memcpyable = std::is_scalar_v<T> || std::is_trivially_copy_constructible_v<T>;
    
    template <typename T>
    concept memsetable = std::is_scalar_v<T>;

    namespace detail 
    {
        template<class T> 
        void destroy_items(Stack<T>* stack, isize from, isize to)
        {
            if constexpr(std::is_trivially_destructible_v<T> == false)
                for(isize i = from; i < to; i++)
                    stack->data[i].~T();
        }

        template<class T> nodisc
        Allocator_State_Type alloc_data(Stack<T>* stack, isize new_capacity)
        {
            Allocation_Result res = stack->allocator->allocate(new_capacity * sizeof(T), DEF_ALIGNMENT<T>);
            if(res.state != Allocator_State::OK)
                return res.state;

            stack->data = res.items.data;
            stack->capacity = new_capacity;
        }

        template<class T> 
        void dealloc_data(Stack<T>* stack)
        {
            if(stack->data != nullptr)
            {
                Slice<u8> old_slice = cast_slice<u8>(capacity_slice(stack));
                stack->allocator->deallocate(old_slice, DEF_ALIGNMENT<T>);
            }
        }

        //can be used for arbitrary growing/shrinking of data
        // when called on uninit stack acts as alloc_data
        // when called with new_capacity = 0 acts as dealloc_data
        // Destroys elements when shrinking but does not construct new ones when growing 
        //  (because doesnt know how to)
        template<class T> nodisc
        Allocator_State_Type set_capacity(Stack<T>* stack, isize new_capacity)
        {
            T* RESTRICT new_data = nullptr;
            T* RESTRICT old_data = stack->data;

            Slice<u8> old_slice = cast_slice<u8>(capacity_slice(stack));

            //the resize will not rarely succeed when using stack allocator
            // and is very expensive when using ring allocator 
            // => we will use only use it when it saves considerable ammount of work
            //if(stack->capacity > 64 / sizeof(T) || std::is_trivially_copyable_v<T> == false)
            {
                Allocation_Result resize_res = stack->allocator->resize(old_slice, DEF_ALIGNMENT<T>, new_capacity * sizeof(T));
                if(resize_res.state == Allocator_State::OK)
                {
                    const auto cast_res = cast_slice<T>(resize_res.items);
                    assert(cast_res.size == new_capacity);
                    assert(cast_res.data == nullptr ? new_capacity == 0 : true);

                    stack->data = cast_res.data;
                    stack->capacity = cast_res.size;
                    return Allocator_State::OK;
                }
            }

            Allocation_Result allocation_res = stack->allocator->allocate(new_capacity * sizeof(T), DEF_ALIGNMENT<T>);
            if(allocation_res.state != Allocator_State::OK)
                return allocation_res.state;

            const auto cast_res = cast_slice<T>(allocation_res.items);
            new_data = cast_res.data;

            assert((are_aliasing<T>(slice(stack), Slice<T>{new_data, new_capacity}) == false));
            
            isize copy_to = min(stack->size, new_capacity);
            if constexpr(memcpyable<T>)
            {
                std::memcpy(new_data, old_data, copy_to * sizeof(T));
            }
            else
            {
                for (isize i = 0; i < copy_to; i++)
                    std::construct_at(new_data + i, move(old_data + i));
            }

            destroy_items(stack, 0, stack->size);
            dealloc_data(stack);

            stack->data = new_data;
            stack->capacity = new_capacity;

            return Allocator_State::OK;
        }
    }

    template<class T> nodisc
    State assign(Stack<T>* to, Stack<T> const& from) noexcept
    {
        State ret_state = OK_STATE;
        if(to == &from)
            return ret_state;

        if(to->capacity < from.size)
        {
            const auto res = detail::set_capacity(to, from.size);
            if(res == ERROR)
                return res;
        }

        isize to_size = min(to->size, from.size);

        //destroy extra
        detail::destroy_items(to, from.size, to->size);

        if(has_bit_by_bit_assign<T> && is_const_eval() == false)
        {
            //construct missing
            if(to->size < from.size)
                memcpy(to->data + to->size, from.data + to->size, (from.size - to->size) * sizeof(T));

            //copy rest
            memcpy(to->data, from.data, to_size * sizeof(T));
        }
        else
        {
            T* RESTRICT to_data = to->data;
            const T* RESTRICT from_data = from.data;

            //construct missing
            for (isize i = to->size; i < from.size; i++)
            {
                State state = construct_assign_at(to_data + i, from_data[i]);
                if(state == ERROR)
                    ret_state = state;
            }

            //copy rest
            for (isize i = 0; i < to_size; i++)
            {
                State state = assign(to_data + i, from_data[i]);
                if(state == ERROR)
                    ret_state = state;
            }
        }
            
        to->size = from.size;
        return ret_state;
    }

    template<typename T>
    Stack<T>::~Stack() noexcept 
    {
        if(data != nullptr)
        {
            detail::destroy_items(this, 0, this->size);
            detail::dealloc_data(this);
        }
    }
    
    template<class T> 
    void swap(Stack<T>* left, Stack<T>* right) noexcept
    {
        swap(&left->data, &right->data);
        swap(&left->size, &right->size);
        swap(&left->capacity, &right->capacity);
        swap(&left->allocator, &right->allocator);
    }

    template<typename T>
    Stack<T>::Stack(Stack && other) noexcept 
    {
        *this = move(&other);
    }

    template<typename T>
    Stack<T>& Stack<T>::operator=(Stack<T> && other) noexcept 
    {
        swap(this, &other);
        return *this;
    }

    template<class T> nodisc
    Allocator_State_Type reserve(Stack<T>* stack, isize to_fit)
    {
        assert(is_invariant(*stack));

        if (stack->capacity >= to_fit)
            return Allocator_State::OK;

        isize realloc_to = calculate_growth(*stack, to_fit);
        Allocator_State_Type state = detail::set_capacity(stack, realloc_to);

        assert(is_invariant(*stack));
        return state;
    }

    template<class T> 
    void clear(Stack<T>* stack)
    {
        assert(is_invariant(*stack));
        detail::destroy_items(stack);
        stack->size = 0;
        assert(is_invariant(*stack));
    }

    template<class T> nodisc
    bool empty(Stack<T> const& stack) noexcept
    {
        assert(is_invariant(stack));
        return stack.size == 0;
    }

    template<class T> nodisc
    bool is_empty(Stack<T> const& stack) noexcept
    {
        assert(is_invariant(stack));
        return stack.size == 0;
    }

    template <class T, stdr::forward_range Inserted> nodisc
    State splice(Stack<T>* stack, isize at, isize replace_size, Inserted && inserted)
    {       
        static_assert(std::convertible_to<stdr::range_value_t<Inserted>, T>, "the types must be comaptible");
        State ret_state = OK_STATE;

        Stack<T>& stack_ref = *stack; //for bounds checks
        assert(is_invariant(*stack));

        constexpr bool do_move_construct = std::is_rvalue_reference_v<decltype(inserted)>;
        constexpr bool do_copy_construct = std::is_trivially_copy_constructible_v<T>;

        const isize inserted_size = cast(isize) stdr::size(inserted);
        const isize insert_to = at + inserted_size;
        const isize replace_to = at + replace_size;
        const isize remaining = stack_ref.size - replace_to;
        const isize final_size = stack_ref.size + inserted_size - replace_size;

        assert(0 <= at && at <= stack_ref.size);
        assert(0 <= replace_to && replace_to <= stack_ref.size);

        isize move_inserted_size = 0;

        if(inserted_size > replace_size)
        {
            auto reserve_state = reserve(stack, final_size);
            if(reserve_state == ERROR)
                return reserve_state;

            const isize constructed = inserted_size - replace_size;
            const isize constructed_in_shift = min(constructed, remaining); 
            const isize construction_gap = constructed - constructed_in_shift;

            move_inserted_size = inserted_size - construction_gap;

            //construct some elements by shifting right previous elements into them
            const isize to = final_size - constructed_in_shift;
            for (isize i = final_size; i-- > to; )
            {
                if constexpr(do_move_construct)
                    std::construct_at(stack->data + i, move(&stack_ref[i - constructed]));
                else if(do_copy_construct)
                    std::construct_at(stack->data + i, stack_ref[i - constructed]);
                else
                {
                    State state = construct_assign_at(stack->data + i, stack_ref[i - constructed]);
                    if(state == ERROR)
                        ret_state = state;
                }
            }

            //shifting right rest of the elems to make space for insertion
            for(isize i = stack_ref.size; i-- > insert_to; )
                stack_ref[i] = move(&stack_ref[i - constructed]);
        }
        else
        {
            move_inserted_size = inserted_size; 

            //move elems left to the freed space
            const isize removed = replace_size - inserted_size;
            for (isize i = insert_to; i < final_size; i++)
                stack_ref[i] = move(&stack_ref[i + removed]);

            
            //Destroy all excess elems
            detail::destroy_items(stack, final_size, stack->size);
        }

        stack->size = final_size;

        //insert the added elems into constructed slots
        auto it = stdr::begin(inserted);
        const isize move_assign_to = at + move_inserted_size;
        for (isize i = at; i < move_assign_to; i++, ++it)
        {
            if constexpr(do_move_construct)
            {
                T const& val = *it; //either copy or reference 
                //either way now ve can actually get ptr to it and pass it to move
                stack_ref[i] = move(&val);
            }
            else if(do_copy_construct)
                stack_ref[i] = *it;
            else
            {
                State state = assign(&stack_ref[i], *it);
                if(state == ERROR)
                    ret_state = state;
            }
        }

        //insert construct leftover elements
        for (isize i = move_assign_to; i < insert_to; i++, ++it)
        {
            //T* prev = stack->data + i;
            if constexpr(do_move_construct)
            {
                T const& val = *it; //either copy or reference 
                std::construct_at(stack->data + i, move(&val));
            }
            else if(do_copy_construct)
                std::construct_at(stack->data + i, *it);
            else
            {
                State state = construct_assign_at(stack->data + i, *it);
                if(state == ERROR)
                    ret_state = state;
            }
        }

        assert(is_invariant(*stack));
        return ret_state;
    }

    template <class T, stdr::forward_range Removed, stdr::forward_range Inserted> nodisc
    State splice(Stack<T>* stack, isize at, Removed* removed, Inserted && inserted)
    {       
        static_assert(std::convertible_to<stdr::range_value_t<Inserted>, T>, "the types must be comaptible");
        static_assert(std::convertible_to<stdr::range_value_t<Removed>, T>, "the types must be comaptible");
        Stack<T>& stack_ref = *stack; //for bounds checks

        auto it = stdr::begin(*removed);
        const auto end = stdr::end(*removed);
        isize i = at;
        for (; it != end; i++, ++it)
            *it = move(&stack_ref[i]);

        return splice(stack, at, i - at, forward(Inserted, inserted));
    }

    template<class T> nodisc
    State remove_multiple(Stack<T>* stack, isize at, isize replace_size)
    {
        Slice<T, isize> empty;
        return splice(stack, at, replace_size, move(&empty));
    }

    template<class T> nodisc
    State push(Stack<T>* stack, no_infer(T) what)
    {
        assert(is_invariant(*stack));

        State reserve_res = reserve(stack, stack->size + 1);
        if(reserve_res == ERROR)
            return reserve_res;
        
        T* ptr = stack->data + stack->size;
        std::construct_at(ptr, move(&what));
        stack->size++;

        assert(is_invariant(*stack));
        return Allocator_State::OK;
    }

    template<class T> 
    T pop(Stack<T>* stack)
    {
        assert(is_invariant(*stack));
        assert(stack->size != 0);

        stack->size--;

        T ret = move(&stack->data[stack->size]);
        stack->data[stack->size].~T();
        assert(is_invariant(*stack));
        return ret;
    }

    template <class T, stdr::forward_range Inserted>  requires (!same<Inserted, T>) nodisc
    State push_multiple(Stack<T>* stack, Inserted && inserted)
    {
        static_assert(std::convertible_to<stdr::range_value_t<Inserted>, T>, "the types must be comaptible");
        return splice(stack, stack->size, 0, std::forward<Inserted>(inserted));
    }

    template<class T> 
    void pop(Stack<T>* stack, isize count)
    {
        State state = splice(stack, stack->size - count, count);
        assert(state == OK);
    }

    template<class T> nodisc
    T* last(Stack<T>* stack)
    {
        assert(stack->size > 0);
        return &stack->data[stack->size - 1];
    }

    template<class T> 
    T const& last(Stack<T> const& stack)
    {
        assert(stack.size > 0);
        return stack.data[stack.size - 1];
    }

    template<class T> 
    T* first(Stack<T>* stack) 
    {
        assert(stack->size > 0);
        return &stack->data[0];
    }

    template<class T> 
    T const& first(Stack<T> const& stack)
    {
        assert(stack.size > 0);
        return stack.data[0];
    }

    template <class T, bool is_zero = false> nodisc
    State resize(Stack<T>* stack, isize to, no_infer(T) fillWith)
    {
        assert(is_invariant(*stack));
        assert(0 <= to);

        State ret_state = reserve(stack, to);
        if(ret_state == ERROR)
            return ret_state;

        if(memsetable<T> && is_zero && is_const_eval() == false)
        {
            if(stack->size < to)
                memset(stack->data + stack->size, 0, (to - stack->size)*sizeof(T));
        }
        else
        {
            for (isize i = stack->size; i < to; i++)
            {
                if constexpr(std::is_trivially_copy_constructible_v<T>)
                    stack->data[i] = fillWith;
                else
                {
                    auto state = construct_assign_at(stack->data + i, fillWith);
                    if(state == ERROR)
                        ret_state = state;
                }
            }

            detail::destroy_items(stack, to, stack->size);
        }

        stack->size = to;
        assert(is_invariant(*stack));
        return ret_state;
    }

    template<class T> nodisc 
    State resize(Stack<T>* stack, isize to)
    {
        return resize<T, true>(stack, to, T());
    }

    template<class T> nodisc 
    State resize_for_overwrite(Stack<T>* stack, isize to)
    {
        if(std::is_trivially_constructible_v<T> == false)
            return resize(stack, to);

        State state = reserve(stack, to);
        if(state == ERROR)
            return state;

        stack->size = to;
        assert(is_invariant(*stack));
        return Allocator_State::OK;
    }

    template<class T> nodisc 
    State insert(Stack<T>* stack, isize at, no_infer(T) what)
    {
        assert(0 <= at && at <= stack->size);

        Slice<T> view = {&what, 1};
        return splice(stack, at, 0, move(*view));
    }

    template<class T> 
    T remove(Stack<T>* stack, isize at)
    {
        assert(0 <= at && at < stack->size);
        assert(stack->size > 0);

        T removed = move(&stack->data[at]);
        splice(stack, at, 1);
        return removed;
    }

    template<class T> 
    T unordered_remove(Stack<T>* stack, isize at)
    {
        assert(0 <= at && at < stack->size);
        assert(stack->size > 0);

        swap(&stack->data[at], back(stack));
        return pop(stack);
    }

    template<class T> nodisc
    State unordered_insert(Stack<T>* stack, isize at, no_infer(T) what)
    {
        assert(0 <= at && at <= stack->size);

        State state = push(stack, move(&what));
        if(state == ERROR)
            return state;

        swap(&stack->data[at], back(stack));
        return Allocator_State::OK;
    }
}

namespace std 
{
    template<class T> 
    void swap(jot::Stack<T>& stack1, jot::Stack<T>& stack2)
    {
        jot::swap(&stack1, &stack2);
    }
}

#include "undefs.h"