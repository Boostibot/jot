#pragma once

#include "defines.h"

namespace jot
{

    template<typename T>
    struct alignas(alignof(T)) No_Destruct 
    {
        u8 bytes[sizeof(T)] = {0};

        No_Destruct() noexcept {
            new (bytes) T;
        }

        No_Destruct(T moved value) noexcept {
            new (bytes) T(move(&value));
        }
    };

    template<typename T>
    No_Destruct(T) -> No_Destruct<T>;

    template<typename T>
    func get(No_Destruct<T> in item) noexcept -> T* {
        return cast(T*) cast(void*) item.bytes;
    }

    template<typename T>
    func get(No_Destruct<T>* item) noexcept -> T in{
        return *cast(const T*) cast(void*) item->bytes;
    }
}

#include "undefs.h"