#pragma once

//#include <iterator>
#include "stack_base.h"
#include "panic.h"
#include "defines.h"

namespace jot
{
    inline const u8 NULL_TERMINATION_ARR[8] = {'\0'};
    
    //we need to differentiate between string types and other types so that we can
    // properly null terminate the Stack (Stack is also used as a string type)
    template <typename T>
    struct String_Character_Type 
    { 
        static constexpr bool is_string_char = false; 
    };

    template<typename T>
    static constexpr bool is_string_char = String_Character_Type<T>::is_string_char;

    //Contiguos dynamic array.
    //Acts in exactly the same as std::vector but is not freely copyable and comaprable
    // (since those ops are costly). Use the appropriate 'copy' and 'compare' functions instead.
    template <typename T>
    struct Stack
    {   
        T* _data = is_string_char<T> ? cast(T*) cast(void*) NULL_TERMINATION_ARR : nullptr;
        isize _size = 0;
        isize _capacity = 0;
        Allocator* _allocator = memory_globals::default_allocator();

        Stack() noexcept = default;
        template<typename C, std::enable_if_t<is_string_char<T> && std::is_same_v<C,T>, bool> cond = true>
        Stack(const C* str);

        Stack(Allocator* allocator) noexcept
            : _allocator{allocator} {}

        Stack(Stack && other) noexcept;
        Stack(Stack const& other);

        ~Stack() noexcept;
        Stack& operator=(Stack && other) noexcept;
        Stack& operator=(Stack const& other);
        
        #define DATA _data
        #define SIZE _size
        #include "slice_members_text.h"
    };

    template<class T> nodisc
    const T* data(Stack<T> const& stack)
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
    isize size(Stack<T>* stack)
    {
        return stack->_size;
    }
    
    template<class T> nodisc
    isize capacity(Stack<T> const& stack)
    {
        return stack._capacity;
    }

    template<class T> nodisc
    isize capacity(Stack<T>* stack)
    {
        return stack->_capacity;
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
        bool size_inv = stack._capacity >= stack._size;
        bool capa_inv = stack._capacity >= 0; 
        bool data_inv = true;
        bool espcape_inv = true;

        if constexpr(is_string_char<T>)
            espcape_inv = stack._data != nullptr && stack._data[stack._size] == T(0);
        else
            data_inv = (stack._capacity == 0) == (stack._data == nullptr);

        return size_inv && capa_inv && data_inv && espcape_inv;
    }
        
    namespace stack_internal 
    {
        template<class T> 
        void null_terminate(Stack<T>* stack) noexcept
        {
            if constexpr(is_string_char<T>)
            {
                assert(stack->_capacity >= stack->_size + 1 && "there must eb enough space for the null termination!");
                stack->_data[stack->_size] = T(0);
            }
        }

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

            if(result.state != Allocator_State::OK)
                return result.state;

