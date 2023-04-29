#pragma once
#include "memory.h"

namespace jot
{
    #ifndef JOT_WEAK_HANDLE
        #define JOT_WEAK_HANDLE
        struct Weak_Handle 
        { 
            uint32_t index; 
            uint32_t generation; 
        };
    #endif

    namespace weak_bucket_array_internal
    {
        struct Bucket;
    }
    
    ///Large array of arrays of items with stable adress accessible through stable handle
    ///The handle is validated if the referenced item didnt change.
    template<typename T>
    struct Weak_Bucket_Array
    {
        weak_bucket_array_internal::Bucket* _buckets = nullptr;
        Allocator* _allocator = nullptr;
        uint32_t _buckets_size = 0; 
        uint32_t _buckets_capacity = 0; 
        uint32_t _size = 0;      
        uint32_t _capacity = 0;  
        uint32_t _first_free = -1;
        uint32_t _handle_offset = 0;

        static_assert(sizeof(T) >= sizeof(uint32_t), "Item must be big enough!");

        explicit Weak_Bucket_Array(Allocator* alloc = default_allocator(), uint32_t handle_offset = 0) noexcept 
            : _allocator(alloc), _handle_offset(handle_offset)
        {}

        ~Weak_Bucket_Array() noexcept;
    };
        
    struct Weak_Bucket_Index
    {
        uint32_t bucket;
        uint32_t item;
    };
    
    ///Getters 
    template<class T> isize      size(Weak_Bucket_Array<T> const& bucket_array) noexcept       { return bucket_array._size; }
    template<class T> isize      size(Weak_Bucket_Array<T>* bucket_array) noexcept             { return bucket_array->_size; }
    template<class T> isize      capacity(Weak_Bucket_Array<T> const& bucket_array) noexcept   { return bucket_array._capacity; }
    template<class T> isize      capacity(Weak_Bucket_Array<T>* bucket_array) noexcept         { return bucket_array->_capacity; }
    template<class T> Allocator* allocator(Weak_Bucket_Array<T> const& bucket_array) noexcept  { return bucket_array._allocator; }
    template<class T> Allocator* allocator(Weak_Bucket_Array<T>* bucket_array) noexcept        { return bucket_array->_allocator; }
    
    ///Reserves exactly to_size slots
    template<class T> bool reserve_failing(Weak_Bucket_Array<T>* bucket_array, isize to_size) noexcept;
    template<class T> void reserve(Weak_Bucket_Array<T>* bucket_array, isize to_capacity);
    
    ///Reserves at least to_size slots using geometric sequence
    template<class T> void grow(Weak_Bucket_Array<T>* bucket_array, isize to_size);
    
    ///Inserts an item to the array and returns its handle
    template<class T> Weak_Handle insert(Weak_Bucket_Array<T>* bucket_array, T val);
    
    ///Removes an item from the array give its handle and returns true. If the handle is invalid does nothing and returns false
    template <typename T> bool remove(Weak_Bucket_Array<T>* bucket_array, Weak_Handle handle) noexcept;

    ///Converts handle to element index
    template<class T> Weak_Bucket_Index to_index(Weak_Bucket_Array<T> const& bucket_array, Weak_Handle slot) noexcept;
    ///Converts element index to handle
<<<<<<< Updated upstream
    template<class T> Weak_Handle to_slot(Weak_Bucket_Array<T> const& bucket_array, isize index) noexcept;
=======
    template<class T> Weak_Handle to_handle(Weak_Bucket_Array<T> const& bucket_array, isize index) noexcept;
>>>>>>> Stashed changes

    ///returns an item given its handle. if the handle is not found or is outdated returns if_not_found instead.
    template <typename T> T const& get(Weak_Bucket_Array<T> const& from, Weak_Handle handle, T const& if_not_found) noexcept;
    template <typename T> T* get(Weak_Bucket_Array<T>* from, Weak_Handle handle, T* if_not_found) noexcept;

    ///Returns true if the structure is correct which should be always
    template<class T> bool is_invariant(Weak_Bucket_Array<T> const& bucket_array) noexcept;
}

namespace jot
{
    namespace weak_bucket_array_internal
    {
        //This structure functions the same way as bucket array except instead of single bit we store a generation
        // counter. As such we can see the regular bucket array as a limit case of this as we decrease the number of bits in counter

        //Only notable thing here is that since the memory footprint is quite high (5 extra bytes per item) we tried really hard to
        // remove all possible fields from bucket. We now track allocation in the lowest bit of the data pointer and calculate the allocation
        // size by summing BUCKET_SIZE's and then rounding in consistent way.
        
