#pragma once 

#include <cstring>
#include <cassert>
#include <cstdint>

//I dont know how to get rid of the dependency on these two:
#include <type_traits>
#include <concepts>
#include <iterator>

#define cast(...) (__VA_ARGS__)
#define nodisc [[nodiscard]]

namespace jot
{ 
    #ifndef JOT_SIZE_T
        using isize = ptrdiff_t;
    #endif

    template<typename Container>
    concept direct_container = requires(Container container)
    {
        { container.data } -> std::convertible_to<void*>;
        { container.size } -> std::convertible_to<size_t>;
        requires(!std::is_same_v<decltype(container.data), void*>);
    };
}

namespace std 
{
    nodisc constexpr auto begin(jot::direct_container auto& arr)        noexcept {return arr.data;}
    nodisc constexpr auto begin(const jot::direct_container auto& arr)  noexcept {return arr.data;}

    nodisc constexpr auto end(jot::direct_container auto& arr)          noexcept {return arr.data + arr.size;}
    nodisc constexpr auto end(const jot::direct_container auto& arr)    noexcept {return arr.data + arr.size;}

    nodisc constexpr auto cbegin(const jot::direct_container auto& arr) noexcept {return arr.data;}
    nodisc constexpr auto cend(const jot::direct_container auto& arr)   noexcept {return arr.data + arr.size;}

    nodisc constexpr auto size(const jot::direct_container auto& arr)   noexcept {return arr.size;}
    nodisc constexpr auto data(const jot::direct_container auto& arr)   noexcept {return arr.data;}
}

namespace jot
{
    using ::std::begin;
    using ::std::end;
    using ::std::size;

    nodisc constexpr 
    isize strlen(const char* str)
    {
        isize size = 0;
        while(str[size] != '\0')
        {
            size++;
        }
        return size;
    };

    template<typename T>
    struct Slice
    {
        T* data = nullptr;
        isize size = 0;

        constexpr Slice() = default;
        constexpr Slice(T* data, isize size) 
            : data(data), size(size) {}

        constexpr Slice(const char* strl) requires std::is_same_v<T, const char> 
            : data(strl), size(strlen(strl)) {}

        constexpr operator Slice<const T>() const noexcept { 
            return Slice<const T>{this->data, this->size}; 
        }

        constexpr bool operator ==(Slice const&) const noexcept = default;
        constexpr bool operator !=(Slice const&) const noexcept = default;

        #define DATA data
        #define SIZE size
        #include "slice_op_text.h"
        #undef DATA
        #undef SIZE
    };

    Slice(const char*) -> Slice<const char>;

    nodisc constexpr 
    Slice<const char> slice(const char* str) 
    {
        return {str, strlen(str)};
    }

    template<direct_container Cont> nodisc constexpr 
    auto slice(Cont const& sliced) 
    {
        using T_ref = decltype(*sliced.data);
        using T = std::remove_reference_t<T_ref>;
        return Slice<T>{sliced.data, sliced.size};
    }

    template<direct_container Cont> nodisc constexpr 
    auto slice(Cont* sliced) 
    {
        using T_ref = decltype(*sliced->data);
        using T = std::remove_reference_t<T_ref>;
        return Slice<T>{sliced->data, sliced->size};
    }

    #define constexpr_assert(a) (std::is_constant_evaluated() ? (void)0 : assert(a))

    template<typename T> nodisc constexpr  
    bool is_invarinat(Slice<T> slice) {
        return slice.size >= 0;
    }

    template<typename T> nodisc constexpr 
    Slice<T> slice(Slice<T> slice, isize from) {
        constexpr_assert((0 <= from && from <= slice.size) && "index out of bounds");
        return Slice<T>{slice.data + from, slice.size - from};
    }

    template<typename T> nodisc constexpr 
    Slice<T> trim(Slice<T> slice, isize to_index) {   
        constexpr_assert((0 <= to_index && to_index <= slice.size) && "index out of bounds");
        return Slice<T>{slice.data, to_index};
    }

