#pragma once

#include "memory.h"
#include "slice.h"
#include "panic.h"

#ifndef NODISCARD
    #define NODISCARD [[nodiscard]]
#endif

namespace jot
{
    ///Resizable dynamic array also used to represent dynamic strings
    template <typename T_>
    struct Stack
    {   
        using T = T_;

        Allocator* _allocator = default_allocator();
        T* _data = nullptr;
        isize _size = 0;
        isize _capacity = 0;
        
        Stack(Allocator* allocator = default_allocator()) noexcept;
        Stack(Stack && other) noexcept;
        Stack(Stack const& other);
        ~Stack() noexcept;

        Stack& operator=(Stack && other) noexcept;
        Stack& operator=(Stack const& other);
        
        using value_type      = T;
        using size_type       = size_t;
        using difference_type = ptrdiff_t;
        using reference       = T&;
        using const_reference = const T&;
        using iterator       = T*;
        using const_iterator = const T*;
        
        constexpr iterator       begin() noexcept       { return _data; }
        constexpr const_iterator begin() const noexcept { return _data; }
        constexpr iterator       end() noexcept         { return _data + _size; }
        constexpr const_iterator end() const noexcept   { return _data + _size; }

        constexpr T const& operator[](isize index) const noexcept  
        { 
            assert(0 <= index && index < _size && "index out of range"); 
            return _data[index]; 
        }
        constexpr T& operator[](isize index) noexcept 
        { 
            assert(0 <= index && index < _size && "index out of range"); 
            return _data[index]; 
        }
    };
    
    ///Calculates new size of a stack which is guaranteed to be greater than to_fit
    inline isize calculate_stack_growth(isize curr_size, isize to_fit, isize growth_num = 3, isize growth_den = 2, isize grow_lin = 8);

    ///Getters 
    template<class T> auto data(Stack<T> const& stack) noexcept -> const T*          {return stack._data;}
    template<class T> auto data(Stack<T>* stack) noexcept -> T*                      {return stack->_data;}
    template<class T> auto size(Stack<T> const& stack) noexcept -> isize             {return stack._size;}
    template<class T> auto size(Stack<T>* stack) noexcept -> isize                   {return stack->_size;}
    template<class T> auto capacity(Stack<T> const& stack) noexcept -> isize         {return stack._capacity;}
    template<class T> auto capacity(Stack<T>* stack) noexcept -> isize               {return stack->_capacity;}
    template<class T> auto allocator(Stack<T> const& stack) noexcept -> Allocator*   {return stack._allocator;}
    template<class T> auto allocator(Stack<T>* stack) noexcept -> Allocator*         {return stack->_data;}

    ///Returns a slice containing all items of the stack
    template<class T> auto slice(Stack<T> const& stack) noexcept -> Slice<const T>   {return {stack._data, stack._size};}
    template<class T> auto slice(Stack<T>* stack) noexcept -> Slice<T>               {return {stack->_data, stack->_size};}

    ///Get first and last items of a stack. Cannot be used on empty stack!
    template<class T> T*       last(Stack<T>* stack) noexcept        { return &(*stack)[stack->_size - 1]; }
    template<class T> T const& last(Stack<T> const& stack) noexcept  { return (stack)[stack._size - 1];}
    template<class T> T*       first(Stack<T>* stack) noexcept       { return &(*stack)[0];}
    template<class T> T const& first(Stack<T> const& stack) noexcept { return (stack)[0];}
    
    ///Returns true i
    template<class T> auto is_invariant(Stack<T> const& stack) noexcept -> bool;
    template<class T> auto is_empty(Stack<T> const& stack) noexcept -> bool;

    ///swaps contents of left and right stacks
    template<class T> void swap(Stack<T>* left, Stack<T>* right) noexcept;
    ///Copies items to stack. Items within the stack before this opperations are discarded
    template<class T> void copy(Stack<T>* stack, Slice<const T> items);
    ///Removes all items from stack
    template<class T> void clear(Stack<T>* stack) noexcept;

    ///Makes a new stack with copied items using the probided allocator
    template<class T> Stack<T> own(Slice<const T> from, Allocator* alloc = default_allocator());
    template<class T> Stack<T> own(Slice<T> from, Allocator* alloc = default_allocator());
    template<class T> Stack<T> own_scratch(Slice<const T> from, Allocator* alloc = scratch_allocator()) {return own(from, alloc);}
    template<class T> Stack<T> own_scratch(Slice<T> from, Allocator* alloc = scratch_allocator())       {return own(from, alloc);}

    ///Reallocates stack to the specified capacity. If the capacity is smaller then its size, shrinks it destroying items
    ///in process
    template<class T> NODISCARD bool set_capacity_failing(Stack<T>* stack, isize new_capacity) noexcept;
    template<class T> void set_capacity(Stack<T>* stack, isize new_capacity);

