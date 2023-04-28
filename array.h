#pragma once

#include "memory.h"

namespace jot
{
    ///Resizable dynamic array also used to represent dynamic strings
    template <typename T_>
    struct Array
    {   
        using T = T_;

        T* _data = nullptr;
        Allocator* _allocator = default_allocator();
        isize _size = 0;
        isize _capacity = 0;
        
        Array(Allocator* allocator = default_allocator()) noexcept;
        Array(Array && other) noexcept;
        Array(Array const& other);
        ~Array() noexcept;

        Array& operator=(Array && other) noexcept;
        Array& operator=(Array const& other);
        
        T const& operator[](isize index) const noexcept  
        { 
            assert(0 <= index && index < _size && "index out of range"); return _data[index]; 
        }

        T& operator[](isize index) noexcept 
        { 
            assert(0 <= index && index < _size && "index out of range"); return _data[index]; 
        }
    };

    ///Getters 
    template<class T> const T*   data(Array<T> const& array) noexcept       { return array._data; }
    template<class T> T*         data(Array<T>* array) noexcept             { return array->_data; }
    template<class T> isize      size(Array<T> const& array) noexcept       { return array._size; }
    template<class T> isize      size(Array<T>* array) noexcept             { return array->_size; }
    template<class T> isize      capacity(Array<T> const& array) noexcept   { return array._capacity; }
    template<class T> isize      capacity(Array<T>* array) noexcept         { return array->_capacity; }
    template<class T> Allocator* allocator(Array<T> const& array) noexcept  { return array._allocator; }
    template<class T> Allocator* allocator(Array<T>* array) noexcept        { return array->_allocator; }

    ///iterators
    template<typename T> T*       begin(Array<T>& array) noexcept           { return array._data; }
    template<typename T> const T* begin(Array<T> const& array) noexcept     { return array._data; }
    template<typename T> T*       end(Array<T>& array) noexcept             { return array._data + array._size; }
    template<typename T> const T* end(Array<T> const& array) noexcept       { return array._data + array._size; }

    ///Returns a slice containing all items of the array
    template<class T> Slice<const T> slice(Array<T> const& array) noexcept  { return {array._data, array._size}; }
    template<class T> Slice<T>       slice(Array<T>* array) noexcept        { return {array->_data, array->_size}; }

    ///Get first and last items of a array. Cannot be used on empty array!
    template<class T> T*       last(Array<T>* array) noexcept        { return &(*array)[array->_size - 1]; }
    template<class T> T const& last(Array<T> const& array) noexcept  { return (array)[array._size - 1]; }
    template<class T> T*       first(Array<T>* array) noexcept       { return &(*array)[0]; }
    template<class T> T const& first(Array<T> const& array) noexcept { return (array)[0]; }
    
    ///Returns true if the structure state is correct which should be always
    template<class T> bool is_invariant(Array<T> const& array) noexcept;
    template<class T> bool is_empty(Array<T> const& array) noexcept;

    ///swaps contents of left and right arrays
    template<class T> void swap(Array<T>* left, Array<T>* right) noexcept;
    ///Copies items to array. Items within the array before this opperations are discarded
    template<class T> void copy(Array<T>* array, Slice<const T> items);
    ///Removes all items from array
    template<class T> void clear(Array<T>* array) noexcept;

    ///Makes a new array with copied items using the probided allocator
    template<class T> Array<T> own(Slice<const T> from, Allocator* alloc = default_allocator());
    template<class T> Array<T> own(Slice<T> from, Allocator* alloc = default_allocator());
    template<class T> Array<T> own_scratch(Slice<const T> from, Allocator* alloc = scratch_allocator()) {return own(from, alloc);}
    template<class T> Array<T> own_scratch(Slice<T> from, Allocator* alloc = scratch_allocator())       {return own(from, alloc);}

    ///Reallocates array to the specified capacity. If the capacity is smaller then its size, shrinks it destroying items
    ///in process
    template<class T> NODISCARD bool set_capacity_failing(Array<T>* array, isize new_capacity) noexcept;
    template<class T> void set_capacity(Array<T>* array, isize new_capacity);

    ///Potentially reallocates array so that capacity is at least to_size. If capacity is already greater than to_size
    ///does nothing.
    template<class T> NODISCARD bool reserve_failing(Array<T>* array, isize to_size) noexcept;
    template<class T> void reserve(Array<T>* array, isize to_size);

