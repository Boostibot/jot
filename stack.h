#pragma once

#include "memory.h"
#include "slice_ops.h"
#include "panic.h"
#include "defines.h"

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
        
        nodisc constexpr iterator       begin() noexcept       { return _data; }
        nodisc constexpr const_iterator begin() const noexcept { return _data; }
        nodisc constexpr iterator       end() noexcept         { return _data + _size; }
        nodisc constexpr const_iterator end() const noexcept   { return _data + _size; }

        nodisc constexpr T const& operator[](isize index) const noexcept  
        { 
            assert(0 <= index && index < _size && "index out of range"); 
            return _data[index]; 
        }
        nodisc constexpr T& operator[](isize index) noexcept 
        { 
            assert(0 <= index && index < _size && "index out of range"); 
            return _data[index]; 
        }
    };
    
    ///Calculates new size of a stack which is guaranteed to be greater than to_fit
    nodisc constexpr inline
    isize calculate_stack_growth(isize curr_size, isize to_fit, isize growth_num = 3, isize growth_den = 2, isize grow_lin = 8);

    ///Getters 
    template<class T> nodisc auto data(Stack<T> const& stack) noexcept -> const T*          {return stack._data;}
    template<class T> nodisc auto data(Stack<T>* stack) noexcept -> T*                      {return stack->_data;}
    template<class T> nodisc auto size(Stack<T> const& stack) noexcept -> isize             {return stack._size;}
    template<class T> nodisc auto size(Stack<T>* stack) noexcept -> isize                   {return stack->_size;}
    template<class T> nodisc auto capacity(Stack<T> const& stack) noexcept -> isize         {return stack._capacity;}
    template<class T> nodisc auto capacity(Stack<T>* stack) noexcept -> isize               {return stack->_capacity;}
    template<class T> nodisc auto allocator(Stack<T> const& stack) noexcept -> Allocator*   {return stack._allocator;}
    template<class T> nodisc auto allocator(Stack<T>* stack) noexcept -> Allocator*         {return stack->_data;}

    ///Returns a slice containing all items of the stack
    template<class T> nodisc auto slice(Stack<T> const& stack) noexcept -> Slice<const T>   {return {stack._data, stack._size};}
    template<class T> nodisc auto slice(Stack<T>* stack) noexcept -> Slice<T>               {return {stack->_data, stack->_size};}

    ///Get first and last items of a stack. Cannot be used on empty stack!
    template<class T> nodisc auto last(Stack<T>* stack) noexcept -> T*;
    template<class T> nodisc auto last(Stack<T> const& stack) noexcept -> T const&;
    template<class T> nodisc auto first(Stack<T>* stack) noexcept -> T*;
    template<class T> nodisc auto first(Stack<T> const& stack) noexcept -> T const&;
    
    ///Returns true i
    template<class T> nodisc auto is_invariant(Stack<T> const& stack) noexcept -> bool;
    template<class T> nodisc auto is_empty(Stack<T> const& stack) noexcept -> bool;

    ///swaps contents of left and right stacks
    template<class T>        void swap(Stack<T>* left, Stack<T>* right) noexcept;
    ///Copies items to stack. Items within the stack before this opperations are discarded
    template<class T>        void copy(Stack<T>* stack, Slice<const T> items);
    ///Removes all items from stack
    template<class T>        void clear(Stack<T>* stack);

    ///Makes a new stack with copied items using the probided allocator
    template<class T> nodisc auto own(Slice<const T> from, Allocator* alloc = default_allocator()) -> Stack<T>;
    template<class T> nodisc auto own(Slice<T> from, Allocator* alloc = default_allocator()) -> Stack<T>;
    template<class T> nodisc auto own_scratch(Slice<const T> from, Allocator* alloc = scratch_allocator()) -> Stack<T> {return own(from, alloc);}
    template<class T> nodisc auto own_scratch(Slice<T> from, Allocator* alloc = scratch_allocator()) -> Stack<T>       {return own(from, alloc);}

    ///Reallocates stack to the specified capacity. If the capacity is smaller then its size, shrinks it destroying items
    ///in process
    template<class T> nodisc auto set_capacity_failing(Stack<T>* stack, isize new_capacity) noexcept -> Allocation_State;
    template<class T>        void set_capacity(Stack<T>* stack, isize new_capacity);

    ///Potentially reallocates stack so that capacity is at least to_size. If capacity is already greater than to_size
    ///does nothing.
    template<class T> nodisc auto reserve_failing(Stack<T>* stack, isize to_size) noexcept -> Allocation_State;
    template<class T>        void reserve(Stack<T>* stack, isize to_size);

    ///Same as reserve expect when reallocation happens grows according to calculate_stack_growth()
    template<class T>        void grow(Stack<T>* stack, isize to_fit);

    ///Sets size of stack. If to_size is smaller then stack size trims the stack. If is greater fills the added space with fill_with
    template<class T>        void resize(Stack<T>* stack, isize to_size, typename Stack<T>::T const& fill_with) noexcept;
    ///Sets size of stack. If to_size is smaller then stack size trims the stack. If is greater fills the added space with T()
    template<class T>        void resize(Stack<T>* stack, isize to_size);
    ///Sets size of stack. If to_size is smaller then stack size trims the stack. 
    ///If is greater and the T type allows it leaves the space uninitialized
    template<class T>        void resize_for_overwrite(Stack<T>* stack, isize to);

    ///Adds an item to the end of the stack
    template<class T>        void push(Stack<T>* stack, typename Stack<T>::T what);
    ///Removes an item at the end stack. The stack must not be empty!
    template<class T>           T pop(Stack<T>* stack) noexcept;

    ///Pushes all items from inserted slice into the stack
    template<class T>        void push_multiple(Stack<T>* stack, Slice<const typename Stack<T>::T> inserted);
    ///Pushes all items from inserted slice into the stack moving them out of inserted slice
    template<class T>        void push_multiple_move(Stack<T>* stack, Slice<typename Stack<T>::T> inserted);
    ///Pops multiple items from stack. The stack must contain at least count elements!
    template<class T>        void pop_multiple(Stack<T>* stack, isize count) noexcept;

    ///Inserts an item into the stack so that its index is at. Moves all later elemnts forward one index
    template<class T>        void insert(Stack<T>* stack, isize at, typename Stack<T>::T what);
    ///Removes an item from the stack at specified index. Moves all later elemnts backwards one index. The stack most not be empty!
    template<class T>           T remove(Stack<T>* stack, isize at) noexcept;

    ///Inserts an item into the stack so that its index is at. The item that was originally at this index becomes last
    template<class T>        void unordered_insert(Stack<T>* stack, isize at, typename Stack<T>::T what);
    ///Removes an item from the stack at specified index. Moves the last item into freed up spot. The stack most not be empty!
    template<class T>           T unordered_remove(Stack<T>* stack, isize at) noexcept;
    
    ///Pair of functions for dealing with any stacks without having to construct Stack<T> directly.
    ///Can be used when we have SOA struct with many arrays whose size and capacity is the same yet we dont want to keep
    /// each in its own separate Stack because that would be space inefficient (repeated size, capacity and allocator ptr).
    ///As such we can inside the SOA structs realloc function construct set of temporary Mini_Stack's and call
    /// set_capacity_allocate on each, check if any failed and if yes undo the allocation and throw, else call
    /// set_capacity_deallocate on each and copy data from mini stacks back to appropriate struct fields
    struct Set_Capacity_Info;

    template<class T> nodisc
    Allocation_State set_capacity_allocate(Slice<T>* new_slice, isize* new_size, Slice<T> old_slice, isize old_size, Set_Capacity_Info info) noexcept;

    template<class T>
    Allocation_State set_capacity_deallocate(Slice<T>* new_slice, isize* new_size, Slice<T> old_slice, isize old_size, Set_Capacity_Info info) noexcept;
    
    ///Tells stack if this type should be null terminated. Provide specialization for your desired type (see string.h)
    template<class T>
    static constexpr bool is_string_char = false;
}

