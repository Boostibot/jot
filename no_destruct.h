#pragma once

namespace jot
{
    //A rather nieche but necessary struct that only constructs and never destructs
    // This is necessary if we want to for example: allocate many std::strings in a row 
    // and let the allocator do the cleaning up all at once (arena allocation). 
    //Breaks all invarinats about the type! Use with caution! 
    template<typename T>
    struct alignas(alignof(T)) No_Destruct 
    {
        unsigned char bytes[sizeof(T)] = {0};

        No_Destruct() noexcept 
        {
            new (bytes) T;
        }

        No_Destruct(T && value) noexcept 
        {
            new (bytes) T((T&&) value);
        }
    };

    template<typename T>
    No_Destruct(T) -> No_Destruct<T>;

    template<typename T> 
    T const& get(No_Destruct<T> const& item) 
    {
        return (T*) (void*) item.bytes;
    }

    template<typename T>
    T* get(No_Destruct<T>* item) 
    {
        return *(const T*) (void*) item->bytes;
    }
}