    ///Same as reserve expect when reallocation happens grows 3/2*size + 8
    template<class T> void grow(Array<T>* array, isize to_fit);

    ///Sets size of array. If to_size is smaller then array size trims the array. If is greater fills the added space with fill_with
    template<class T> void resize(Array<T>* array, isize to_size, typename Array<T>::T const& fill_with = {}) noexcept;
    ///Sets size of array. If to_size is smaller then array size trims the array. 
    ///If is greater and the T type allows it leaves the space uninitialized
    template<class T> void resize_for_overwrite(Array<T>* array, isize to);

    ///Adds an item to the end of the array
    template<class T> void push(Array<T>* array, typename Array<T>::T what);
    ///Removes an item at the end array. The array must not be empty!
    template<class T>    T pop(Array<T>* array) noexcept;

    ///Pushes all items from inserted slice into the array
    template<class T> void push_multiple(Array<T>* array, Slice<const typename Array<T>::T> inserted);
    ///Pushes all items from inserted slice into the array moving them out of inserted slice
    template<class T> void push_multiple_move(Array<T>* array, Slice<typename Array<T>::T> inserted);
    ///Pops multiple items from array. The array must contain at least count elements!
    template<class T> void pop_multiple(Array<T>* array, isize count) noexcept;

    ///Inserts an item into the array so that its index is at. Moves all later elemnts forward one index
    template<class T> void insert(Array<T>* array, isize at, typename Array<T>::T what);
    ///Removes an item from the array at specified index. Moves all later elemnts backwards one index. The array most not be empty!
    template<class T>    T remove(Array<T>* array, isize at) noexcept;

    ///Inserts an item into the array so that its index is at. The item that was originally at this index becomes last
    template<class T> void unordered_insert(Array<T>* array, isize at, typename Array<T>::T what);
    ///Removes an item from the array at specified index. Moves the last item into freed up spot. The array most not be empty!
    template<class T>    T unordered_remove(Array<T>* array, isize at) noexcept;
    
    ///Tells array if this type should be null terminated. Provide specialization for your desired type if you
    /// want it to be considered a string (see string.h)
    template<class T> static constexpr bool is_string_char = false;
}

namespace jot
{
    namespace array_internal 
    {
        inline const uint8_t* null_termination_array()
        {
            static const uint8_t NULL_TERMINATION_ARRAY[8] = {'\0'};
            return NULL_TERMINATION_ARRAY;
        }

        template<class T> 
        void null_terminate(Array<T>* array) noexcept
        {
            if constexpr(is_string_char<T>)
                array->_data[array->_size] = T();
        }
        
        template<class T> 
        void set_data_to_termination(Array<T>* array)
        {
            if constexpr(is_string_char<T>)
                array->_data = (T*) (void*) null_termination_array();
            else
                array->_data = nullptr;
        }   
        
        template<typename T> constexpr 
        void destruct_items(T* data, isize from, isize to) noexcept
        {
            for(isize i = from; i < to; i++)
                data[i].~T();
        }
    }
    
    template<class T>
    bool set_capacity_failing(Array<T>* array, isize new_capacity) noexcept
    {
        assert(is_invariant(*array));

        isize old_byte_cap = array->_capacity * (isize) sizeof(T);
        isize new_byte_cap = new_capacity * (isize) sizeof(T);

        bool is_string = is_string_char<T>;
        if(is_string && new_byte_cap != 0)
            new_byte_cap += (isize) sizeof(T);
            
        if(is_string && old_byte_cap != 0)
            old_byte_cap += (isize) sizeof(T);

        void* new_data = nullptr;
        bool state = memory_resize_allocate(array->_allocator, &new_data, new_byte_cap, 
            array->_data, old_byte_cap, DEF_ALIGNMENT<T>, GET_LINE_INFO());

        if(state == false)
            return false;

        isize to_size = array->_size;
        if(new_capacity < to_size)
            to_size = new_capacity;

        if(new_data != array->_data)
        {
            T* new_data_t = (T*) new_data; 
            for(isize i = 0; i < to_size; i++)
            {
                new(new_data_t + i) T((T&&) array->_data[i]);
                array->_data[i].~T();
            }
        }

        array_internal::destruct_items(array->_data, new_capacity, array->_size);
        memory_resize_deallocate(array->_allocator, &new_data, new_byte_cap, 
            array->_data, old_byte_cap, DEF_ALIGNMENT<T>, GET_LINE_INFO());

        array->_size = to_size;
        array->_data = (T*) new_data;
        array->_capacity = new_capacity;

        if(array->_capacity == 0)
            array_internal::set_data_to_termination(array);
        else
            array_internal::null_terminate(array);

        assert(is_invariant(*array));
        return true;
    }