        //The size in bytes to which all Bucket allocations will get padded to. 
        //(Doesnt apply for the Bucket struct array itself just for the Buckets themselvses)
        const static uint32_t BUCKET_GRANULARITY = 4096; 
        const static uint32_t BUCKET_SIZE = 8;

        //The minimum ammount of items per bucket
        const static uint32_t LEAST_ITEMS_COUNT = 128;

        //The minimum number of empty buckets to make during the first allocation
        const static uint32_t LEAST_BUCKETS_COUNT = 128;
        const static uint32_t USED_BIT = 1 << 31;
        const static uint64_t ALLOCATED_BIT = 1;

        struct Bucket
        {
            void* data;
            uint32_t generations[BUCKET_SIZE];
        };

        #define BUCKET_DATA(bucket) ((T*) ((uint64_t) (bucket)->data & ~ALLOCATED_BIT))

        template <typename T>
        isize add_buckets_failing(Weak_Bucket_Array<T>* bucket_array, isize added_item_count_)
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
            for(isize i = 0; i < added_bucket_count; i++)
            {
                Bucket new_bucket = {0}; 
                new_bucket.data = bucket_data + i * BUCKET_SIZE;
                bucket_array->_buckets[bucket_array->_buckets_size + i] = new_bucket;
            }

            //Set the first bucket allocation
            Bucket* first_bucket = bucket_array->_buckets + bucket_array->_buckets_size;
            first_bucket->data = (void*) ((uint64_t) first_bucket->data | ALLOCATED_BIT);

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
    bool is_invariant(Weak_Bucket_Array<T> const& bucket_array) noexcept
    {
        bool free_in_range = bucket_array._first_free == (uint32_t) -1 
            || bucket_array._first_free / weak_bucket_array_internal::BUCKET_SIZE < bucket_array._buckets_size;
        bool bucket_sizes_correct = bucket_array._buckets_size <= bucket_array._buckets_capacity;
        bool bucket_correct = (bucket_array._buckets == nullptr) == (bucket_array._buckets_capacity == 0);

        bool ret = free_in_range && bucket_sizes_correct && bucket_correct;
        assert(ret);
        return ret;
    }

    template <typename T>
    void add_buckets(Weak_Bucket_Array<T>* bucket_array, isize added_item_count)
    {
        isize failed_bytes_requested = weak_bucket_array_internal::add_buckets_failing(bucket_array, added_item_count);
        if(failed_bytes_requested != 0)
        {
            memory_globals::out_of_memory_hadler()(GET_LINE_INFO(),"Weak_Bucket_Array<T> allocation failed! "
                "Attempted to allocated %t bytes from allocator %p Weak_Bucket_Array: {size: %t, capacity: %t} sizeof(T): %z",
                failed_bytes_requested, bucket_array->_allocator, bucket_array->_size, bucket_array->_capacity, sizeof(T));
        }
    }

    template <typename T>
    Weak_Bucket_Array<T>::~Weak_Bucket_Array() noexcept
    {
        using namespace weak_bucket_array_internal;

        //We always 'give' the allocation
        // to the first bucket so we need to iterate backwards
        // as to not touch freed memory
        isize accumulated_count = 0;
        for(isize i = _buckets_size; i-- > 0; )
        {
            Bucket* curr = _buckets + i;
            for(uint32_t j = 0; j < BUCKET_SIZE; j++)
            {
                if(curr->generations[j] & USED_BIT)
                    BUCKET_DATA(curr)[j].~T();
            }

            accumulated_count += BUCKET_SIZE;
            uint64_t was_alloced = (uint64_t) curr->data & ALLOCATED_BIT;
            if(was_alloced)
            {
                isize alloced_size = accumulated_count * (isize) sizeof(T) / BUCKET_GRANULARITY * BUCKET_GRANULARITY;
                _allocator->deallocate(BUCKET_DATA(curr), alloced_size, 8, GET_LINE_INFO());
                accumulated_count = 0;
            }
        }

        if(_buckets != nullptr)
            _allocator->deallocate(_buckets, _buckets_capacity*(isize) sizeof(Bucket), 8, GET_LINE_INFO());
    }

    template <typename T>
    bool reserve_failing(Weak_Bucket_Array<T>* bucket_array, isize to_size) noexcept
    {
        if (bucket_array->_capacity >= to_size)
            return true;

        return weak_bucket_array_internal::add_buckets_failing(bucket_array, to_size - bucket_array->_capacity) == 0;
    }

    template <typename T>
    void reserve(Weak_Bucket_Array<T>* bucket_array, isize to_size)
    {
        if (bucket_array->_capacity >= to_size)
            return;

        add_buckets(bucket_array, to_size - bucket_array->_capacity);
    }
    