namespace jot
{
    namespace stack_internal 
    {
        static inline const u8 NULL_TERMINATION_ARR[8] = {'\0'};

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
                stack->_data = cast(T*) cast(void*) NULL_TERMINATION_ARR;
            else
                stack->_data = nullptr;
        }   

        template<class T> 
        void destruct(Stack<T>* stack)
        {
            if(stack->_capacity != 0)
            {
                destruct_items(stack->_data, 0, stack->_size);
                Slice<T> old_slice = {stack->_data, stack->_capacity + cast(isize) is_string_char<T>};
                stack->_allocator->deallocate(cast_slice<u8>(old_slice), DEF_ALIGNMENT<T>);
            }
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
        stack_internal::destruct(this);
    }

    template<typename T>
    Stack<T>::Stack(Stack && other) noexcept 
    {
        stack_internal::set_data_to_termination(this);
        *this = cast(Stack&&) other;
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
    T* last(Stack<T>* stack) noexcept
    {
        assert(stack->_size > 0);
        return &stack->_data[stack->_size - 1];
    }

    template<class T>
    T const& last(Stack<T> const& stack) noexcept
    {
        assert(stack._size > 0);
        return stack._data[stack._size - 1];
    }

    template<class T>
    T* first(Stack<T>* stack) noexcept
    {
        assert(stack->_size > 0);
        return &stack->_data[0];
    }

    template<class T>
    T const& first(Stack<T> const& stack) noexcept
    {
        assert(stack._size > 0);
        return stack._data[0];
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
    nodisc constexpr inline
    isize calculate_stack_growth(isize curr_size, isize to_fit, isize growth_num, isize growth_den, isize grow_lin)
    {
        //with default values for small sizes grows fatser than classic factor of 2 for big slower
        isize size = curr_size;
        while(size < to_fit)
            size = size * growth_num / growth_den + grow_lin; 

        return size;
    }

    struct Set_Capacity_Info
    {
        Allocator* allocator = nullptr;
        isize new_capacity = 0;
        isize align = 0;

        //Bytes to add to each allocation/deallocation.
        //Is basically only used for null terminated stacks (String_Builder) 
        //where it saves many ifs and many headaches
        isize padding_bytes = 0; 

        //If should attempt to call allocator->resize(...) before reserving to
        // allocator->allocate(...). This is because on small stack time saved on
        // resize wouldnt make up for the calling overhead and additional checks
        bool try_resize = false;
        Type_Optims optims = Type_Optims::DEF;
    };
    
    template<class T> nodisc
    Allocation_State set_capacity_allocate(Slice<T>* new_slice, isize* new_size, Slice<T> old_slice, isize old_size, Set_Capacity_Info info) noexcept
    {
        *new_slice = Slice<T>{};
        *new_size = 0;
        
        //if is just deallocation do nothing
        if(info.new_capacity <= 0)
            return Allocation_State::OK;
        
        const isize new_byte_capacity = info.new_capacity * sizeof(T) + info.padding_bytes;
        Slice<u8> new_data;
        Allocation_State allocation_res = Allocation_State::UNSUPPORTED_ACTION;

        //If can & should resize tries to resize
        if(old_slice.size > 0 && info.try_resize)
        {
            Slice<u8> old_data = cast_slice<u8>(old_slice);
            old_data.size += info.padding_bytes;
            allocation_res = info.allocator->resize(&new_data, cast_slice<u8>(old_slice), new_byte_capacity, info.align);
        }

        //If resize failed or didnt even try (allocation_res is set to unsupported) tries to allocate
        if(allocation_res != Allocation_State::OK)
            allocation_res = info.allocator->allocate(&new_data, new_byte_capacity, info.align);
        
        if(allocation_res != Allocation_State::OK)
            return allocation_res;

        new_data.size -= info.padding_bytes;
        *new_slice = cast_slice<T>(new_data);
        *new_size = old_size;
        if(*new_size > info.new_capacity)
            *new_size = info.new_capacity;

        return allocation_res;
    }

    template<class T>
    Allocation_State set_capacity_deallocate(Slice<T>* new_slice, isize* new_size, Slice<T> old_slice, isize old_size, Set_Capacity_Info info) noexcept
    {
        //if there is nothing to deallocate return
        if(old_slice.size <= 0)
            return Allocation_State::OK;

        //If just deallocating deallocate
        Slice<u8> old_data = cast_slice<u8>(old_slice);
        old_data.size += info.padding_bytes;
        if(info.new_capacity <= 0)
        {
            destruct_items(old_slice.data, 0, old_size, info.optims);
            return info.allocator->deallocate(old_data, info.align);
        }

        //if resized in place just destroy extra items (happens while shrinking)
        if(new_slice->data == old_slice.data)
        {
            destruct_items(tail(old_slice, *new_size), info.optims);
            return Allocation_State::OK;
        }
        else
        {
            transfer_items(*new_slice, head(old_slice, *new_size), info.optims);
            destruct_items(old_slice.data, *new_size, old_size, info.optims);
            return info.allocator->deallocate(old_data, info.align);
        }
    }

    template<class T> nodisc
    Allocation_State set_capacity_failing(Stack<T>* stack, isize new_capacity) noexcept
    {
        assert(is_invariant(*stack));

        Set_Capacity_Info info;
        info.new_capacity  = new_capacity;
        info.allocator     = stack->_allocator;
        info.align         = DEF_ALIGNMENT<T>;
        info.optims        = DEF_TYPE_OPTIMS<T>; 
        info.try_resize    = stack->_size * sizeof(T) > 64 || has_flag(info.optims, Type_Optims::BYTE_COPY) == false;
        info.padding_bytes = cast(isize) is_string_char<T> * sizeof(T);

        Slice<T> new_slice;
        isize new_size = 0; 
        Slice<T> old_slice = Slice<T>{stack->_data, stack->_capacity};

        Allocation_State state = set_capacity_allocate(&new_slice, &new_size, old_slice, stack->_size, info);
        if(state != Allocation_State::OK)
            return state;

        set_capacity_deallocate(&new_slice, &new_size, old_slice, stack->_size, info);
        
        stack->_size = new_size;
        stack->_data = new_slice.data;
        stack->_capacity = new_capacity;

        if(stack->_capacity == 0)
            stack_internal::set_data_to_termination(stack);
        else
            stack_internal::null_terminate(stack);

        assert(is_invariant(*stack));
        return Allocation_State::OK;
    }
     
    template<class T> nodisc
    Allocation_State reserve_failing(Stack<T>* stack, isize to_size) noexcept
    {
        if (stack->_capacity >= to_size)
            return Allocation_State::OK;
            
        return set_capacity_failing(stack, to_size);
    }
    
    template<class T>
    void set_capacity(Stack<T>* stack, isize new_capacity)
    {
        Allocation_State state = set_capacity_failing(stack, new_capacity);
        if(state != Allocation_State::OK)
            panic("Stack<T> allocation failed!");
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
        if constexpr(has_flag(DEF_TYPE_OPTIMS<T>, Type_Optims::BYTE_COPY))
            copy_items(capacity_slice, from);
        //else copy then copy construct the rest
        else
        {
            isize copy_to = to->_size;
            if(copy_to > from.size)
                copy_to = from.size;

            copy_items(capacity_slice, head(from, copy_to));
            copy_construct_items(tail(capacity_slice, copy_to), tail(from, copy_to));
        }

        destruct_items(to->_data, from.size, to->_size);
        to->_size = from.size;
        if(to->_capacity == 0)
            stack_internal::set_data_to_termination(to);
        else
            stack_internal::null_terminate(to);

        assert(is_invariant(*to));
    }
    
    template<class T> nodisc
    Stack<T> own(Slice<const T> from, Allocator* alloc)
    {
        Stack<T> out(alloc);
        copy<T>(&out, from);
        return out;
    }
    
    template<class T> nodisc
    Stack<T> own(Slice<T> from, Allocator* alloc)
    {
        Stack<T> out(alloc);
        copy<T>(&out, cast(Slice<const T>) from);
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
    
    template<class T> nodisc
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
        new(ptr) T(cast(T&&) what);
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

        T ret = cast(T&&) (stack->_data[stack->_size]);
        stack->_data[stack->_size].~T();
        
        stack_internal::null_terminate(stack);
        assert(is_invariant(*stack));
        return ret;
    }

    template <class T>
    void push_multiple(Stack<T>* stack, Slice<const typename Stack<T>::T> inserted)
    {
        grow(stack, stack->_size + inserted.size);

        assert(stack->_capacity >= stack->_size + inserted.size && "after reserve there should be enough size");
        Slice<T> availible_slice = {stack->_data + stack->_size, inserted.size};

        copy_construct_items(availible_slice, inserted);
        
        stack->_size += inserted.size;
        stack_internal::null_terminate(stack);
        assert(is_invariant(*stack));
    }
    
    template <class T>
    void push_multiple_move(Stack<T>* stack, Slice<typename Stack<T>::T> inserted)
    {
        grow(stack, stack->_size + inserted.size);

        assert(stack->_capacity >= stack->_size + inserted.size && "after reserve there should be enough size");
        Slice<T> availible_slice = {stack->_data + stack->_size, inserted.size};

        move_construct_items(availible_slice, inserted);

        stack->_size += inserted.size;
        stack_internal::null_terminate(stack);
        assert(is_invariant(*stack));
    }

    template<class T> 
    void pop_multiple(Stack<T>* stack, isize count) noexcept
    {
        assert(count <= size(*stack));
        destruct_items(stack->_data, stack->_size - count, stack->_size);

        stack->_size -= count;
        stack_internal::null_terminate(stack);
        assert(is_invariant(*stack));
    }
    
    template<class T> 
    void clear(Stack<T>* stack)
    {
        pop_multiple(stack, stack->_size);
    }

    template <class T, bool is_zero = false>
    void _resize(Stack<T>* stack, isize to, typename Stack<T>::T const& fill_with) noexcept
    {
        assert(is_invariant(*stack));
        assert(0 <= to);

        reserve(stack, to);
        
        if constexpr(has_flag(DEF_TYPE_OPTIMS<T>, Type_Optims::BYTE_NULL) && is_zero)
        {
            if(stack->_size < to)
                memset(stack->_data + stack->_size, 0, (to - stack->_size)*sizeof(T));
        }
        else
        {
            for (isize i = stack->_size; i < to; i++)
                new(stack->_data + i) T(fill_with);

            destruct_items(stack->_data, to, stack->_size);
        }
        
        stack->_size = to;
        stack_internal::null_terminate(stack);
        assert(is_invariant(*stack));
    }
   
    template <class T>
    void resize(Stack<T>* stack, isize to, typename Stack<T>::T const& fill_with) noexcept
    {
        _resize<T, false>(stack, to, fill_with);
    }

    template<class T> 
    void resize(Stack<T>* stack, isize to)
    {
        _resize<T, true>(stack, to, T());
    }

    template<class T> 
    void resize_for_overwrite(Stack<T>* stack, isize to)
    {
        if constexpr(has_flag(DEF_TYPE_OPTIMS<T>, Type_Optims::BYTE_COPY))
            return resize(stack, to);

        reserve(stack, to);
        stack->_size = to;
        stack_internal::null_terminate(stack);
        assert(is_invariant(*stack));
    }

    template<class T> 
    void insert(Stack<T>* stack, isize at, typename Stack<T>::T what)
    {
        assert(0 <= at && at <= stack->_size);
        if(at >= size(*stack))
            return push(stack, cast(T&&) what);
            
        grow(stack, stack->_size + 1);

        new(last(stack) + 1) T(cast(T&&) *last(stack));

        Slice<T> move_from = slice_range(slice(stack), at, stack->_size - 1);
        Slice<T> move_to = slice_range(slice(stack), at + 1, stack->_size);
        move_items(move_to, move_from);

        stack->_data[at] = cast(T&&) what;
        stack->_size += 1;
        stack_internal::null_terminate(stack);
        assert(is_invariant(*stack));
    }

    template<class T> 
    T remove(Stack<T>* stack, isize at) noexcept
    {
        assert(0 <= at && at < stack->_size);
        assert(stack->_size > 0);
        
        T removed = cast(T&&) stack->_data[at];
        
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

        push(stack, cast(T&&) what);
        swap(&stack->_data[at], last(stack));
    }
    
    //Simple struct that acts as a slice to stack ment to be used
    // as an iterface type when its desired to only allow pushing into a stack
    // pretends it only has data from _from_index guarding the data in front of it from modification
    template <typename T_>
    struct Stack_Appender
    {
        using T = T_;
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
    void push_multiple(Stack_Appender<T>* appender, Slice<const typename Stack<T>::T> inserted)
    {
        return push_multiple(appender->_stack, inserted);
    }
    
    template <class T>
    void push_multiple_move(Stack_Appender<T>* appender, Slice<typename Stack<T>::T> inserted)
    {
        return push_multiple_move(appender->_stack, inserted);
    }

    template<class T>
    void push(Stack_Appender<T>* appender, typename Stack<T>::T what)
    {
        return push(appender->_stack, cast(T&&) what);
    }
    
    template<class T> 
    void reserve(Stack_Appender<T>* appender, isize to)
    {
        return reserve(appender->_stack, to + appender->_from_index);
    }
    
    template<class T> 
    void grow(Stack_Appender<T>* appender, isize to)
    {
        return grow(appender->_stack, to + appender->_from_index);
    }

    template<class T> nodisc
    Allocation_State reserve_failing(Stack_Appender<T>* appender, isize to) noexcept
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
    void resize(Stack_Appender<T>* appender, isize to, typename Stack<T>::T const& fill_with)
    {
        return resize(appender->_stack, to + appender->_from_index, fill_with);
    }
    
    template <class T>
    Stack_Appender<T> append_to(Stack_Appender<T>* appender) noexcept
    {
        Stack_Appender<T> new_appender(appender->_stack);
        new_appender._from_index = size(appender->_stack);
        return new_appender;
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

#include "undefs.h"