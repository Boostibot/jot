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
    struct Array
    {   
        using T = T_;

        Allocator* _allocator = default_allocator();
        T* _data = nullptr;
        isize _size = 0;
        isize _capacity = 0;
        
        Array(Allocator* allocator = default_allocator()) noexcept;
        Array(Array && other) noexcept;
        Array(Array const& other);
        ~Array() noexcept;

        Array& operator=(Array && other) noexcept;
        Array& operator=(Array const& other);
        
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
    template<class T> auto data(Array<T> const& stack) noexcept -> const T*          {return stack._data;}
    template<class T> auto data(Array<T>* stack) noexcept -> T*                      {return stack->_data;}
    template<class T> auto size(Array<T> const& stack) noexcept -> isize             {return stack._size;}
    template<class T> auto size(Array<T>* stack) noexcept -> isize                   {return stack->_size;}
    template<class T> auto capacity(Array<T> const& stack) noexcept -> isize         {return stack._capacity;}
    template<class T> auto capacity(Array<T>* stack) noexcept -> isize               {return stack->_capacity;}
    template<class T> auto allocator(Array<T> const& stack) noexcept -> Allocator*   {return stack._allocator;}
    template<class T> auto allocator(Array<T>* stack) noexcept -> Allocator*         {return stack->_data;}

    ///Returns a slice containing all items of the stack
    template<class T> auto slice(Array<T> const& stack) noexcept -> Slice<const T>   {return {stack._data, stack._size};}
    template<class T> auto slice(Array<T>* stack) noexcept -> Slice<T>               {return {stack->_data, stack->_size};}

    ///Get first and last items of a stack. Cannot be used on empty stack!
    template<class T> T*       last(Array<T>* stack) noexcept        { return &(*stack)[stack->_size - 1]; }
    template<class T> T const& last(Array<T> const& stack) noexcept  { return (stack)[stack._size - 1];}
    template<class T> T*       first(Array<T>* stack) noexcept       { return &(*stack)[0];}
    template<class T> T const& first(Array<T> const& stack) noexcept { return (stack)[0];}
    
    ///Returns true i
    template<class T> auto is_invariant(Array<T> const& stack) noexcept -> bool;
    template<class T> auto is_empty(Array<T> const& stack) noexcept -> bool;

    ///swaps contents of left and right stacks
    template<class T> void swap(Array<T>* left, Array<T>* right) noexcept;
    ///Copies items to stack. Items within the stack before this opperations are discarded
    template<class T> void copy(Array<T>* stack, Slice<const T> items);
    ///Removes all items from stack
    template<class T> void clear(Array<T>* stack) noexcept;

    ///Makes a new stack with copied items using the probided allocator
    template<class T> Array<T> own(Slice<const T> from, Allocator* alloc = default_allocator());
    template<class T> Array<T> own(Slice<T> from, Allocator* alloc = default_allocator());
    template<class T> Array<T> own_scratch(Slice<const T> from, Allocator* alloc = scratch_allocator()) {return own(from, alloc);}
    template<class T> Array<T> own_scratch(Slice<T> from, Allocator* alloc = scratch_allocator())       {return own(from, alloc);}

    ///Reallocates stack to the specified capacity. If the capacity is smaller then its size, shrinks it destroying items
    ///in process
    template<class T> NODISCARD bool set_capacity_failing(Array<T>* stack, isize new_capacity) noexcept;
    template<class T> void set_capacity(Array<T>* stack, isize new_capacity);

    ///Potentially reallocates stack so that capacity is at least to_size. If capacity is already greater than to_size
    ///does nothing.
    template<class T> NODISCARD bool reserve_failing(Array<T>* stack, isize to_size) noexcept;
    template<class T> void reserve(Array<T>* stack, isize to_size);

    ///Same as reserve expect when reallocation happens grows according to calculate_stack_growth()
    template<class T> void grow(Array<T>* stack, isize to_fit);

    ///Sets size of stack. If to_size is smaller then stack size trims the stack. If is greater fills the added space with fill_with
    template<class T> void resize(Array<T>* stack, isize to_size, typename Array<T>::T const& fill_with = {}) noexcept;
    ///Sets size of stack. If to_size is smaller then stack size trims the stack. 
    ///If is greater and the T type allows it leaves the space uninitialized
    template<class T> void resize_for_overwrite(Array<T>* stack, isize to);

    ///Adds an item to the end of the stack
    template<class T> void push(Array<T>* stack, typename Array<T>::T what);
    ///Removes an item at the end stack. The stack must not be empty!
    template<class T>    T pop(Array<T>* stack) noexcept;

    ///Pushes all items from inserted slice into the stack
    template<class T> void push_multiple(Array<T>* stack, Slice<const typename Array<T>::T> inserted);
    ///Pushes all items from inserted slice into the stack moving them out of inserted slice
    template<class T> void push_multiple_move(Array<T>* stack, Slice<typename Array<T>::T> inserted);
    ///Pops multiple items from stack. The stack must contain at least count elements!
    template<class T> void pop_multiple(Array<T>* stack, isize count) noexcept;

    ///Inserts an item into the stack so that its index is at. Moves all later elemnts forward one index
    template<class T> void insert(Array<T>* stack, isize at, typename Array<T>::T what);
    ///Removes an item from the stack at specified index. Moves all later elemnts backwards one index. The stack most not be empty!
    template<class T>    T remove(Array<T>* stack, isize at) noexcept;

    ///Inserts an item into the stack so that its index is at. The item that was originally at this index becomes last
    template<class T> void unordered_insert(Array<T>* stack, isize at, typename Array<T>::T what);
    ///Removes an item from the stack at specified index. Moves the last item into freed up spot. The stack most not be empty!
    template<class T>    T unordered_remove(Array<T>* stack, isize at) noexcept;
    
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
        void null_terminate(Array<T>* stack) noexcept
        {
            if constexpr(is_string_char<T>)
                stack->_data[stack->_size] = T();
        }
        
        template<class T> 
        void set_data_to_termination(Array<T>* stack)
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
    
    template<class T>
    bool set_capacity_failing(Array<T>* stack, isize new_capacity) noexcept
    {
        assert(is_invariant(*stack));

        isize old_byte_cap = stack->_capacity * (isize) sizeof(T);
        isize new_byte_cap = new_capacity * (isize) sizeof(T);

        bool is_string = is_string_char<T>;
        if(is_string && new_byte_cap != 0)
            new_byte_cap += (isize) sizeof(T);
            
        if(is_string && old_byte_cap != 0)
            old_byte_cap += (isize) sizeof(T);

        void* new_data = nullptr;
        bool state = memory_resize_allocate(stack->_allocator, &new_data, new_byte_cap, 
            stack->_data, old_byte_cap, DEF_ALIGNMENT<T>, GET_LINE_INFO());

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
                memmove(new_data, stack->_data, (size_t) to_size * sizeof(T));
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
        memory_resize_deallocate(stack->_allocator, &new_data, new_byte_cap, 
            stack->_data, old_byte_cap, DEF_ALIGNMENT<T>, GET_LINE_INFO());

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

    template<typename T>
    Array<T>::Array(Allocator* allocator) noexcept
    {
        stack_internal::set_data_to_termination(this);
        _allocator = allocator;
    }

    template<typename T>
    Array<T>::~Array() noexcept 
    {
        if(_capacity != 0)
        {
            stack_internal::destruct_items(_data, 0, _size);
            isize cap = _capacity + (isize) is_string_char<T>;
            _allocator->deallocate(_data, cap * (isize) sizeof(T), DEF_ALIGNMENT<T>, GET_LINE_INFO());
        }
    }

    template<typename T>
    Array<T>::Array(Array && other) noexcept 
    {
        stack_internal::set_data_to_termination(this);
        *this = (Array&&) other;
    }

    template<typename T>
    Array<T>& Array<T>::operator=(Array<T> && other) noexcept 
    {
        swap(this, &other);
        return *this;
    }
    
    template<typename T>
    Array<T>::Array(Array const& other) 
    {
        stack_internal::set_data_to_termination(this);
        copy(this, slice(other));
    }

    template<typename T>
    Array<T>& Array<T>::operator=(Array<T> const& other) 
    {
        copy(this, slice(other));
        return *this;
    }

    template<class T>
    bool is_invariant(Array<T> const& stack) noexcept
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
    bool reserve_failing(Array<T>* stack, isize to_size) noexcept
    {
        if (stack->_capacity >= to_size)
            return true;
            
        return set_capacity_failing(stack, to_size);
    }
    
    template<class T>
    void set_capacity(Array<T>* stack, isize new_capacity)
    {
        if(set_capacity_failing(stack, new_capacity) == false)
        {
            const char* alloc_name = stack->_allocator->get_stats().name;
            PANIC(panic_cformat, "Array<T> allocation failed! "
                "Attempted to allocated %t bytes from allocator %s"
                "Array: {size: %t, capacity: %t} sizeof(T): %z",
                new_capacity*sizeof(T), alloc_name ? alloc_name : "<No alloc name>", 
                stack->_size, stack->_capacity, sizeof(T));
        }
    }

    template<class T>
    void reserve(Array<T>* stack, isize to_capacity)
    {
        if (stack->_capacity < to_capacity)
            set_capacity(stack, to_capacity);
    }

    template<class T>
    void grow(Array<T>* stack, isize to_fit)
    {
        if (stack->_capacity >= to_fit)
            return;
        
		isize new_capacity = calculate_stack_growth(stack->_capacity, to_fit);
        set_capacity(stack, new_capacity);
    }

    template<class T>
    void copy(Array<T>* to, Slice<const T> from)
    {
        assert(is_invariant(*to));
        reserve(to, from.size);
        
        Slice<T> capacity_slice = {to->_data, to->_capacity};
        //if is byte copyable just copy the contents all in one go
        bool by_byte = JOT_IS_TRIVIALLY_COPYABLE(T);
        if(by_byte)
            memmove(capacity_slice.data, from.data, (size_t) from.size * sizeof(T));
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
    Array<T> own(Slice<const T> from, Allocator* alloc)
    {
        Array<T> out(alloc);
        copy<T>(&out, from);
        return out;
    }
    
    template<class T>
    Array<T> own(Slice<T> from, Allocator* alloc)
    {
        Array<T> out(alloc);
        copy<T>(&out, (Slice<const T>) from);
        return out;
    }
    
    template <typename T>
    void swap(Array<T>* left, Array<T>* right) noexcept
    { 
        swap(&left->_data, &right->_data);
        swap(&left->_size, &right->_size);
        swap(&left->_capacity, &right->_capacity);
        swap(&left->_allocator, &right->_allocator);
    }
    
    template<class T>
    bool is_empty(Array<T> const& stack) noexcept
    {
        assert(is_invariant(stack));
        return stack._size == 0;
    }
    
    template<class T>
    void push(Array<T>* stack, typename Array<T>::T what)
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
    T pop(Array<T>* stack) noexcept
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
    void push_multiple(Array<T>* stack, Slice<const typename Array<T>::T> inserted)
    {
        grow(stack, stack->_size + inserted.size);
        
        bool is_by_byte = JOT_IS_TRIVIALLY_COPYABLE(T);
        T* base = stack->_data + stack->_size;

        if(is_by_byte)
            memmove(base, inserted.data, (size_t) inserted.size * sizeof(T));
        else
        {
            for(isize i = 0; i < inserted.size; i++)
                new(base + i) T(inserted.data[i]);
        }

        stack->_size += inserted.size;
        stack_internal::null_terminate(stack);
        assert(is_invariant(*stack));
    }
    
    template <class T>
    void push_multiple_move(Array<T>* stack, Slice<typename Array<T>::T> inserted)
    {
        grow(stack, stack->_size + inserted.size);
        
        bool is_by_byte = JOT_IS_TRIVIALLY_COPYABLE(T);
        T* base = stack->_data + stack->_size;

        if(is_by_byte)
            memmove(base, inserted.data, (size_t) inserted.size * sizeof(T));
        else
        {
            for(isize i = 0; i < inserted.size; i++)
                new(base + i) T((T&&) inserted.data[i]);
        }

        stack->_size += inserted.size;
        stack_internal::null_terminate(stack);
        assert(is_invariant(*stack));
    }

    template<class T> 
    void pop_multiple(Array<T>* stack, isize count) noexcept
    {
        assert(count <= size(*stack));
        stack_internal::destruct_items(stack->_data, stack->_size - count, stack->_size);

        stack->_size -= count;
        stack_internal::null_terminate(stack);
        assert(is_invariant(*stack));
    }
    
    template<class T> 
    void clear(Array<T>* stack) noexcept
    {
        pop_multiple(stack, stack->_size);
    }

    template <class T>
    void resize(Array<T>* stack, isize to, typename Array<T>::T const& fill_with) noexcept
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
    void resize_for_overwrite(Array<T>* stack, isize to)
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
    void insert(Array<T>* stack, isize at, typename Array<T>::T what)
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
    T remove(Array<T>* stack, isize at) noexcept
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
    T unordered_remove(Array<T>* stack, isize at) noexcept
    {
        assert(0 <= at && at < stack->_size);
        assert(stack->_size > 0);

        swap(&stack->_data[at], last(stack));
        return pop(stack);
    }

    template<class T>
    void unordered_insert(Array<T>* stack, isize at, typename Array<T>::T what)
    {
        assert(0 <= at && at <= stack->_size);

        push(stack, (T&&) what);
        swap(&stack->_data[at], last(stack));
    }
}

namespace std 
{
    template<class T> 
    size_t size(jot::Array<T> const& stack) noexcept
    {
        return jot::size(stack);
    }

    template<class T> 
    void swap(jot::Array<T>& stack1, jot::Array<T>& stack2)
    {
        jot::swap(&stack1, &stack2);
    }
}
