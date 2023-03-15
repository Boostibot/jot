#pragma once

#include "utils.h"
#include "slice.h"
#include "memory.h"
#include "defines.h"

namespace jot
{

    //Contiguos dynamic array.
    //Acts in exactly the same as std::vector but is not freely copyable and comaprable
    // (since those ops are costly). Use the appropriate 'copy' and 'compare' functions instead.
    template <typename T>
    struct Stack
    {   
        T* _data = nullptr;
        isize _size = 0;
        isize _capacity = 0;
        Allocator* _allocator = memory_globals::default_allocator();

        Stack() noexcept = default;
        Stack(Allocator* allocator) noexcept
            : _allocator{allocator} {}

        Stack(Stack && other) noexcept;
        Stack(Stack const& other);

        Stack(T* data, isize size, isize capacity, Allocator* allocator) noexcept
            : _data{data}, _size{size}, _capacity{capacity}, _allocator{allocator} {}

        ~Stack() noexcept;
        Stack& operator=(Stack && other) noexcept;
        Stack& operator=(Stack const& other);
        
        #define DATA _data
        #define SIZE _size
        #include "slice_op_text.h"
    };

    template<class T> nodisc
    T* const data(Stack<T> const& stack)
    {
        return stack._data;
    }

    template<class T> nodisc
    T* data(Stack<T>* stack)
    {
        return stack->_data;
    }

    template<class T> nodisc
    isize size(Stack<T> const& stack)
    {
        return stack._size;
    }
    
    template<class T> nodisc
    isize capacity(Stack<T> const& stack)
    {
        return stack._capacity;
    }

    template<class T> nodisc
    Slice<const T> slice(Stack<T> const& stack) 
    {
        return Slice<const T>{stack._data, stack._size};
    }

    template<class T> nodisc
    Slice<T> slice(Stack<T>* stack) 
    {
        return Slice<T>{stack->_data, stack->_size};
    }

    template<class T> nodisc
    Allocator* allocator(Stack<T> const& stack) 
    { 
        return stack._allocator; 
    }

    template<class T> nodisc
    Allocator** allocator(Stack<T>* stack) 
    { 
        return &stack._allocator; 
    }

    template<class T> nodisc
    T* last(Stack<T>* stack)
    {
        assert(stack->_size > 0);
        return &stack->_data[stack->_size - 1];
    }

    template<class T> 
    T const& last(Stack<T> const& stack)
    {
        assert(stack._size > 0);
        return stack._data[stack._size - 1];
    }

    template<class T> 
    T* first(Stack<T>* stack) 
    {
        assert(stack->_size > 0);
        return &stack->_data[0];
    }

    template<class T> 
    T const& first(Stack<T> const& stack)
    {
        assert(stack._size > 0);
        return stack._data[0];
    }

    template<class T> nodisc
    bool is_invariant(Stack<T> const& stack)
    {
        const bool size_inv = stack._capacity >= stack._size;
        const bool data_inv = (stack._capacity == 0) == (stack._data == nullptr);
        const bool capa_inv = stack._capacity >= 0; 

        return size_inv && capa_inv && data_inv;
    }

    constexpr inline
    isize calculate_stack_growth(isize curr_size, isize to_fit)
    {
        isize size = curr_size;
        while(size < to_fit)
            size = size * 3 / 2 + 8; //for small sizes grows fatser than classic factor of 2 for big slower

        return size;
    }

    template<class T>
    struct Set_Capacity_Result
    {
        Allocator_State_Type state = Allocator_State::OK;
        Slice<T> items;
        bool adress_changed = true;
    };