    template<typename T> nodisc constexpr 
    Slice<T> slice_size(Slice<T> base_slice, isize from, isize size) {
        return trim(slice(base_slice, from), size);
    }

    template<typename T> nodisc constexpr 
    Slice<T> slice_range(Slice<T> base_slice, isize from, isize to) {
        return slice(trim(base_slice, to), from);
    }

    template<typename T> nodisc constexpr 
    isize byte_size(Slice<T> slice) {
        return slice.size * cast(isize) sizeof(T);
    }

    template<typename To, typename From = int> nodisc constexpr
    Slice<To> cast_slice(Slice<From> slice)
    {
        if constexpr (std::is_convertible_v<From*, To*>)
            return {cast(To*) slice.data, slice.size};
        else
            return {cast(To*) cast(void*) slice.data, (byte_size(slice) / cast(isize) sizeof(To))};
    }

    template<typename T> nodisc constexpr 
    bool are_aliasing(Slice<T> left, Slice<T> right)
    { 
        uintptr_t left_pos = cast(uintptr_t) left.data;
        uintptr_t right_pos = cast(uintptr_t) right.data;
        if(right_pos < left_pos)
        {
            uintptr_t diff = left_pos - right_pos;
            return diff < cast(uintptr_t) right.size;
        }
        else
        {
            uintptr_t diff = right_pos - left_pos;
            return diff < cast(uintptr_t) left.size;
        }
    }

    template<typename T> nodisc constexpr 
    bool are_one_way_aliasing(Slice<T> before, Slice<T> after)
    { 
        return (before.data + before.size > after.data) && (after.data > before.data);
    }

    template<typename T> constexpr 
    void fill(Slice<T>* to , T const& with) noexcept
    {
        for(isize i = 0; i < to->size; i++)
            to->data[i] = with;
    }

    template<typename T> constexpr 
    void null_bytes(Slice<T>* to) noexcept
    {
        if(std::is_constant_evaluated() == false)
        {
            memset(to->data, 0, to->size * sizeof(T));
            return;
        }

        fill(to, cast(T) 0);
    }

    template<typename T> nodisc constexpr 
    int compare(Slice<T> a, Slice<T> b, bool byte_by_byte = false) noexcept
    {
        if(a.size < b.size)
            return -1;

        if(a.size > b.size)
            return 1;

        if(byte_by_byte && std::is_constant_evaluated() == false)
            return memcmp(a.data, b.data, a.size * sizeof(T));

        for(isize i = 0; i < a.size; i++)
        {
            if(a[i] < b[i])
                return -1;

            if(a[i] > b[i])
                return 1;
        }

        return 0;
    }

    template<typename T> nodisc constexpr 
    int compare_bytes(Slice<T> a, Slice<T> b) noexcept
    {
        return compare(a, b, true);
    }

    template<typename T> constexpr 
    void copy_bytes(Slice<T>* to, Slice<const T> from) noexcept
    {
        //we by default use memmove since its safer
        // (memcpy is still availible though under longer, uglier name)
        if(std::is_constant_evaluated() == false)
        {
            assert(to->size >= from.size && "size must be big enough");
            memmove(to->data, from.data, from.size * sizeof(T));
            return;
        }

        if(to->data < from.data)
        {
            for(isize i = 0; i < from.size; i++)
                to->data[i] = from.data[i];
        }
        else
        {
            for(isize i = from.size; i-- > 0;)
                to->data[i] = from.data[i];
        }
    }

    template<typename T> constexpr 
    void copy_bytes_no_alias(Slice<T>* to, Slice<const T> from) noexcept
    {
        if(std::is_constant_evaluated() == false)
        {
            assert(are_aliasing(*to, from) == false && "must not alias");
            assert(to->size >= from.size && "size must be big enough");

            memcpy(to->data, from.data, from.size * sizeof(T));
        }
        else
        {
            for(isize i = 0; i < from.size; i++)
                to->data[i] = from.data[i];
        }
    }
}

#undef nodisc
#undef cast