    template <typename T>
    void grow(Weak_Bucket_Array<T>* bucket_array, isize to_size)
    {
        if (bucket_array->_capacity >= to_size)
            return;
            
        isize added_item_count = weak_bucket_array_internal::LEAST_ITEMS_COUNT;
        if(added_item_count < to_size - bucket_array->_capacity)
            added_item_count = to_size - bucket_array->_capacity;

        add_buckets(bucket_array, added_item_count);
    }
    
    template <typename T>
    Weak_Bucket_Index to_index(Weak_Bucket_Array<T> const& bucket_array, Weak_Handle handle) noexcept
    {
        using namespace weak_bucket_array_internal;
        uint32_t index = handle.index - bucket_array._handle_offset;
        Weak_Bucket_Index out = {0};
        out.bucket = index / BUCKET_SIZE;
        out.item = index % BUCKET_SIZE;

        assert(out.bucket < bucket_array._buckets_size);
        return out;
    }
    
    template <typename T>
    Weak_Handle to_handle(Weak_Bucket_Array<T> const& bucket_array, Weak_Bucket_Index index) noexcept
    {
        assert(index.bucket <= bucket_array._buckets_size && "invalid index!");
        assert(index.item <= weak_bucket_array_internal::BUCKET_SIZE && "invalid index!");

        Weak_Handle out = {0};
        out.index = index.bucket * weak_bucket_array_internal::BUCKET_SIZE + index.item;
        out.generation = bucket_array._buckets->generations[index];

        return out;
    }

    template <typename T>
    Weak_Handle insert(Weak_Bucket_Array<T>* bucket_array, T what)
    {
        using namespace weak_bucket_array_internal;
        grow(bucket_array, bucket_array->_size + 1);

        assert(bucket_array->_first_free != (uint32_t) -1);
        Weak_Bucket_Index index = to_index(*bucket_array, Weak_Handle{bucket_array->_first_free});
        Bucket* bucket = bucket_array->_buckets + index.bucket;
        
        assert((bucket->generations[index.item] & USED_BIT) == 0);

        bucket->generations[index.item] += 1;
        bucket->generations[index.item] |= USED_BIT;

        Weak_Handle handle = {0};
        handle.index = bucket_array->_first_free;
        handle.generation = bucket->generations[index.item];

        T* bucket_data = BUCKET_DATA(bucket);
        uint64_t ptr_num = (uint64_t) bucket->data;
        uint64_t rest_num = ~ALLOCATED_BIT;
        uint64_t result = ptr_num & rest_num;
        T* _data = (T*) result;

        bucket_array->_first_free = *(uint32_t*) (void*) (bucket_data + index.item);
        bucket_array->_size += 1;
        new (bucket_data + index.item) T((T&&) what);
        
        assert(is_invariant(*bucket_array));
        return handle;
    }
    
    template <typename T>
    bool remove(Weak_Bucket_Array<T>* bucket_array, Weak_Handle handle) noexcept
    { 
        using namespace weak_bucket_array_internal;

        Weak_Bucket_Index index = to_index(*bucket_array, handle);
        Bucket* bucket = bucket_array->_buckets + index.bucket;
        if(index.bucket > bucket_array->_size || handle.generation != bucket->generations[index.item])
            return false;

        assert(bucket->generations[index.item] & USED_BIT);
        bucket->generations[index.item] += 1;
        bucket->generations[index.item] &= ~USED_BIT;
        T* bucket_data = BUCKET_DATA(bucket);
        bucket_data[index.item].~T();

        *(uint32_t*) (void*) (bucket_data + index.item) = bucket_array->_first_free;
        bucket_array->_first_free = handle.index;
        bucket_array->_size -= 1;
        
        assert(is_invariant(*bucket_array));
        return true;
    }
    

    template <typename T>
    T* get(Weak_Bucket_Array<T>* from, Weak_Handle handle, T* if_not_found) noexcept
    {
        using namespace weak_bucket_array_internal;

        Weak_Bucket_Index index = to_index(*from, handle);
        Bucket* bucket = from->_buckets + index.bucket;

        if(index.bucket > from->_size || bucket->generations[index.item] != handle.generation)
            return if_not_found;

        return BUCKET_DATA(bucket) + index.item;
    }
    
    template <typename T>
    T const& get(Weak_Bucket_Array<T> const& from, Weak_Handle handle, T const& if_not_found) noexcept
    {
        Weak_Bucket_Array<T>* cheated_arr = (Weak_Bucket_Array<T>*) (void*) &from;
        T* cheated_val = (T*) (void*) &if_not_found;
        return *get(cheated_arr, handle, cheated_val);
    }
}
