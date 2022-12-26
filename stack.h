#pragma once

#include "utils.h"
#include "types.h"
#include "slice.h"
#include "memory.h"
#include "defines.h"

namespace jot
{
    static_assert(hasable<Allocator_State_Type>, "!");

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

        Stack(Stack moved other) noexcept;

        Stack(T* data, isize size, isize capacity, Allocator* allocator) noexcept
            : data{data}, size{size}, capacity{capacity}, allocator{allocator} {}

        ~Stack() noexcept;
        Stack& operator=(Stack moved other) noexcept;

        Stack(Stack in other) noexcept = delete;
        Stack& operator=(Stack in other) noexcept = delete;

        #include "slice_op_text.h"
    };

    #define templ_func template<class T> func
    #define templ_proc template<class T> proc

    templ_func slice(Stack<T> in stack) noexcept -> Slice<const T>{
        return Slice<const T>{stack.data, stack.size};
    }

    templ_func slice(Stack<T>* stack) noexcept -> Slice<T>{
        return Slice<T>{stack->data, stack->size};
    }

    templ_func capacity_slice(Stack<T> in stack) noexcept -> Slice<const T>{
        return Slice<const T>{stack.data, stack.capacity};
    }

    templ_func capacity_slice(Stack<T>* stack) noexcept -> Slice<T>{
        return Slice<T>{stack->data, stack->capacity};
    }

    templ_func allocator(Stack<T> in stack) noexcept -> const Allocator* { 
        return &stack.allocator; 
    }

    templ_func allocator(Stack<T>* stack) noexcept -> Allocator* { 
        return &stack.allocator; 
    }

    templ_func is_invariant(Stack<T> in stack) noexcept -> bool
    {
        const bool size_inv = stack.capacity >= stack.size;
        const bool data_inv = (stack.capacity == 0) == (stack.data == nullptr);
        const bool capa_inv = stack.capacity >= 0; 

        return size_inv && capa_inv && data_inv;
    }

    templ_func calculate_growth(Stack<T> in stack, isize to_fit) -> isize
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
        templ_proc destroy_items(Stack<T>* stack, isize from, isize to)
        {
            if constexpr(std::is_trivially_destructible_v<T> == false)
                for(isize i = from; i < to; i++)
                    stack->data[i].~T();
        }

        templ_proc alloc_data(Stack<T>* stack, isize new_capacity) -> Allocator_State_Type 
        {
            Allocation_Result res = stack->allocator->allocate(new_capacity * sizeof(T), DEF_ALIGNMENT<T>);
            if(res.state != Allocator_State::OK)
                return res.state;

            stack->data = res.items.data;
            stack->capacity = new_capacity;
        }

        templ_proc dealloc_data(Stack<T>* stack) -> void 
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
        templ_proc set_capacity(Stack<T>* stack, isize new_capacity) -> Allocator_State_Type 
        {
            T* no_alias new_data = nullptr;
            T* no_alias old_data = stack->data;

            Slice<u8> old_slice = cast_slice<u8>(capacity_slice(stack));
            Allocation_Result resize_res = stack->allocator->resize(old_slice, new_capacity * sizeof(T));
            if(resize_res.state == Allocator_State::OK)
            {
                let cast_res = cast_slice<T>(resize_res.items);
                assert(cast_res.size == new_capacity);
                assert(cast_res.data == nullptr ? new_capacity == 0 : true);

                stack->data = cast_res.data;
                stack->capacity = cast_res.size;
                return Allocator_State::OK;
            }

            Allocation_Result allocation_res = stack->allocator->allocate(new_capacity * sizeof(T), DEF_ALIGNMENT<T>);
            if(allocation_res.state != Allocator_State::OK)
                return allocation_res.state;

            let cast_res = cast_slice<T>(allocation_res.items);
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

    templ_proc assign(Stack<T>* to, Stack<T> in from) noexcept -> State
    {
        State ret_state = OK_STATE;
        if(to == &from)
            return ret_state;

        if(to->capacity < from.size)
        {
            let res = detail::set_capacity(to, from.size);
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
            T* no_alias to_data = to->data;
            const T* no_alias from_data = from.data;

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
    
    templ_proc swap(Stack<T>* left, Stack<T>* right) noexcept -> void 
    {
        swap(&left->data, &right->data);
        swap(&left->size, &right->size);
        swap(&left->capacity, &right->capacity);
        swap(&left->allocator, &right->allocator);
    }

    template<typename T>
    Stack<T>::Stack(Stack moved other) noexcept 
    {
        *this = move(&other);
    }

    template<typename T>
    Stack<T>& Stack<T>::operator=(Stack<T> moved other) noexcept 
    {
        swap(this, &other);
        return *this;
    }

    templ_proc reserve(Stack<T>* stack, isize to_fit) -> Allocator_State_Type
    {
        assert(is_invariant(*stack));

        if (stack->capacity >= to_fit)
            return Allocator_State::OK;

        isize realloc_to = calculate_growth(*stack, to_fit);
        Allocator_State_Type state = detail::set_capacity(stack, realloc_to);

        assert(is_invariant(*stack));
        return state;
    }

    templ_proc clear(Stack<T>* stack) -> void
    {
        assert(is_invariant(*stack));
        detail::destroy_items(stack);
        stack->size = 0;
        assert(is_invariant(*stack));
    }

    templ_func empty(Stack<T> in stack) noexcept -> bool
    {
        assert(is_invariant(stack));
        return stack.size == 0;
    }

    templ_func is_empty(Stack<T> in stack) noexcept -> bool
    {
        assert(is_invariant(stack));
        return stack.size == 0;
    }

    template <class T, stdr::forward_range Inserted>
    proc splice(Stack<T>* stack, isize at, isize replace_size, Inserted moved inserted) -> State
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
            mut reserve_state = reserve(stack, final_size);
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
        mut it = stdr::begin(inserted);
        const isize move_assign_to = at + move_inserted_size;
        for (isize i = at; i < move_assign_to; i++, ++it)
        {
            if constexpr(do_move_construct)
            {
                T in val = *it; //either copy or reference 
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
            T* prev = stack->data + i;
            if constexpr(do_move_construct)
            {
                T in val = *it; //either copy or reference 
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

    template <class T, stdr::forward_range Removed, stdr::forward_range Inserted>
    proc splice(Stack<T>* stack, isize at, Removed* removed, Inserted moved inserted) -> State
    {       
        static_assert(std::convertible_to<stdr::range_value_t<Inserted>, T>, "the types must be comaptible");
        static_assert(std::convertible_to<stdr::range_value_t<Removed>, T>, "the types must be comaptible");
        Stack<T>& stack_ref = *stack; //for bounds checks

        mut it = stdr::begin(*removed);
        let end = stdr::end(*removed);
        isize i = at;
        for (; it != end; i++, ++it)
            *it = move(&stack_ref[i]);

        return splice(stack, at, i - at, forward(Inserted, inserted));
    }

    templ_proc splice(Stack<T>* stack, isize at, isize replace_size) -> State
    {
        Slice<T, isize> empty;
        return splice(stack, at, replace_size, move(&empty));
    }

    templ_proc push(Stack<T>* stack, no_infer(T) what) -> State
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

    templ_proc pop(Stack<T>* stack) -> T
    {
        assert(is_invariant(*stack));
        assert(stack->size != 0);

        stack->size--;

        T ret = move(&stack->data[stack->size]);
        stack->data[stack->size].~T();
        assert(is_invariant(*stack));
        return ret;
    }

    template <class T, stdr::forward_range Inserted> requires (!same<Inserted, T>) 
    proc push(Stack<T>* stack, Inserted moved inserted) -> State
    {
        static_assert(std::convertible_to<stdr::range_value_t<Inserted>, T>, "the types must be comaptible");
        return splice(stack, stack->size, 0, std::forward<Inserted>(inserted));
    }

    templ_proc pop(Stack<T>* stack, isize count) -> void
    {
        State state = splice(stack, stack->size - count, count);
        assert(state == OK);
    }

    templ_proc last(Stack<T>* stack) -> T*
    {
        assert(is_invariant(*stack));
        assert(stack->size > 0);
        return &stack->data[stack->size - 1];
    }

    templ_proc last(Stack<T> in stack) -> T in
    {
        assert(is_invariant(stack));
        assert(stack.size > 0);
        return stack.data[stack.size - 1];
    }

    templ_proc first(Stack<T>* stack) -> T*
    {
        assert(is_invariant(*stack));
        assert(stack->size > 0);
        return &stack->data[0];
    }

    templ_proc first(Stack<T> in stack) -> T in
    {
        assert(is_invariant(stack));
        assert(stack.size > 0);
        return stack.data[0];
    }

    template <class T, bool is_zero = false>
    proc resize(Stack<T>* stack, isize to, no_infer(T) fillWith) -> State
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
                    mut state = construct_assign_at(stack->data + i, fillWith);
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

    templ_proc resize(Stack<T>* stack, isize to) -> State
    {
        return resize<T, true>(stack, to, T());
    }

    templ_proc resize_for_overwrite(Stack<T>* stack, isize to) -> State
    {
        static_assert(std::is_trivially_constructible_v<T>, "type must be POD!");
        State state = reserve(stack, to);
        if(state == ERROR)
            return state;

        stack->size = to;
        assert(is_invariant(*stack));
        return Allocator_State::OK;
    }

    templ_proc insert(Stack<T>* stack, isize at, no_infer(T) what) -> T*
    {
        assert(is_invariant(*stack));
        assert(0 <= at && at <= stack->size);

        Slice<T> view = {&what, 1};
        splice(stack, at, 0, move(*view));
        return stack->data + at;
    }

    templ_proc remove(Stack<T>* stack, isize at) -> T
    {
        assert(is_invariant(*stack));
        assert(0 <= at && at < stack->size);
        assert(0 < stack->size);

        T removed = move(&stack->data[at]);
        splice(stack, at, 1);
        return removed;
    }

    templ_proc unordered_remove(Stack<T>* stack, isize at) -> T
    {
        assert(0 <= at && at < stack->size);
        assert(0 < stack->size);

        swap(&stack->data[at], back(stack));
        return pop(stack);
    }

    templ_proc unordered_insert(Stack<T>* stack, isize at, no_infer(T) what) -> State
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
    templ_proc swap(jot::Stack<T>& stack1, jot::Stack<T>& stack2) -> void
    {
        jot::swap(&stack1, &stack2);
    }
}


#undef templ_func
#undef templ_proc

#include "undefs.h"