    ///Potentially reallocates stack so that capacity is at least to_size. If capacity is already greater than to_size
    ///does nothing.
    template<class T> NODISCARD bool reserve_failing(Stack<T>* stack, isize to_size) noexcept;
    template<class T> void reserve(Stack<T>* stack, isize to_size);

    ///Same as reserve expect when reallocation happens grows according to calculate_stack_growth()
    template<class T> void grow(Stack<T>* stack, isize to_fit);

    ///Sets size of stack. If to_size is smaller then stack size trims the stack. If is greater fills the added space with fill_with
    template<class T> void resize(Stack<T>* stack, isize to_size, typename Stack<T>::T const& fill_with = {}) noexcept;
    ///Sets size of stack. If to_size is smaller then stack size trims the stack. 
    ///If is greater and the T type allows it leaves the space uninitialized
    template<class T> void resize_for_overwrite(Stack<T>* stack, isize to);

    ///Adds an item to the end of the stack
    template<class T> void push(Stack<T>* stack, typename Stack<T>::T what);
    ///Removes an item at the end stack. The stack must not be empty!
    template<class T>    T pop(Stack<T>* stack) noexcept;

    ///Pushes all items from inserted slice into the stack
    template<class T> void push_multiple(Stack<T>* stack, Slice<const typename Stack<T>::T> inserted);
    ///Pushes all items from inserted slice into the stack moving them out of inserted slice
    template<class T> void push_multiple_move(Stack<T>* stack, Slice<typename Stack<T>::T> inserted);
    ///Pops multiple items from stack. The stack must contain at least count elements!
    template<class T> void pop_multiple(Stack<T>* stack, isize count) noexcept;

    ///Inserts an item into the stack so that its index is at. Moves all later elemnts forward one index
    template<class T> void insert(Stack<T>* stack, isize at, typename Stack<T>::T what);
    ///Removes an item from the stack at specified index. Moves all later elemnts backwards one index. The stack most not be empty!
    template<class T>    T remove(Stack<T>* stack, isize at) noexcept;

    ///Inserts an item into the stack so that its index is at. The item that was originally at this index becomes last
    template<class T> void unordered_insert(Stack<T>* stack, isize at, typename Stack<T>::T what);
    ///Removes an item from the stack at specified index. Moves the last item into freed up spot. The stack most not be empty!
    template<class T>    T unordered_remove(Stack<T>* stack, isize at) noexcept;
    
    ///Tells stack if this type should be null terminated. Provide specialization for your desired type (see string.h)
    template<class T> static constexpr bool is_string_char = false;
}

namespace jot
{
    namespace stack_internal 
    {
        //@TODO: remove static inline
        static inline const uint8_t NULL_TERMINATION_ARR[8] = {'\0'};

        template<class T> 
        void null_terminate(Stack<T>* stack) noexcept
        {
            if constexpr(is_string_char<T>)
                stack->_data[stack->_size] = T();
        }
        
        template<class T> 
        void set_data_to_termination(Stack<T>* stack)
        {
            if constexpr(is_string_char<T>)
                stack->_data = (T*) (void*) NULL_TERMINATION_ARR;
            else
                stack->_data = nullptr;
        }   
        
        template<typename T> constexpr 
        void destruct_items(T* data, isize from, isize to) noexcept
        {
            bool byte_destruct = JOT_IS_TRIVIALLY_DESTRUCTIBLE(T);
            if(byte_destruct == false)
                for(isize i = from; i < to; i++)
                    data[i].~T();
        }
    }

    template<typename T>
    Stack<T>::Stack(Allocator* allocator) noexcept
    {
        stack_internal::set_data_to_termination(this);
        _allocator = allocator;
    }

    template<typename T>
    Stack<T>::~Stack() noexcept 
    {
        if(_capacity != 0)
        {
            stack_internal::destruct_items(_data, 0, _size);
            isize cap = _capacity + (isize) is_string_char<T>;
            _allocator->deallocate(_data, cap * sizeof(T), DEF_ALIGNMENT<T>, GET_LINE_INFO());
        }
    }

    template<typename T>
    Stack<T>::Stack(Stack && other) noexcept 
    {
        stack_internal::set_data_to_termination(this);
        *this = (Stack&&) other;
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
        stack_internal::set_data_to_termination(this);
        copy(this, slice(other));
    }

    template<typename T>
    Stack<T>& Stack<T>::operator=(Stack<T> const& other) 
    {
        copy(this, slice(other));
        return *this;
    }

    template<class T>
    bool is_invariant(Stack<T> const& stack) noexcept
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
    
