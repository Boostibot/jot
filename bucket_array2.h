#pragma once
#include "memory.h"

namespace jot::second
{
    #ifndef JOT_HANDLE
        #define JOT_HANDLE
        struct Handle { uint32_t index; };
    #endif

    namespace bucket_array_internal
    {
        struct Bucket;
    }

    template<typename T>
    struct Bucket_Array
    {
        using Bucket = bucket_array_internal::Bucket;
        Bucket* _buckets = nullptr;
        Allocator* _allocator = nullptr;
        uint32_t _buckets_size = 0; 
        uint32_t _buckets_capacity = 0; 
        uint32_t _size = 0;      
        uint32_t _capacity = 0;  
        uint32_t _first_free = -1;
        uint32_t _handle_offset = 0;

        static_assert(sizeof(T) >= sizeof(uint32_t), "Item must be big enough!");

        explicit Bucket_Array(Allocator* alloc = default_allocator(), uint32_t handle_offset = 0) noexcept 
            : _allocator(alloc), _handle_offset(handle_offset)
        {}

        ~Bucket_Array() noexcept;
    };
        
    struct Bucket_Index
    {
        uint32_t bucket;
        uint32_t item;
        uint8_t mask;
        uint8_t bit;
    };
    
    ///Getters 
    template<class T> const T*   data(Bucket_Array<T> const& bucket_array) noexcept       { return bucket_array._data; }
    template<class T> T*         data(Bucket_Array<T>* bucket_array) noexcept             { return bucket_array->_data; }
    template<class T> isize      size(Bucket_Array<T> const& bucket_array) noexcept       { return bucket_array._size; }
    template<class T> isize      size(Bucket_Array<T>* bucket_array) noexcept             { return bucket_array->_size; }
    template<class T> isize      capacity(Bucket_Array<T> const& bucket_array) noexcept   { return bucket_array._capacity; }
    template<class T> isize      capacity(Bucket_Array<T>* bucket_array) noexcept         { return bucket_array->_capacity; }
    template<class T> Allocator* allocator(Bucket_Array<T> const& bucket_array) noexcept  { return bucket_array._allocator; }
    template<class T> Allocator* allocator(Bucket_Array<T>* bucket_array) noexcept        { return bucket_array->_data; }
    
    ///Reserves exactly to_size slots
    template<class T> bool reserve_failing(Bucket_Array<T>* bucket_array, isize to_size) noexcept;
    template<class T> void reserve(Bucket_Array<T>* bucket_array, isize to_capacity);
    
    ///Reserves at least to_size slots using geometric sequence
    template<class T> void grow(Bucket_Array<T>* bucket_array, isize to_size);
    
    ///Inserts an item to the array and returns its handle
    template<class T> Handle insert(Bucket_Array<T>* bucket_array, T val);
    
    ///Inserts an item from the array give its handle
    template<class T> T remove(Bucket_Array<T>* bucket_array, Handle handle) noexcept;

    ///Converts handle to element index
    template<class T> Bucket_Index to_index(Bucket_Array<T> const& bucket_array, Handle slot) noexcept;
    ///Converts element index to handle
    template<class T> Handle to_slot(Bucket_Array<T> const& bucket_array, isize index) noexcept;

    ///returns an item given its handle
    template<class T> T const& get(Bucket_Array<T> const& bucket_array, Handle handle) noexcept;
    template<class T> T* get(Bucket_Array<T>* bucket_array, Handle handle) noexcept;

    ///Returns true if the structure is correct which should be always
    template<class T> bool is_invariant(Bucket_Array<T> const& bucket_array) noexcept;
}

namespace jot::second
{
    namespace bucket_array_internal
    {
        using Mask = uint64_t;

        //The size in bytes to which all Bucket allocations will get padded to. 
        //(Doesnt apply for the Bucket struct array itself just for the Buckets themselvses;
        const static uint32_t BUCKET_GRANULARITY = 4096;
        const static uint32_t BUCKET_SIZE = 256;
        const static uint32_t MASK_BITS = 64;

        //The minimum ammount of items per bucket
        const static uint32_t LEAST_ITEMS_COUNT = 128;

        //The minimum number of empty buckets to make during the first allocation
        const static uint32_t LEAST_BUCKETS_COUNT = 128;

        struct Bucket
        {
            void* data = nullptr;
            uint32_t allocation_size = 0;
            uint32_t capacity = 0;
            Mask mask[BUCKET_SIZE / MASK_BITS] = {0};
        };

