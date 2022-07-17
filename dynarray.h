#pragma once

#include "utils.h"
#include <memory>
#include <vector>
#include "defines.h"

namespace jot
{
    //
    namespace detail
    {

        template<typename T, bool is_empty>
        struct AllocatorData
        {
            T allocator = T();
        };

        template<typename T>
        struct AllocatorData<T, true>
        {
            static constexpr T allocator = T();
        };

        template<typename T, size_t static_size_>
        struct StaticData
        {
            T static_data[static_size];
        };

        template<typename T>
        struct StaticData<T, 0>
        {
            static constexpr T static_data[1] = {T()};
        };

        template<typename Size, size_t static_size, bool is_static>
        struct CapacityData
        {
            Size capacity = static_size;
        };

        template<typename Size, size_t static_size>
        struct CapacityData<Size, static_size, true>
        {
            static constexpr Size capacity = static_size;
        };

        template<typename T, typename Size, size_t static_size, class Allocator>
        struct DynarrData : AllocatorData<Allocator, std::is_empty_v<Allocator>>
        {
            static constexpr Size static_size = cast(Size) static_size_;

            T* data = nullptr;
            Size size = 0;
            Size capacity = static_size;

            T static_data[static_size];

            func operator<=>(const DynarrData&) const noexcept = default;
        };

        template<typename T, typename Size, class Allocator>
        struct DynarrData<T, Size, 0, Allocator> : AllocatorData<Allocator, std::is_empty_v<Allocator>>
        {
            static constexpr Size static_size = 0;

            T* data = nullptr;
            Size size = 0;
            Size capacity = static_size;

            static constexpr T static_data[1] = {T()};

            func operator<=>(const DynarrData&) const noexcept = default;
        };
    }

    static constexpr size_t DYNARR_DEFAULT_TOTAL_BYTE_SIZE = 64;

    template <class T, class Size, class Alloc>
    static constexpr size_t DYNARR_RESERVED_BYTE_SIZE = sizeof(T*) + 0;

    template <typename T, typename Size = size_t, size_t static_size = DYNARR_STATIC_BYTES / sizeof(T), typename AllocatorT = std::allocator<T>>
    struct Dynarr : DynarrData<T, static_size, Size, AllocatorT>
    {
        using value_type      = T;
        using size_type       = Size;
        using difference_type = ptrdiff_t;
        using pointer         = T*;
        using const_pointer   = const T*;
        using reference       = T&;
        using const_reference = const T&;

        using Size = Size;

        Dynarr() = default;

        Dynarr(Size size) noexcept
        {
            if(size <= static_size)
                return;

            data = (T*)(void*)(new char[size * sizeof(T)]);
            capacity = size;
        }

        Dynarr(const Dynarr& other)
        {
            Reallocate(other.size);
            size = other.size;

            for (Size i = 0; i < size; i++)
                new (data + i) T(other.data[i]);
        }

        Dynarr(Dynarr&& other) noexcept
        {
            swap(other);
        }

        ~Dynarr()
        {
            for (Size i = 0; i < size; i++)
                data[i].~T();

            if (data != nullptr)
                delete[](char*)(void*)data;
        }

        Dynarr& operator=(Dynarr vec)
        {
            swap(vec);
            return *this;
        }

              T& operator[](Size index)       noexcept { return data[index]; }
        const T& operator[](Size index) const noexcept { return data[index]; }

        public:
        void swap(Dynarr& other) noexcept
        {
            std::swap(data, other.data);
            std::swap(size, other.size);
            std::swap(capacity, other.capacity);
            std::swap(allocator, other.allocator);

            if constexpr()
            std::swap(allocator, other.allocator);
        }

        bool reserve(Size toFit)
        {
            if (capacity >= toFit)
                return false;
            if (capacity == 0)
                Reallocate(toFit);
            else
            {
                Size reallocTo = capacity;
                while(reallocTo < toFit)
                    reallocTo *= 2;

                Reallocate(reallocTo);
            }
            return true;
        }

        void clear()
        {
            Dynarr fresh;
            swap(fresh);
        }

        void push(T what)
        {
            reserve(size + 1);
            new (data + size) T(move(what));
            size++;
        }

        T pop()  
        {
            if (size == 0)
                return T();

            size--;

            T ret = move(data[size]);
            return ret;
        }

        T remove(Size at)
        {
            if (size == 0)
                return T();

            for (Size i = at; i < size - 1; i++)
                data[i] = move(data[i + 1]);

            return pop();
        }

        void resize(Size to, T fillWith = T())
        {
            reserve(to);
            for (Size i = size; i < to; i++)
                new (data + i) T(fillWith);

            for (Size i = to; i < size; i++)
                data[i].~T();
            size = to;
        }

        void grow(Size to, T fillWith = T())
        {
            reserve(to);
            for (Size i = size; i < to; i++)
                new (data + i) T(fillWith);
            if (to > size)
                size = to;
        }

        void shift_left(SignedSize by, Size from, Size to)
        {
            for (Size i = from + by; i < to; i++)
                swap(data[i], data[i - by]);
        }

        void shift_right(SignedSize by, Size from, Size to)
        {
            Size end = from + by;
            for (Size i = to; i-- > end; )
                swap(data[i], data[i - by]);
        }

        void shift(SignedSize by, Size from, Size to)
        {
            if(by > 0)
                return shift_right((Size)by, from, to);
            else
                return shift_left((Size)(-by), from, to);
        }
        void shift(SignedSize by) {return shift(by, 0, size);}

        void insert(Size at, T&& what)
        {
            push(move(what));

            if (size == 1)
                return;

            for (Size i = size - 1; i > at; i--)
                swap(data[i], data[i - 1]);
        }

        template<typename Out>
        friend Out& operator<<(Out& out, const Dynarr<T>& vector)
        {
            out << "{ s:" << vector.size() << ", c:" << vector.capacity() << ", [";
            if(vector.size() > 0)
            {
                out << vector[0];
                for (Size i = 1; i < vector.size(); i++)
                    out << ", " << vector[i];
            }
            return out << "]}";
        }

        void Reallocate(Size to)
        {
            Dynarr newVector(to);

            for (Size i = 0; i < this->size; i++)
                new (newVector.data + i) T(move(data[i]));
            newVector.size = size;

            swap(newVector);
        }
    };
}

#include "undefs.h"