    //Returns new size of a stack which is guaranteed to be greater than to_fit
    inline isize calculate_stack_growth(isize curr_size, isize to_fit, isize growth_num, isize growth_den, isize grow_lin)
    {
        //with default values for small sizes grows fatser than classic factor of 2 for big slower
        isize size = curr_size;
        while(size < to_fit)
            size = size * growth_num / growth_den + grow_lin; 

        return size;
    }

    template<class T>
    bool set_capacity_failing(Stack<T>* stack, isize new_capacity) noexcept
    {
        assert(is_invariant(*stack));

        isize actual_new_capacity = /*TODO*/;
        void* new_data = nullptr;
        bool state = resize_allocate(stack->_allocator, &new_data, new_capacity, stack->_data, 
            stack->_capacity, DEF_ALIGNMENT<T>, GET_LINE_INFO());
        if(state == false)
            return false;

        isize to_size = stack->_size;
        if(new_capacity < to_size)
            to_size = new_capacity;

        if(new_data != stack->_data)
        {
            T* new_data_t = (T*) new_data; 
            bool byte_trasnfer = JOT_IS_REALLOCATABLE(T);
            if(byte_trasnfer)
                memmove(new_data, stack->_data, to_size * sizeof(T));
            else
            {
                for(isize i = 0; i < to_size; i++)
                {
                    new(new_data_t + i) T((T&&) stack->_data[i]);
                    stack->_data[i].~T();
                }
            }
        }

        stack_internal::destruct_items(stack->_data, new_capacity, stack->_size);
        resize_deallocate(stack->_allocator, &new_data, new_capacity, stack->_data, stack->_capacity, DEF_ALIGNMENT<T>, GET_LINE_INFO());

        stack->_size = to_size;
        stack->_data = (T*) new_data;
        stack->_capacity = new_capacity;

        if(stack->_capacity == 0)
            stack_internal::set_data_to_termination(stack);
        else
            stack_internal::null_terminate(stack);

        assert(is_invariant(*stack));
        return true;
    }
     
    template<class T>
    bool reserve_failing(Stack<T>* stack, isize to_size) noexcept
    {
        if (stack->_capacity >= to_size)
            return true;
            
        return set_capacity_failing(stack, to_size);
    }
    
    template<class T>
    void set_capacity(Stack<T>* stack, isize new_capacity)
    {
        if(set_capacity_failing(stack, new_capacity) == false)
            PANIC(panic_cformat, "Stack<T> allocation failed! ");
    }

    template<class T>
    void reserve(Stack<T>* stack, isize to_capacity)
    {
        if (stack->_capacity < to_capacity)
            set_capacity(stack, to_capacity);
    }

    template<class T>
    void grow(Stack<T>* stack, isize to_fit)
    {
        if (stack->_capacity >= to_fit)
            return;
        
		isize new_capacity = calculate_stack_growth(stack->_capacity, to_fit);
        set_capacity(stack, new_capacity);
    }

    template<class T>
    void copy(Stack<T>* to, Slice<const T> from)
    {
        assert(is_invariant(*to));
        reserve(to, from.size);
        
        Slice<T> capacity_slice = {to->_data, to->_capacity};
        //if is byte copyable just copy the contents all in one go
        bool by_byte = JOT_IS_TRIVIALLY_COPYABLE(T);
        if(by_byte)
            memmove(capacity_slice.data, from.data, from.size * sizeof(T));
        //else copy then copy construct the rest
        else
        {
            isize copy_to = to->_size;
            if(copy_to > from.size)
                copy_to = from.size;

            for(isize i = 0; i < copy_to; i++)
                capacity_slice[i] = from[i];

            for(isize i = copy_to; i < from.size; i++)
                new(&capacity_slice[i]) T(from[i]);
        }

        stack_internal::destruct_items(to->_data, from.size, to->_size);

        to->_size = from.size;
        if(to->_capacity == 0)
            stack_internal::set_data_to_termination(to);
        else
            stack_internal::null_terminate(to);

        assert(is_invariant(*to));
    }
    
    template<class T>
    Stack<T> own(Slice<const T> from, Allocator* alloc)
    {
        Stack<T> out(alloc);
        copy<T>(&out, from);
        return out;
    }
    
    template<class T>
    Stack<T> own(Slice<T> from, Allocator* alloc)
    {
        Stack<T> out(alloc);
        copy<T>(&out, (Slice<const T>) from);
        return out;
    }
    
    template <typename T>
    void swap(Stack<T>* left, Stack<T>* right) noexcept
    { 
        swap(&left->_data, &right->_data);
        swap(&left->_size, &right->_size);
        swap(&left->_capacity, &right->_capacity);
        swap(&left->_allocator, &right->_allocator);
    }
    
    template<class T>
    bool is_empty(Stack<T> const& stack) noexcept
    {
        assert(is_invariant(stack));
        return stack._size == 0;
    }
    