        template <typename T>
        isize add_buckets_failing(Bucket_Array<T>* bucket_array, isize added_item_count_)
        {
            assert(is_invariant(*bucket_array));

            isize new_bucket_bytes = div_round_up(added_item_count_ * (isize) sizeof(T), BUCKET_GRANULARITY)*BUCKET_GRANULARITY;
            if(new_bucket_bytes > (uint32_t) -1)
                new_bucket_bytes = (uint32_t) -1;

            isize added_item_count = new_bucket_bytes / (isize) sizeof(T);
            assert(added_item_count >= added_item_count_);

            isize added_bucket_count = div_round_up(added_item_count, BUCKET_SIZE);
            assert(added_item_count > 0);
            assert(added_bucket_count > 0);

            //If doesnt have enough space to store the created buckets reallocates _buckets
            if(added_bucket_count + bucket_array->_buckets_size > bucket_array->_buckets_capacity)
            {
                isize old_size = bucket_array->_buckets_capacity;
                isize new_size = old_size * 2;
                if(new_size < LEAST_BUCKETS_COUNT)
                    new_size = LEAST_BUCKETS_COUNT;

                isize BUCKET_BYTES = (isize) sizeof(Bucket);
                void* new_ptr = reallocate(bucket_array->_allocator, bucket_array->_buckets, 
                    new_size*BUCKET_BYTES, old_size*BUCKET_BYTES, 8, GET_LINE_INFO());
                
                if(new_ptr == nullptr)
                    return new_size*BUCKET_BYTES;

                bucket_array->_buckets = (Bucket*) new_ptr;
                bucket_array->_buckets_capacity = (uint32_t) new_size;
            }

            //Allocates the new added bucket data
            T* bucket_data = (T*) bucket_array->_allocator->allocate(new_bucket_bytes, 8, GET_LINE_INFO());
            if(bucket_data == nullptr)
                return new_bucket_bytes;

            //Sets the added buckets to empty
            isize remaining_item_count = added_item_count;
            for(isize i = 0; i < added_bucket_count; i++)
            {
                assert(remaining_item_count > 0);
                Bucket* curr = &bucket_array->_buckets[bucket_array->_buckets_size + i];

                curr->data = bucket_data + i * BUCKET_SIZE;
                curr->capacity = (uint32_t) min(remaining_item_count, BUCKET_SIZE);
                curr->allocation_size = 0;
                memset(curr->mask, 0, sizeof(curr->mask));

                remaining_item_count -= BUCKET_SIZE;
            }

            //Set the first bucket allocation
            bucket_array->_buckets[bucket_array->_buckets_size].allocation_size = (uint32_t) new_bucket_bytes;

            //Add all new items to free list
            uint32_t first_link_i = bucket_array->_buckets_size * BUCKET_SIZE;
            for(uint32_t i = 0; i < added_item_count; i++)
            {
                uint32_t* link = (uint32_t*) (void*) (bucket_data + i);
                *link = i + 1 + first_link_i;
            }
            
            //Add last item to the end of the free list and set free list to first
            uint32_t* last_link = (uint32_t*) (void*) (bucket_data + added_item_count - 1);
            *last_link = bucket_array->_first_free;
            bucket_array->_first_free = first_link_i;
            bucket_array->_buckets_size += (uint32_t) added_bucket_count;
            bucket_array->_capacity += (uint32_t) added_item_count;
            
            assert(is_invariant(*bucket_array));
            return 0;
        }
    }
    
    template<class T> 
    bool is_invariant(Bucket_Array<T> const& bucket_array) noexcept
    {
        bool free_in_range = bucket_array._first_free == (uint32_t) -1 
            || bucket_array._first_free / bucket_array_internal::BUCKET_SIZE < bucket_array._buckets_size;
        bool bucket_sizes_correct = bucket_array._buckets_size <= bucket_array._buckets_capacity;
        bool bucket_correct = (bucket_array._buckets == nullptr) == (bucket_array._buckets_capacity == 0);

        bool ret = free_in_range && bucket_sizes_correct && bucket_correct;
        assert(ret);
        return ret;
    }

    template <typename T>
    void add_buckets(Bucket_Array<T>* bucket_array, isize added_item_count)
    {
        isize failed_bytes_requested = bucket_array_internal::add_buckets_failing(bucket_array, added_item_count);
        if(failed_bytes_requested != 0)
        {
            memory_globals::out_of_memory_hadler()(GET_LINE_INFO(),"Bucket_Array<T> allocation failed! "
                "Attempted to allocated %t bytes from allocator %p Bucket_Array: {size: %t, capacity: %t} sizeof(T): %z",
                failed_bytes_requested, bucket_array->_allocator, bucket_array->_size, bucket_array->_capacity, sizeof(T));
        }
    }

    template <typename T>
    Bucket_Array<T>::~Bucket_Array() noexcept
    {
        using namespace bucket_array_internal;

        //We always 'give' the allocation (allocation_size != 0) 
        // to the first bucket so we need to iterate backwards
        // as to not touch freed memory
        for(isize i = _buckets_size; i-- > 0; )
        {
            Bucket* curr = _buckets + i;
            for(uint32_t j = 0; j < curr->capacity; j++)
            {
                uint32_t mask = j / MASK_BITS;
                uint32_t bit_i = j % MASK_BITS;
                Mask bit = (Mask) 1 << bit_i;

                if(curr->mask[mask] & bit)
                {
                    T* item = (T*) curr->data + j;
                    item->~T();
                }
            }

            if(curr->allocation_size != 0)
                _allocator->deallocate(curr->data, curr->allocation_size, 8, GET_LINE_INFO());
        }

        if(_buckets != nullptr)
            _allocator->deallocate(_buckets, _buckets_capacity*(isize) sizeof(Bucket), 8, GET_LINE_INFO());
    }

