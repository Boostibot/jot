#pragma once

namespace jot
{
    template<typename T>
    struct alignas(alignof(T)) No_Destruct 
    {
        u8 bytes[sizeof(T)] = {0};

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