    template<class T>
    void push(Stack<T>* stack, typename Stack<T>::T what)
    {
        assert(is_invariant(*stack));

        grow(stack, stack->_size + 1);
        
        T* ptr = stack->_data + stack->_size;
        new(ptr) T((T&&) what);
        stack->_size++;
        
        stack_internal::null_terminate(stack);
        assert(is_invariant(*stack));
    }

    template<class T> 
    T pop(Stack<T>* stack) noexcept
    {
        assert(is_invariant(*stack));
        assert(stack->_size != 0);

        stack->_size--;

        T ret = (T&&) (stack->_data[stack->_size]);
        stack->_data[stack->_size].~T();
        
        stack_internal::null_terminate(stack);
        assert(is_invariant(*stack));
        return ret;
    }

    template <class T>
    void push_multiple(Stack<T>* stack, Slice<const typename Stack<T>::T> inserted)
    {
        grow(stack, stack->_size + inserted.size);
        
        bool is_by_byte = JOT_IS_TRIVIALLY_COPYABLE(T);
        if(is_by_byte)
            memmove(stack->_data, inserted.data, inserted.size * sizeof(T));
        else
        {
            for(isize i = 0; i < inserted.size; i++)
                new(stack->_data + i) T(inserted.data[i]);
        }

        stack->_size += inserted.size;
        stack_internal::null_terminate(stack);
        assert(is_invariant(*stack));
    }
    
    template <class T>
    void push_multiple_move(Stack<T>* stack, Slice<typename Stack<T>::T> inserted)
    {
        grow(stack, stack->_size + inserted.size);
        
        bool is_by_byte = JOT_IS_TRIVIALLY_COPYABLE(T);
        if(is_by_byte)
            memmove(stack->_data, inserted.data, inserted.size * sizeof(T));
        else
        {
            for(isize i = 0; i < inserted.size; i++)
                new(stack->_data + i) T((T&&) inserted.data[i]);
        }

        stack->_size += inserted.size;
        stack_internal::null_terminate(stack);
        assert(is_invariant(*stack));
    }

    template<class T> 
    void pop_multiple(Stack<T>* stack, isize count) noexcept
    {
        assert(count <= size(*stack));
        stack_internal::destruct_items(stack->_data, stack->_size - count, stack->_size);

        stack->_size -= count;
        stack_internal::null_terminate(stack);
        assert(is_invariant(*stack));
    }
    
    template<class T> 
    void clear(Stack<T>* stack) noexcept
    {
        pop_multiple(stack, stack->_size);
    }

    template <class T>
    void resize(Stack<T>* stack, isize to, typename Stack<T>::T const& fill_with) noexcept
    {
        assert(is_invariant(*stack));
        assert(0 <= to);

        reserve(stack, to);
        
        for (isize i = stack->_size; i < to; i++)
            new(stack->_data + i) T(fill_with);

        stack_internal::destruct_items(stack->_data, to, stack->_size);
        
        stack->_size = to;
        stack_internal::null_terminate(stack);
        assert(is_invariant(*stack));
    }

    template<class T> 
    void resize_for_overwrite(Stack<T>* stack, isize to)
    {
        bool is_by_byte = JOT_IS_TRIVIALLY_COPYABLE(T);
        if(is_by_byte)
            return resize(stack, to);
        else
        {
            stack->_size = to;
            stack_internal::null_terminate(stack);
            assert(is_invariant(*stack));
            reserve(stack, to);
        }
    }

    template<class T> 
    void insert(Stack<T>* stack, isize at, typename Stack<T>::T what)
    {
        assert(0 <= at && at <= stack->_size);
        if(at >= size(*stack))
            return push(stack, (T&&) what);
            
        grow(stack, stack->_size + 1);

        new(last(stack) + 1) T((T&&) *last(stack));

        Slice<T> move_from = slice_range(slice(stack), at, stack->_size - 1);
        Slice<T> move_to = slice_range(slice(stack), at + 1, stack->_size);
        move_items(move_to, move_from);

        stack->_data[at] = (T&&) what;
        stack->_size += 1;
        stack_internal::null_terminate(stack);
        assert(is_invariant(*stack));
    }

    template<class T> 
    T remove(Stack<T>* stack, isize at) noexcept
    {
        assert(0 <= at && at < stack->_size);
        assert(stack->_size > 0);
        
        T removed = (T&&) stack->_data[at];
        
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
    T unordered_remove(Stack<T>* stack, isize at) noexcept
    {
        assert(0 <= at && at < stack->_size);
        assert(stack->_size > 0);

        swap(&stack->_data[at], last(stack));
        return pop(stack);
    }

    template<class T>
    void unordered_insert(Stack<T>* stack, isize at, typename Stack<T>::T what)
    {
        assert(0 <= at && at <= stack->_size);

        push(stack, (T&&) what);
        swap(&stack->_data[at], last(stack));
    }
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