            stack->_data = result.items.data;
            stack->_capacity = new_capacity;
            stack_internal::null_terminate(stack);
            return Allocator_State::OK;
        }
    }

    template<class T>
    Allocator_State_Type reserve_failing(Stack<T>* stack, isize to_fit) noexcept
    {
        isize null_terminate_space = cast(isize) is_string_char<T>;
        if (stack->_capacity >= to_fit + null_terminate_space)
            return Allocator_State::OK;
            
        assert(is_invariant(*stack));
        isize realloc_to = calculate_stack_growth(stack->_size, to_fit + null_terminate_space);
        Allocator_State_Type state = stack_internal::set_capacity(stack, realloc_to);
        assert(is_invariant(*stack));
        return state;
    }

    template<class T>
    void reserve(Stack<T>* stack, isize to_fit)
    {
        Allocator_State_Type state = reserve_failing(stack, to_fit);
        force(state == Allocator_State::OK && "allocation failed!");
    }

    template<class T>
    void copy(Stack<T>* to, Slice<const T> from)
    {
        if(data(to) == from.data)
            return;

        reserve(to, from.size);
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
                copy_construct_at(to_data + i, from_data[i]);

            //copy rest
            for (isize i = 0; i < to_size; i++)
                to_data[i] =  from_data[i];
        }
            
        to->_size = from.size;
        stack_internal::null_terminate(to);
        assert(is_invariant(*to));
    }
    
    template<class T> nodisc
    Stack<T> own(Slice<const T> from, memory_globals::Default_Alloc alloc = {})
    {
        Stack<T> out(alloc.val);
        copy(&out, from);
        return out;
    }
    
    template<class T> nodisc
    Stack<T> own(Slice<T> from, memory_globals::Default_Alloc alloc = {})
    {
        Stack<T> out(alloc);
        copy(&out, Slice<const T>(from));
        return out;
    }

    template<class T> nodisc
    Stack<T> own_scratch(Slice<const T> from, memory_globals::Default_Alloc alloc = {})
    {
        return own(from, alloc.val);
    }
    
    template<class T> nodisc
    Stack<T> own_scratch(Slice<T> from, memory_globals::Default_Alloc alloc = {})
    {
        return own(from, alloc.val);
    }

    template <typename T>
    struct Swappable<Stack<T>>
    {
        static
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
        copy(this, slice(other));
    }

    template<typename T>
    Stack<T>& Stack<T>::operator=(Stack<T> const& other) 
    {
        copy(this, slice(other));
        return *this;
    }
    
    template<typename T>
    template<typename C, std::enable_if_t<is_string_char<T> && std::is_same_v<C,T>, bool> cond>
    Stack<T>::Stack(const C* str)
    {
        copy(this, slice(str));
    }

    template<class T> nodisc
    bool is_empty(Stack<T> const& stack) noexcept
    {
        assert(is_invariant(stack));
        return stack._size == 0;
    }
    
    template<class T>
    void push(Stack<T>* stack, no_infer(T) what)
    {
        assert(is_invariant(*stack));

        reserve(stack, stack->_size + 1);
        
        T* ptr = stack->_data + stack->_size;
        construct_at(ptr, move(&what));
        stack->_size++;
        
        stack_internal::null_terminate(stack);
        assert(is_invariant(*stack));
    }

    template<class T> 
    T pop(Stack<T>* stack)
    {
        assert(is_invariant(*stack));
        assert(stack->_size != 0);

        stack->_size--;

        T ret = move(&stack->_data[stack->_size]);
        stack->_data[stack->_size].~T();
        
        stack_internal::null_terminate(stack);
        assert(is_invariant(*stack));
        return ret;
    }

    template <class T>
    void push_multiple(Stack<T>* stack, Slice<const T> inserted)
    {
        reserve(stack, stack->_size + inserted.size);

        assert(stack->_capacity >= stack->_size + inserted.size && "after reserve there should be enough size");
        Slice<T> availible_slice = {stack->_data + stack->_size, inserted.size};
        if constexpr(is_byte_copyable<T>)
            copy_items(availible_slice, inserted);
        else
        {
            for(isize i = 0; i < inserted.size; i++)
                construct_at(availible_slice.data + i, inserted.data[i]);
        }
        
        stack->_size += inserted.size;
        stack_internal::null_terminate(stack);
        assert(is_invariant(*stack));
    }
    
    template <class T>
    void push_multiple_move(Stack<T>* stack, Slice<T> inserted)
    {
        reserve(stack, stack->_size + inserted.size);

        assert(stack->_capacity >= stack->_size + inserted.size && "after reserve there should be enough size");
        Slice<T> availible_slice = {stack->_data + stack->_size, inserted.size};
        
        if constexpr(is_byte_copyable<T>)
            move_items(availible_slice, inserted);
        else
        {
            for(isize i = 0; i < inserted.size; i++)
                construct_at(availible_slice.data + i, move(inserted.data + i));
        }

        stack->_size += inserted.size;
        stack_internal::null_terminate(stack);
        assert(is_invariant(*stack));
    }

    template<class T> 
    void pop_multiple(Stack<T>* stack, isize count)
    {
        assert(count <= size(*stack));
        stack_internal::destroy_items(stack, stack->_size - count, stack->_size);

        stack->_size -= count;
        stack_internal::null_terminate(stack);
        assert(is_invariant(*stack));
    }
    
    template<class T> 
    void clear(Stack<T>* stack)
    {
        pop_multiple(stack, stack->_size);
        assert(is_invariant(*stack));
    }

    template <class T, bool is_zero = false>
    void resize(Stack<T>* stack, isize to, no_infer(T) const& fillWith) noexcept
    {
        assert(is_invariant(*stack));
        assert(0 <= to);

        reserve(stack, to);

        if constexpr(is_byte_nullable<T> && is_zero)
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
                    copy_construct_at(stack->_data + i, fillWith);
            }

            stack_internal::destroy_items(stack, to, stack->_size);
        }

        stack->_size = to;
        stack_internal::null_terminate(stack);
        assert(is_invariant(*stack));
    }

    template<class T> 
    void resize(Stack<T>* stack, isize to)
    {
        resize<T, true>(stack, to, T());
    }

    template<class T> 
    void resize_for_overwrite(Stack<T>* stack, isize to)
    {
        if constexpr(std::is_trivially_constructible_v<T> == false)
            return resize(stack, to);

        reserve(stack, to);
        stack->_size = to;
        stack_internal::null_terminate(stack);
        assert(is_invariant(*stack));
    }

    template<class T> 
    void insert(Stack<T>* stack, isize at, no_infer(T) what)
    {
        assert(0 <= at && at <= stack->_size);
        if(at >= size(*stack))
            return push(stack, move(&what));
            
        reserve(stack, stack->_size + 1);

        construct_at(last(stack) + 1, move(last(stack)));

        Slice<T> move_from = slice_range(slice(stack), at, stack->_size - 1);
        Slice<T> move_to = slice_range(slice(stack), at + 1, stack->_size);
        move_items(move_to, move_from);

        stack->_data[at] = move(&what);
        stack->_size += 1;
        stack_internal::null_terminate(stack);
        assert(is_invariant(*stack));
    }

    template<class T> 
    T remove(Stack<T>* stack, isize at)
    {
        assert(0 <= at && at < stack->_size);
        assert(stack->_size > 0);
        
        T removed = move(&stack->_data[at]);
        
        Slice<T> move_from = slice_range(slice(stack), at + 1, stack->_size);
        Slice<T> move_to = slice_range(slice(stack), at, stack->_size - 1);
        move_items(move_to, move_from);

        T* last_ = last(stack);
        last_->~T();
        stack->_size -= 1;
        stack_internal::null_terminate(stack);
        assert(is_invariant(*stack));
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

    template<class T>
    void unordered_insert(Stack<T>* stack, isize at, no_infer(T) what)
    {
        assert(0 <= at && at <= stack->_size);

        push(stack, move(&what));
        swap(&stack->_data[at], last(stack));
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
    isize size(Stack_Appender<T>* appender)
    {
        return size(*appender);
    }
    
    template<class T> nodisc
    isize capacity(Stack_Appender<T> const& appender)
    {
        return capacity(*appender._stack) - appender._from_index;
    }

    template<class T> nodisc
    isize capacity(Stack_Appender<T>* appender)
    {
        return capacity(*appender);
    }

    template<class T> nodisc
    Slice<const T> slice(Stack_Appender<T> const& appender) 
    {
        return tail(slice(*appender._stack), appender._from_index);
    }

    template<class T> nodisc
    Slice<T> slice(Stack_Appender<T>* appender) 
    {
        return tail(slice(appender->_stack), appender->_from_index);
    }

    template <class T>
    void push_multiple(Stack_Appender<T>* appender, Slice<const T> inserted)
    {
        return push_multiple(appender->_stack, inserted);
    }
    
    template <class T>
    void push_multiple_move(Stack_Appender<T>* appender, Slice<T> inserted)
    {
        return push_multiple_move(appender->_stack, inserted);
    }

    template<class T>
    void push(Stack_Appender<T>* appender, no_infer(T) what)
    {
        return push(appender->_stack, move(&what));
    }
    
    template<class T> 
    void reserve(Stack_Appender<T>* appender, isize to)
    {
        return reserve(appender->_stack, to + appender->_from_index);
    }
    
    template<class T> nodisc
    Allocator_State_Type reserve_failing(Stack_Appender<T>* appender, isize to) noexcept
    {
        return reserve_failing(appender->_stack, to + appender->_from_index);
    }

    template<class T> 
    void resize(Stack_Appender<T>* appender, isize to)
    {
        return resize(appender->_stack, to + appender->_from_index);
    }

    template<class T> 
    void resize_for_overwrite(Stack_Appender<T>* appender, isize to)
    {
        return resize_for_overwrite(appender->_stack, to + appender->_from_index);
    }

    template <class T>
    void resize(Stack_Appender<T>* appender, isize to, no_infer(T) const& fillWith)
    {
        return resize(appender->_stack, to + appender->_from_index, fillWith);
    }
}


#include "undefs.h"