    template<class T> nodisc
    Set_Capacity_Result<T> set_capacity_allocation_stage(Allocator* allocator, Slice<T>* old_slice, isize align, isize new_capacity, bool try_resize) noexcept
    {
        Set_Capacity_Result<T> result;
        if(new_capacity == 0)
        {
            result.state = Allocator_State::OK;
            result.items = Slice<T>();
            result.adress_changed = false;
            return result;
        }

        if(old_slice->data != nullptr && try_resize)
        {
            Slice<u8> old_data = cast_slice<u8>(*old_slice);
            Allocation_Result resize_res = allocator->resize(old_data, align, new_capacity * cast(isize) sizeof(T));
            if(resize_res.state == Allocator_State::OK)
            {
                result.state = Allocator_State::OK;
                result.items = cast_slice<T>(resize_res.items);
                result.adress_changed = false;

                assert(result.items.size >= new_capacity);
                assert(result.items.data == nullptr ? new_capacity == 0 : true);

                return result;
            }
        }
            
        Allocation_Result allocation_res = allocator->allocate(new_capacity * cast(isize) sizeof(T), align);
        result.state = allocation_res.state;
        result.items = cast_slice<T>(allocation_res.items);
        result.adress_changed = true;

        return result;
    }
        
    template<class T>
    void set_capacity_deallocation_stage(Allocator* allocator, Slice<T>* old_slice, isize filled_to, isize align, Set_Capacity_Result<T>* result) noexcept
    {
        assert(result->state == OK && "must be okay!");

        T* RESTRICT new_data = result->items.data;
        T* RESTRICT old_data = old_slice->data;
            
        isize new_capacity = result->items.size;
        if(result->adress_changed)
        {
            assert((are_aliasing<T>(*old_slice, result->items) == false));

            isize copy_to = min(filled_to, new_capacity);
            if constexpr(is_byte_copyable<T>)
            {
                std::memcpy(new_data, old_data, copy_to * sizeof(T));
            }
            else
            {
                for (isize i = 0; i < copy_to; i++)
                    construct_at(new_data + i, move(old_data + i));
            }
            
            if constexpr(std::is_trivially_destructible_v<T> == false)
                for(isize i = 0; i < filled_to; i++)
                    old_data[i].~T();
        }
        else
        {
            if constexpr(std::is_trivially_destructible_v<T> == false)
                for(isize i = new_capacity; i < filled_to; i++)
                    old_data[i].~T();
        }

        if(old_slice->data != nullptr)
            allocator->deallocate(cast_slice<u8>(*old_slice), align);
    }
      
    //fast path for destroying of stacks (instead of set_capacity which is longer)
    template<class T>
    Allocator_State_Type destroy_and_deallocate(Allocator* allocator, Slice<T>* old_slice, isize filled_to, isize align) noexcept
    {
        if constexpr(std::is_trivially_destructible_v<T> == false)
            for(isize i = 0; i < filled_to; i++)
                (*old_slice)[i].~T();

        if(old_slice->data != nullptr)
            return allocator->deallocate(cast_slice<u8>(*old_slice), align);

        return Allocator_State::OK;
    }

    // Can be used for arbitrary growing/shrinking of data
    // When old_slice is null only allocates the data
    // When new_capacity is 0 only deallocates the data
    // Destroys elements when shrinking but does not construct new ones when growing 
    //  (because doesnt know how to)
    template<class T> nodisc
    Set_Capacity_Result<T> set_capacity(Allocator* allocator, Slice<T>* old_slice, isize filled_to, isize align, isize new_capacity, bool try_resize) noexcept
    {
        Set_Capacity_Result<T> result = set_capacity_allocation_stage(allocator, old_slice, align, new_capacity, try_resize);
        if(result.state == ERROR)
            return result;

        set_capacity_deallocation_stage(allocator, old_slice, filled_to, align, &result);
        return result;
    }
        
    namespace stack_internal 
    {
        template<class T> 
        void destroy_items(Stack<T>* stack, isize from, isize to)
        {
            if constexpr(std::is_trivially_destructible_v<T> == false)
                for(isize i = from; i < to; i++)
                    stack->_data[i].~T();
        }

        template<class T> nodisc
        Allocator_State_Type set_capacity(Stack<T>* stack, isize new_capacity) noexcept
        {
            Slice<T> old_slice = {data(stack), capacity(*stack)};
            bool is_data_size_saving = stack->_capacity > 64 / sizeof(T);
            bool is_constructor_call_saving = std::is_trivially_copyable_v<T> == false;

            Set_Capacity_Result<T> result = set_capacity(stack->_allocator, &old_slice, stack->_size, DEF_ALIGNMENT<T>, 
                new_capacity, is_data_size_saving || is_constructor_call_saving);

            if(result.state == ERROR)
                return result.state;

            stack->_data = result.items.data;
            stack->_capacity = new_capacity;
            return Allocator_State::OK;
        }
    }