    template<typename T>
    Array<T>::Array(Allocator* allocator) noexcept
    {
        array_internal::set_data_to_termination(this);
        _allocator = allocator;
    }

    template<typename T>
    Array<T>::~Array() noexcept 
    {
        if(_capacity != 0)
        {
            array_internal::destruct_items(_data, 0, _size);
            isize cap = _capacity + (isize) is_string_char<T>;
            _allocator->deallocate(_data, cap * (isize) sizeof(T), DEF_ALIGNMENT<T>, GET_LINE_INFO());
        }
    }

    template<typename T>
    Array<T>::Array(Array && other) noexcept 
    {
        array_internal::set_data_to_termination(this);
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
        array_internal::set_data_to_termination(this);
        copy(this, slice(other));
    }

    template<typename T>
    Array<T>& Array<T>::operator=(Array<T> const& other) 
    {
        copy(this, slice(other));
        return *this;
    }

    template<class T>
    bool is_invariant(Array<T> const& array) noexcept
    {
        bool size_inv = array._capacity >= array._size;
        bool capa_inv = array._capacity >= 0; 
        bool data_inv = true;
        bool espcape_inv = true;

        if constexpr(is_string_char<T>)
            espcape_inv = array._data != nullptr && array._data[array._size] == T(0);
        else
            data_inv = (array._capacity == 0) == (array._data == nullptr);

        return size_inv && capa_inv && data_inv && espcape_inv;
    }
     
    template<class T>
    bool reserve_failing(Array<T>* array, isize to_size) noexcept
    {
        if (array->_capacity >= to_size)
            return true;
            
        return set_capacity_failing(array, to_size);
    }
    
    template<class T>
    void set_capacity(Array<T>* array, isize new_capacity)
    {
        if(set_capacity_failing(array, new_capacity) == false)
        {
            const char* alloc_name = array->_allocator->get_stats().name; 
            isize requested = new_capacity* (isize) sizeof(T);
            memory_globals::out_of_memory_hadler()(GET_LINE_INFO(),
                "Array<T> memory allocation failed! "
                "Attempted to allocated %t bytes from allocator %p name %s"
                "Array: {size: %t, capacity: %t} sizeof(T): %z",
                requested, array->_allocator, 
                alloc_name ? alloc_name : "<No alloc name>", 
                array->_size, array->_capacity, sizeof(T));
        }
    }

    template<class T>
    void reserve(Array<T>* array, isize to_capacity)
    {
        if (array->_capacity < to_capacity)
            set_capacity(array, to_capacity);
    }

    template<class T>
    void grow(Array<T>* array, isize to_fit)
    {
        if (array->_capacity >= to_fit)
            return;
        
        //for small sizes grows fatser than classic factor of 2 for big slower
        isize new_capacity = array->_size;
        while(new_capacity < to_fit)
            new_capacity = new_capacity * 3/2 + 8; 

        set_capacity(array, new_capacity);
    }

