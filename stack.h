#pragma once

#include "utils.h"
#include "slice.h"
#include "memory.h"
#include "defines.h"

namespace jot
{
    const u8 NULL_TERMINATION_ARR[8] = {'\0'};

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

    constexpr inline
    isize calculate_stack_growth(isize curr_size, isize to_fit, isize growth_num = 3, isize growth_den = 2, isize grow_lin = 8)
    {
        //with default values for small sizes grows fatser than classic factor of 2 for big slower
        isize size = curr_size;
        while(size < to_fit)
            size = size * growth_num / growth_den + grow_lin; 

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

        if(old_slice->size != 0 && try_resize)
        {
            assert(old_slice->data != nullptr);
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

        if(old_slice->size != 0)
        {
            assert(old_slice->data != nullptr);
            allocator->deallocate(cast_slice<u8>(*old_slice), align);
        }
    }
      
    //fast path for destroying of stacks (instead of set_capacity which is longer)
    template<class T>
    Allocator_State_Type destroy_and_deallocate(Allocator* allocator, Slice<T>* old_slice, isize filled_to, isize align) noexcept
    {
        if constexpr(std::is_trivially_destructible_v<T> == false)
            for(isize i = 0; i < filled_to; i++)
                (*old_slice)[i].~T();

        if(old_slice->size != 0)
        {
            assert(old_slice->data != nullptr);
            return allocator->deallocate(cast_slice<u8>(*old_slice), align);
        }

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

            if(result.state == ERROR)
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
        assert(is_invariant(*stack));

        isize null_terminate_space = cast(isize) is_string_char<T>;
        if (stack->_capacity >= to_fit + null_terminate_space)
            return Allocator_State::OK;

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

    template <class T>
    void splice_bytes(Stack<T>* stack, isize at, isize replace_size, Slice<const T> inserted)
    { 
        assert(is_invariant(*stack));
        Slice<T> items = slice(stack);

        const isize size_delta = inserted.size - replace_size;
        const isize final_size = items.size + size_delta;
        
        reserve(stack, final_size);

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
        stack_internal::null_terminate(stack);
        assert(is_invariant(*stack));
    }
    
    template <class T, class Inserted>
    void splice(Stack<T>* stack, isize at, isize replace_size, Inserted && inserted)
    {       
        Slice<T> items = slice(stack); //for bounds checks
        assert(is_invariant(*stack));
        
        auto it = std::begin(inserted);
        constexpr bool do_move_construct = std::is_rvalue_reference_v<decltype(inserted)>;
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
            reserve(stack, final_size);

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
                else
                    construct_at(items.data + i, items[i - constructed]);
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
            else
                items[i] = *it;
        }

        //insert construct leftover elements
        for (isize i = move_assign_to; i < insert_to; i++, ++it)
        {
            //T* prev = items.data + i;
            if constexpr(do_move_construct)
                construct_at(items.data + i, move(&*it));
            else
                construct_at(items.data + i, *it);
        }
        
        stack_internal::null_terminate(stack);
        assert(is_invariant(*stack));
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

    template <class T, class Inserted>
    void push_multiple(Stack<T>* stack, Inserted && inserted)
    {
        isize inserted_size = cast(isize) std::size(inserted);
        reserve(stack, size(*stack) + inserted_size);

        auto it = std::begin(inserted);
        auto end = std::end(inserted);
        using Iter_Val = std::remove_reference_t<decltype(*it)>;

        constexpr bool is_contiguous_iterator = std::is_pointer_v<decltype(it)>;
        constexpr bool do_move_construct = std::is_rvalue_reference_v<decltype(inserted)>;

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
                else
                    construct_at(capacity_slice.data + i, *it);
            }
        }

        stack->_size += inserted_size;
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
        if(std::is_trivially_constructible_v<T> == false)
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
        move_items(&move_to, &move_from);

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
        move_items(&move_to, &move_from);

        (last(stack))->~T();
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
        return slice(slice(*appender._stack), appender._from_index);
    }

    template<class T> nodisc
    Slice<T> slice(Stack_Appender<T>* appender) 
    {
        return slice(slice(appender->_stack), appender->_from_index);
    }

    template <class T, class Inserted> nodisc
    void push_multiple(Stack_Appender<T>* appender, Inserted && inserted)
    {
        return push_multiple(appender->_stack, cast(Inserted&&) inserted);
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