    template<class T> nodisc
    State copy(Stack<T>* to, Slice<const T> from) noexcept
    {
        State ret_state = OK_STATE;
        if(data(to) == from.data)
            return ret_state;

        if(to->_capacity < from.size)
        {
            const auto res = stack_internal::set_capacity(to, from.size);
            if(res == ERROR)
                return res;
        }

        isize to_size = min(to->_size, from.size);

        //destroy extra
        stack_internal::destroy_items(to, from.size, to->_size);
        if(is_byte_copyable<T>)
        {
            //construct missing
            if(to->_size < from.size)
                memcpy(to->_data + to->_size, from.data + to->_size, (from.size - to->_size) * sizeof(T));

            //copy rest
            memcpy(to->_data, from.data, to_size * sizeof(T));
        }
        else
        {
            T* RESTRICT to_data = to->_data;
            const T* RESTRICT from_data = from.data;

            //construct missing
            for (isize i = to->_size; i < from.size; i++)
            {
                State state = copy_construct_at(to_data + i, from_data[i]);
                if(state == ERROR)
                    ret_state = state;
            }

            //copy rest
            for (isize i = 0; i < to_size; i++)
            {
                State state = copy(to_data + i, from_data[i]);
                if(state == ERROR)
                    ret_state = state;
            }
        }
            
        to->_size = from.size;
        return ret_state;
    }

    template <typename T>
    struct Copyable<Stack<T>>
    {
        nodisc static
        State copy(Stack<T>* to, Stack<T> const& from) noexcept
        {
            return jot::copy<T>(to, slice(from));
        }
    };

    template <typename T>
    struct Swappable<Stack<T>>
    {
        nodisc static
        void swap(Stack<T>* left, Stack<T>* right) noexcept
        {
            jot::swap(&left->_data, &right->_data);
            jot::swap(&left->_size, &right->_size);
            jot::swap(&left->_capacity, &right->_capacity);
            jot::swap(&left->_allocator, &right->_allocator);
        }
    };

}

namespace std 
{
    template<class T> 
    size_t size(jot::Stack<T> const& stack) noexcept
    {
        return jot::size(stack);
    }

    template<class T> 
    void swap(jot::Stack<T>& stack1, jot::Stack<T>& stack2)
    {
        jot::swap(&stack1, &stack2);
    }
}