    template <typename T>
    bool reserve_failing(Bucket_Array<T>* bucket_array, isize to_size) noexcept
    {
        if (bucket_array->_capacity >= to_size)
            return true;

        return bucket_array_internal::add_buckets_failing(bucket_array, to_size - bucket_array->_capacity) == 0;
    }

    template <typename T>
    void reserve(Bucket_Array<T>* bucket_array, isize to_size)
    {
        if (bucket_array->_capacity >= to_size)
            return;

        add_buckets(bucket_array, to_size - bucket_array->_capacity);
    }
    
    template <typename T>
    void grow(Bucket_Array<T>* bucket_array, isize to_size)
    {
        if (bucket_array->_capacity >= to_size)
            return;
            
        isize added_item_count = bucket_array_internal::LEAST_ITEMS_COUNT;
        if(added_item_count < to_size - bucket_array->_capacity)
            added_item_count = to_size - bucket_array->_capacity;

        add_buckets(bucket_array, added_item_count);
    }
    
    template <typename T>
    Bucket_Index to_index(Bucket_Array<T> const& bucket_array, Handle handle) noexcept
    {
        using namespace bucket_array_internal;
        uint32_t index = handle.index - bucket_array._handle_offset;
        Bucket_Index out = {0};
        out.bucket = index / BUCKET_SIZE;
        out.item = index % BUCKET_SIZE;
        out.mask = (uint8_t) (out.item / MASK_BITS); 
        out.bit = (uint8_t) (out.item % MASK_BITS); 

        assert(out.bucket < bucket_array._buckets_size);
        return out;
    }
    
    template <typename T>
    Handle to_handle(Bucket_Array<T> const& bucket_array, Bucket_Index index) noexcept
    {
        assert(index.bucket <= bucket_array._buckets_size && "invalid index!");
        assert(index.item <= bucket_array_internal::BUCKET_SIZE && "invalid index!");

        isize out = index.bucket * bucket_array_internal::BUCKET_SIZE + index.item;
        return Handle{(uint32_t) out + bucket_array._handle_offset};
    }

    template <typename T>
    Handle insert(Bucket_Array<T>* bucket_array, T what)
    {
        using namespace bucket_array_internal;
        grow(bucket_array, bucket_array->_size + 1);

        assert(bucket_array->_first_free != (uint32_t) -1);
        Bucket_Index index = to_index(*bucket_array, Handle{bucket_array->_first_free});
        Bucket* bucket = bucket_array->_buckets + index.bucket;
        
        
        Mask bit = (Mask) 1 << index.bit;
        assert((bucket->mask[index.mask] & bit) == 0);
        assert(bucket->capacity <= BUCKET_SIZE);

        bucket->mask[index.mask] |= bit;

        Handle handle = {bucket_array->_first_free};

        T* bucket_data = (T*) bucket->data;
        bucket_array->_first_free = *(uint32_t*) (void*) (bucket_data + index.item);
        bucket_array->_size += 1;
        new (bucket_data + index.item) T((T&&) what);
        
        assert(is_invariant(*bucket_array));
        return handle;
    }
    
    template <typename T>
    T remove(Bucket_Array<T>* bucket_array, Handle handle) noexcept
    { 
        using namespace bucket_array_internal;

        Bucket_Index index = to_index(*bucket_array, handle);
        Bucket* bucket = bucket_array->_buckets + index.bucket;
        Mask bit = (Mask) 1 << index.bit;
        
        assert((bucket->mask[index.mask] & bit) > 0);
        assert(bucket->capacity <= BUCKET_SIZE);

        bucket->mask[index.mask] &= ~bit;

        T* bucket_data = (T*) bucket->data;
        T removed = (T&&) bucket_data[index.item];
        bucket_data[index.item].~T();

        *(uint32_t*) (void*) (bucket_data + index.item) = bucket_array->_first_free;
        bucket_array->_first_free = handle.index;
        bucket_array->_size -= 1;
        
        assert(is_invariant(*bucket_array));
        return removed;
    }

    template <typename T>
    T* get(Bucket_Array<T>* from, Handle handle) noexcept
    {
        using namespace bucket_array_internal;

        Bucket_Index index = to_index(*from, handle);
        Mask bit = (Mask) 1 << index.bit;
        assert((from->_buckets[index.bucket].mask[index.mask] & bit) > 0 && "handle not used!");
        T* bucket_data = (T*) from->_buckets[index.bucket].data;

        return bucket_data + index.item;
    }
    
    template <typename T>
    T const& get(Bucket_Array<T> const& from, Handle handle) noexcept
    {
        Bucket_Array<T>* cheated = (Bucket_Array<T>*) (void*) &from;
        return *get(cheated, handle);
    }
}
