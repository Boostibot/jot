#pragma once 

#include <cassert>
#include <ranges>

#include "traits.h"
#include "types.h"
#include "defines.h"

namespace jot
{
    namespace stdr = std::ranges;
    namespace stdv = std::views;  

    template<typename Container>
    concept direct_container = requires(Container container)
    {
        { container.data } -> std::convertible_to<void*>;
        { container.size } -> std::convertible_to<size_t>;
        requires(!same<decltype(container.data), void*>);
    };

    template<typename T>
    struct Range 
    {
        T from;
        T to;
    };

    using IRange = Range<isize>;

    constexpr func is_invarinat(IRange range) -> bool {
        return range.from <= range.to;
    }

    constexpr func in_range(IRange range, isize index) {
        return (range.from <= index && index < range.to);
    }

    constexpr func in_inclusive_range(IRange range, isize index) {
        return (range.from <= index && index <= range.to);
    }

    constexpr func sized_range(isize from, isize size) -> IRange {
        return IRange{from, from + size};
    }
}

namespace std 
{
    constexpr func begin(jot::direct_container auto& arr) noexcept {return arr.data;}
    constexpr func begin(const jot::direct_container auto& arr) noexcept {return arr.data;}

    constexpr func end(jot::direct_container auto& arr) noexcept {return arr.data + arr.size;}
    constexpr func end(const jot::direct_container auto& arr) noexcept {return arr.data + arr.size;}

    constexpr func cbegin(const jot::direct_container auto& arr) noexcept {return arr.data;}
    constexpr func cend(const jot::direct_container auto& arr) noexcept {return arr.data + arr.size;}

    constexpr func size(const jot::direct_container auto& arr) noexcept {return arr.size;}
    constexpr func data(const jot::direct_container auto& arr) noexcept {return arr.data;}

    template <typename T>
    constexpr func size(const jot::Range<T>& range) noexcept -> T {return range.from - range.to;}
}


namespace jot
{
    using ::std::begin;
    using ::std::end;
    using ::std::size;

    constexpr func strlen(cstring str) noexcept -> isize
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
        constexpr Slice(const char* strl) requires same<T, const char> 
            : data(strl), size(strlen(strl)) {}

        constexpr operator Slice<const T>() const noexcept { return Slice<const T>{this->data, this->size}; }
        constexpr bool operator ==(Slice const&) const noexcept = default;
        constexpr bool operator !=(Slice const&) const noexcept = default;

        #include "slice_op_text.h"
    };

    template<typename T>
    Slice(T*, isize) -> Slice<T>;

    using String = Slice<const char>;

    Slice(const char*) -> String;

    constexpr func slice(cstring str) -> String {
        return {str, strlen(str)};
    }

    #define templ_func template<typename T> constexpr func

    templ_func slice(Slice<T> sliced) -> Slice<T> {
        return sliced;
    }

    templ_func is_invarinat(Slice<T> slice) -> bool {
        return slice.size >= 0;
    }

    templ_func slice(Slice<T> slice, isize from) -> Slice<T> {
        assert((0 <= from && from <= slice.size) && "index out of bounds");
        return Slice<T>{slice.data + from, slice.size - from};
    }

    templ_func trim(Slice<T> slice, isize to_index) -> Slice<T> {   
        assert((0 <= to_index && to_index <= slice.size) && "index out of bounds");
        return Slice<T>{slice.data, to_index};
    }

    templ_func slice_size(Slice<T> base_slice, isize from, isize size) -> Slice<T> {
        return trim(slice(base_slice, from), size);
    }

    templ_func slice_range(Slice<T> base_slice, isize from, isize to) -> Slice<T> {
        return slice(trim(base_slice, to), from);
    }

    templ_func slice(Slice<T> base_slice, IRange range) -> Slice<T> {
        return slice_range(base_slice, range.from, range.to);
    }

    templ_func byte_size(Slice<T> slice) -> isize {
        return slice.size * sizeof(T);
    }

    template<typename To_T, typename From_T = int>
    constexpr func cast_slice(Slice<From_T> slice) -> Slice<To_T> 
    {
        if constexpr (std::is_convertible_v<From_T*, To_T*>)
            return {cast(To_T*) slice.data, slice.size};
        else
            return {cast(To_T*) cast(void*) slice.data, (byte_size(slice) / cast(isize) sizeof(To_T))};
    }

    templ_func are_aliasing(Slice<const T> left, Slice<const T> right) noexcept -> bool
    { 
        uintptr_t left_pos = cast(uintptr_t) left.data;
        uintptr_t right_pos = cast(uintptr_t) right.data;
        if(right_pos < left_pos)
        {
            //[ right ]      [ left ]
            //[ ?? right size ?? ]

            uintptr_t diff = left_pos - right_pos;
            return diff < cast(uintptr_t) right.size;
        }
        else
        {
            //[ left ]      [ right ]
            //[ ?? left size ?? ]
            uintptr_t diff = right_pos - left_pos;
            return diff < cast(uintptr_t) left.size;
        }
    }

    templ_func are_one_way_aliasing(Slice<const T> before, Slice<const T> after) noexcept -> bool
    { 
        return (before.data + before.size > after.data) && (after.data > before.data);
    }

    #undef templ_func
}


#include "undefs.h"