namespace jot
{
    template<typename T>
    Stack<T>::~Stack() noexcept 
    {
        Slice<T> old_slice = {_data, _capacity};
        destroy_and_deallocate(_allocator, &old_slice, _size, DEF_ALIGNMENT<T>);
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
    
    template<typename T>
    Stack<T>::Stack(Stack const& other) 
    {
        State state = Copyable<Stack<T>>::copy(this, other);
        if(state == ERROR)
            panic("Stack copy assigment failed!");
    }

    template<typename T>
    Stack<T>& Stack<T>::operator=(Stack<T> const& other) 
    {
        State state = Copyable<Stack<T>>::copy(this, other);
        if(state == ERROR)
            panic("Stack copy assigment failed!");

        return *this;
    }

    template<class T> nodisc
    Allocator_State_Type reserve(Stack<T>* stack, isize to_fit)
    {
        assert(is_invariant(*stack));

        if (stack->_capacity >= to_fit)
            return Allocator_State::OK;

        isize realloc_to = calculate_stack_growth(stack->_size, to_fit);
        Allocator_State_Type state = stack_internal::set_capacity(stack, realloc_to);

        assert(is_invariant(*stack));
        return state;
    }

    template<class T> nodisc
    bool is_empty(Stack<T> const& stack) noexcept
    {
        assert(is_invariant(stack));
        return stack._size == 0;
    }

    template <class T> nodisc
    State splice_bytes(Stack<T>* stack, isize at, isize replace_size, Slice<const T> inserted) noexcept
    { 
        assert(is_invariant(*stack));
        Slice<T> items = slice(stack);

        const isize size_delta = inserted.size - replace_size;
        const isize final_size = items.size + size_delta;
        
        State state = reserve(stack, final_size);
        if(state == ERROR)
            return state;

        if(size_delta > 0)
        {
            Slice<T> move_from = slice(slice(stack), at);
            Slice<T> move_to = {move_from.data + size_delta, move_from.size};
            move_items<T>(&move_to, &move_from);
        }
        else
        {
            Slice<T> move_to = slice(slice(stack), at);
            Slice<T> move_from = slice(move_to, -size_delta); //delta is now negative
            move_items<T>(&move_to, &move_from);
        }

        Slice<T> insert_to = {data(stack) + at, inserted.size};
        copy_items<T>(&insert_to, inserted);

        stack->_size = final_size;
        assert(is_invariant(*stack));
        return state;
    }
    
    template <class T, class Inserted> nodisc
    State splice(Stack<T>* stack, isize at, isize replace_size, Inserted && inserted) noexcept
    {       
        State state = OK_STATE;

        Slice<T> items = slice(stack); //for bounds checks
        assert(is_invariant(*stack));
        
        auto it = std::begin(inserted);
        constexpr bool do_move_construct = std::is_rvalue_reference_v<decltype(inserted)>;
        constexpr bool do_copy_construct = std::is_trivially_copy_constructible_v<T>;
        constexpr bool is_contiguous_iterator = std::is_pointer_v<decltype(it)>;
        
        const isize inserted_size = cast(isize) std::size(inserted);

        if constexpr(is_byte_copyable<T>)
            if(is_contiguous_iterator)
                return splice_bytes(stack, at, replace_size, Slice<const T>{it, inserted_size});
        
        const isize insert_to = at + inserted_size;
        const isize replace_to = at + replace_size;
        const isize remaining = items.size - replace_to;
        const isize final_size = items.size + inserted_size - replace_size;

        assert(0 <= at && at <= items.size);
        assert(0 <= replace_to && replace_to <= items.size);

        isize move_inserted_size = 0;

        if(inserted_size > replace_size)
        {
            State reserve_state = reserve(stack, final_size);
            if(reserve_state == ERROR)
                return reserve_state;

            items = slice(stack);
            const isize constructed = inserted_size - replace_size;
            const isize constructed_in_shift = min(constructed, remaining); 
            const isize construction_gap = constructed - constructed_in_shift;

            move_inserted_size = inserted_size - construction_gap;

            //construct some elements by shifting right previous elements into them
            const isize to = final_size - constructed_in_shift;
            for (isize i = final_size; i-- > to; )
            {
                if constexpr(do_move_construct)
                    construct_at(items.data + i, move(&items[i - constructed]));
                else if(do_copy_construct)
                    construct_at(items.data + i, items[i - constructed]);
                else
                    acumulate(&state, copy_construct_at(items.data + i, items[i - constructed]));
            }

            //shifting right rest of the elems to make space for insertion
            for(isize i = items.size; i-- > insert_to; )
                items[i] = move(&items[i - constructed]);
        }
        else
        {
            move_inserted_size = inserted_size; 

            //move elems left to the freed space
            const isize removed = replace_size - inserted_size;
            for (isize i = insert_to; i < final_size; i++)
                items[i] = move(&items[i + removed]);

            //Destroy all excess elems
            stack_internal::destroy_items(stack, final_size, stack->_size);
        }

        stack->_size = final_size;

        //insert the added elems into constructed slots
        const isize move_assign_to = at + move_inserted_size;
        for (isize i = at; i < move_assign_to; i++, ++it)
        {
            if constexpr(do_move_construct)
                items[i] = move(&*it);
            else if(do_copy_construct)
                items[i] = *it;
            else
                acumulate(&state, copy(&items[i], *it));
        }

        //insert construct leftover elements
        for (isize i = move_assign_to; i < insert_to; i++, ++it)
        {
            //T* prev = items.data + i;
            if constexpr(do_move_construct)
                construct_at(items.data + i, move(&*it));
            else if(do_copy_construct)
                construct_at(items.data + i, *it);
            else
                acumulate(&state, copy_construct_at(items.data + i, *it));
        }

        assert(is_invariant(*stack));
        return state;
    }

    template<class T> nodisc
    State push(Stack<T>* stack, no_infer(T) what)
    {
        assert(is_invariant(*stack));

        State reserve_res = reserve(stack, stack->_size + 1);
        if(reserve_res == ERROR)
            return reserve_res;
        
        T* ptr = stack->_data + stack->_size;
        construct_at(ptr, move(&what));
        stack->_size++;

        assert(is_invariant(*stack));
        return Allocator_State::OK;
    }

    template<class T> 
    T pop(Stack<T>* stack)
    {
        assert(is_invariant(*stack));
        assert(stack->_size != 0);

        stack->_size--;

        T ret = move(&stack->_data[stack->_size]);
        stack->_data[stack->_size].~T();
        assert(is_invariant(*stack));
        return ret;
    }

    template <class T, class Inserted> nodisc
    State push_multiple(Stack<T>* stack, Inserted && inserted)
    {
        isize inserted_size = cast(isize) std::size(inserted);
        State state = reserve(stack, size(*stack) + inserted_size);
        if(state == ERROR)
            return state;

        auto it = std::begin(inserted);
        auto end = std::end(inserted);
        using Iter_Val = std::remove_reference_t<decltype(*it)>;

        constexpr bool is_contiguous_iterator = std::is_pointer_v<decltype(it)>;
        constexpr bool do_move_construct = std::is_rvalue_reference_v<decltype(inserted)>;
        constexpr bool do_copy_construct = std::is_trivially_copy_constructible_v<T>;

        static_assert(std::is_same_v<T, std::remove_cv_t<Iter_Val>>, "must be compatible!");
        Slice<T> capacity_slice = {data(stack), capacity(*stack)};

        if constexpr(is_contiguous_iterator && is_byte_copyable<T>)
        {
            Slice empty_space = slice(capacity_slice, size(*stack));
            Slice inserted_slice = Slice{it, inserted_size};
            copy_items<T>(&empty_space, inserted_slice);
        }
        else
        {
            for(isize i = size(*stack); it != end; ++it, ++ i)
            {
                if constexpr(do_move_construct)
                    construct_at(capacity_slice.data + i, move(&*it));
                else if(do_copy_construct)
                    construct_at(capacity_slice.data + i, *it);
                else
                    acumulate(&state, copy_construct_at(capacity_slice.data + i, *it));
            }
        }

        stack->_size += inserted_size;
        return state;
    }

    template<class T> 
    void pop_multiple(Stack<T>* stack, isize count)
    {
        assert(count <= size(*stack));
        stack_internal::destroy_items(stack, stack->_size - count, stack->_size);
        stack->_size -= count;
    }
    
    template<class T> 
    void clear(Stack<T>* stack)
    {
        pop_multiple(stack, stack->_size);
        assert(is_invariant(*stack));
    }

    template <class T, bool is_zero = false> nodisc
    State resize(Stack<T>* stack, isize to, no_infer(T) const& fillWith) noexcept
    {
        assert(is_invariant(*stack));
        assert(0 <= to);

        State ret_state = reserve(stack, to);
        if(ret_state == ERROR)
            return ret_state;

        if(is_byte_nullable<T> && is_zero)
        {
            if(stack->_size < to)
                memset(stack->_data + stack->_size, 0, (to - stack->_size)*sizeof(T));
        }
        else
        {
            for (isize i = stack->_size; i < to; i++)
            {
                if constexpr(std::is_trivially_copy_constructible_v<T>)
                    stack->_data[i] = fillWith;
                else
                {
                    State state = copy_construct_at(stack->_data + i, fillWith);
                    if(state == ERROR)
                        ret_state = state;
                }
            }

            stack_internal::destroy_items(stack, to, stack->_size);
        }

        stack->_size = to;
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

        stack->_size = to;
        assert(is_invariant(*stack));
        return Allocator_State::OK;
    }

    template<class T> nodisc 
    State insert(Stack<T>* stack, isize at, no_infer(T) what)
    {
        assert(0 <= at && at <= stack->_size);
        if(at >= size(*stack))
            return push(stack, move(&what));
            
        State state = reserve(stack, stack->_size + 1);
        if(state == ERROR)
            return state;

        construct_at(last(stack) + 1, move(last(stack)));

        Slice<T> move_from = slice_range(slice(stack), at, stack->_size - 1);
        Slice<T> move_to = slice_range(slice(stack), at + 1, stack->_size);
        move_items(&move_to, &move_from);

        stack->_data[at] = move(&what);
        stack->_size += 1;

        return state;
    }

    template<class T> 
    T remove(Stack<T>* stack, isize at)
    {
        assert(0 <= at && at < stack->_size);
        assert(stack->_size > 0);
        
        T removed = move(&stack->_data[at]);
        
        Slice<T> move_from = slice_range(slice(stack), at + 1, stack->_size);
        Slice<T> move_to = slice_range(slice(stack), at, stack->_size - 1);
        move_items(&move_to, &move_from);

        (last(stack))->~T();
        stack->_size -= 1;
        return removed;
    }

    template<class T> 
    T unordered_remove(Stack<T>* stack, isize at)
    {
        assert(0 <= at && at < stack->_size);
        assert(stack->_size > 0);

        swap(&stack->_data[at], last(stack));
        return pop(stack);
    }

    template<class T> nodisc
    State unordered_insert(Stack<T>* stack, isize at, no_infer(T) what)
    {
        assert(0 <= at && at <= stack->_size);

        State state = push(stack, move(&what));
        if(state == ERROR)
            return state;

        swap(&stack->_data[at], last(stack));
        return Allocator_State::OK;
    }
    
    //Simple struct that acts as a slice to stack ment to be used
    // as an iterface type when its desired to only allow pushing into a stack
    // pretends it only has data from _from_index guarding the data in front of it from modification
    template <typename T>
    struct Stack_Appender
    {
        Stack<T>* _stack;
        isize _from_index = 0;

        Stack_Appender(Stack<T>* stack) : _stack(stack), _from_index(stack->_size) {}
    };
    
    template<class T> nodisc
    T* const data(Stack_Appender<T> const& appender)
    {
        return data(appender._stack) + appender._from_index;
    }

    template<class T> nodisc
    T* data(Stack_Appender<T>* appender)
    {
        return data(appender->_stack) + appender->_from_index;
    }

    template<class T> nodisc
    isize size(Stack_Appender<T> const& appender)
    {
        return size(*appender._stack) - appender._from_index;
    }
    
    template<class T> nodisc
    isize capacity(Stack_Appender<T> const& appender)
    {
        return capacity(*appender._stack) - appender._from_index;
    }

    template<class T> nodisc
    Slice<const T> slice(Stack_Appender<T> const& appender) 
    {
        return slice(slice(*appender._stack), appender._from_index);
    }

    template<class T> nodisc
    Slice<T> slice(Stack_Appender<T>* appender) 
    {
        return slice(slice(appender->_stack), appender->_from_index);
    }

    template <class T, class Inserted> nodisc
    State push_multiple(Stack_Appender<T>* appender, Inserted && inserted)
    {
        return push_multiple(appender->_stack, cast(Inserted&&) inserted);
    }

    template<class T> nodisc
    State push(Stack_Appender<T>* appender, no_infer(T) what)
    {
        return push(appender->_stack, move(&what));
    }
    
    template<class T> nodisc 
    State resize(Stack_Appender<T>* appender, isize to)
    {
        return resize(appender->_stack, to + appender->_from_index);
    }

    template<class T> nodisc 
    State resize_for_overwrite(Stack_Appender<T>* appender, isize to)
    {
        return resize_for_overwrite(appender->_stack, to + appender->_from_index);
    }

    template <class T> nodisc
    State resize(Stack_Appender<T>* appender, isize to, no_infer(T) const& fillWith)
    {
        return resize(appender->_stack, to + appender->_from_index, fillWith);
    }
}


#include "undefs.h"