    template<class T>
    void copy(Array<T>* to, Slice<const T> from)
    {
        assert(is_invariant(*to));
        reserve(to, from.size);
        
        isize copy_to = to->_size;
        if(copy_to > from.size)
            copy_to = from.size;

        for(isize i = 0; i < copy_to; i++)
            to->_data[i] = from[i];

        for(isize i = copy_to; i < from.size; i++)
            new(&to->_data[i]) T(from[i]);

        array_internal::destruct_items(to->_data, from.size, to->_size);

        to->_size = from.size;
        if(to->_capacity == 0)
            array_internal::set_data_to_termination(to);
        else
            array_internal::null_terminate(to);

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
    bool is_empty(Array<T> const& array) noexcept
    {
        return array._size == 0;
    }
    
    template<class T>
    void push(Array<T>* array, typename Array<T>::T what)
    {
        grow(array, array->_size + 1);
        
        new (array->_data + array->_size) T(((T&&) (what)));
        array->_size++;
        
        array_internal::null_terminate(array);
        assert(is_invariant(*array));
    }

    template<class T> 
    T pop(Array<T>* array) noexcept
    {
        assert(is_invariant(*array));
        assert(array->_size != 0);

        array->_size--;

        T ret = (T&&) (array->_data[array->_size]);
        array->_data[array->_size].~T();
        
        array_internal::null_terminate(array);
        assert(is_invariant(*array));
        return ret;
    }

    template <class T>
    void push_multiple(Array<T>* array, Slice<const typename Array<T>::T> inserted)
    {
        grow(array, array->_size + inserted.size);
        
        T* base = array->_data + array->_size;
        for(isize i = 0; i < inserted.size; i++)
            new(base + i) T(inserted.data[i]);

        array->_size += inserted.size;
        array_internal::null_terminate(array);
        assert(is_invariant(*array));
    }
    
    template <class T>
    void push_multiple_move(Array<T>* array, Slice<typename Array<T>::T> inserted)
    {
        grow(array, array->_size + inserted.size);
        
        T* base = array->_data + array->_size;
        for(isize i = 0; i < inserted.size; i++)
            new(base + i) T((T&&) inserted.data[i]);

        array->_size += inserted.size;
        array_internal::null_terminate(array);
        assert(is_invariant(*array));
    }

    template<class T> 
    void pop_multiple(Array<T>* array, isize count) noexcept
    {
        assert(count <= size(*array));
        array_internal::destruct_items(array->_data, array->_size - count, array->_size);

        array->_size -= count;
        array_internal::null_terminate(array);
        assert(is_invariant(*array));
    }
    
    template<class T> 
    void clear(Array<T>* array) noexcept
    {
        pop_multiple(array, array->_size);
    }

    template <class T>
    void resize(Array<T>* array, isize to, typename Array<T>::T const& fill_with) noexcept
    {
        assert(is_invariant(*array));
        assert(0 <= to);

        reserve(array, to);
        
        for (isize i = array->_size; i < to; i++)
            new(array->_data + i) T(fill_with);

        array_internal::destruct_items(array->_data, to, array->_size);
        
        array->_size = to;
        array_internal::null_terminate(array);
        assert(is_invariant(*array));
    }

    template<class T> 
    void resize_for_overwrite(Array<T>* array, isize to)
    {
        assert(is_invariant(*array));
        reserve(array, to);
        array->_size = to;
        array_internal::null_terminate(array);
    }

    template<class T> 
    void insert(Array<T>* array, isize at, typename Array<T>::T what)
    {
        assert(0 <= at && at <= array->_size);
        if(at >= size(*array))
            return push(array, (T&&) what);
            
        grow(array, array->_size + 1);

        T* created = &array->_data[array->_size];
        new(created) T((T&&) *(created - 1));

        for(isize i = array->_size - 1; i-- > at + 1; )
            array->_data[i] = (T&&) array->_data[i - 1];

        array->_data[at] = (T&&) what;
        array->_size += 1;
        array_internal::null_terminate(array);
        assert(is_invariant(*array));
    }

    template<class T> 
    T remove(Array<T>* array, isize at) noexcept
    {
        assert(0 <= at && at < array->_size);
        assert(array->_size > 0);
        
        T removed = (T&&) array->_data[at];
        
        for(isize i = at; i < array->_size - 1; i++)
            array->_data[i] = (T&&) array->_data[i + 1];
        
        array->_size -= 1;
        array->_data[array->_size].~T();
        array_internal::null_terminate(array);
        assert(is_invariant(*array));
        return removed;
    }

    template<class T> 
    T unordered_remove(Array<T>* array, isize at) noexcept
    {
        assert(0 <= at && at < array->_size);
        assert(array->_size > 0);

        swap(&array->_data[at], last(array));
        return pop(array);
    }

    template<class T>
    void unordered_insert(Array<T>* array, isize at, typename Array<T>::T what)
    {
        assert(0 <= at && at <= array->_size);

        push(array, (T&&) what);
        swap(&array->_data[at], last(array));
    }
}

