#pragma once 

#include <cassert>
#include <ranges>

#include "meta.h"
#include "types.h"
#include "defines.h"

namespace jot
{
    namespace stdr = std::ranges;
    namespace stdv = std::views;  

    using std::move;
    using std::forward;
    using std::begin;
    using std::end;
    using std::size;

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

    using IRange = Range<tsize>;

    func is_invarinat(IRange range) -> bool {
        return range.from <= range.to;
    }

    func in_range(IRange range, tsize index) {
        return (range.from <= index && index < range.to);
    }

    func in_inclusive_range(IRange range, tsize index) {
        return (range.from <= index && index <= range.to);
    }

    func sized_range(tsize from, tsize size) -> IRange {
        return IRange{from, from + size};
    }
}

namespace std 
{
    func begin(jot::direct_container auto& arr) noexcept {return arr.data;}
    func begin(const jot::direct_container auto& arr) noexcept {return arr.data;}

    func end(jot::direct_container auto& arr) noexcept {return arr.data + arr.size;}
    func end(const jot::direct_container auto& arr) noexcept {return arr.data + arr.size;}

    func cbegin(const jot::direct_container auto& arr) noexcept {return arr.data;}
    func cend(const jot::direct_container auto& arr) noexcept {return arr.data + arr.size;}

    func size(const jot::direct_container auto& arr) noexcept {return arr.size;}

    template <typename T>
    func size(const jot::Range<T>& range) noexcept -> T {return range.from - range.to;}
}


namespace jot
{
    using ::std::begin;
    using ::std::end;
    using ::std::size;

    func strlen(cstring str) noexcept -> tsize
    {
        tsize size = 0;
        while(str[size] != '\0')
        {
            size++;
        }
        return size;
    };


    template<typename T, typename Size = tsize>
    struct Slice
    {
        T* data = nullptr;
        Size size = 0;

        constexpr Slice() = default;
        constexpr Slice(T* data, Size size) 
            : data(data), size(size) {}
        constexpr Slice(const char* strl) requires same<T, const char> 
            : data(strl), size(strlen(strl)) {}

        constexpr operator Slice<const T, Size>() const noexcept { return Slice<const T, Size>{this->data, this->size}; }
        constexpr bool operator ==(Slice const&) const noexcept = default;
        constexpr bool operator !=(Slice const&) const noexcept = default;

        #include "slice_op_text.h"
    };

    template<typename T, typename S>
    Slice(T*, S) -> Slice<T, tsize>;

    using String = Slice<const char>;

    Slice(const char*) -> String;

    func slice(cstring str) -> String {
        return {str, strlen(str)};
    }

    template<typename T>
    func slice(Slice<T> sliced) -> Slice<T> {
        return sliced;
    }

    template<typename T>
    func is_invarinat(Slice<T> slice) -> bool {
        return slice.size >= 0;
    }

    template<typename T>
    func slice(Slice<T> slice, tsize from) -> Slice<T> {
        assert((0 <= from && from <= slice.size) && "index out of bounds");
        return Slice<T>{slice.data + from, slice.size - from};
    }

    template<typename T>
    func trim(Slice<T> slice, tsize to_index) -> Slice<T> {   
        assert((0 <= to_index && to_index <= slice.size) && "index out of bounds");
        return Slice<T>{slice.data, to_index};
    }

    template<typename T> 
    func slice_size(Slice<T> base_slice, tsize from, tsize size) -> Slice<T> {
        return trim(slice(base_slice, from), size);
    }

    template<typename T> 
    func slice_range(Slice<T> base_slice, tsize from, tsize to) -> Slice<T> {
        return slice(trim(base_slice, to), from);
    }

    template<typename T> 
    func slice(Slice<T> base_slice, IRange range) -> Slice<T> {
        return slice_range(base_slice, range.from, range.to);
    }

    template<typename T>
    func byte_size(Slice<T> slice) -> isize {
        return slice.size * sizeof(T);
    }

    template<typename To_T, typename To_Size = tsize, typename From_T = int, typename From_Size = tsize>
    func cast_slice(Slice<From_T, From_Size> slice) -> Slice<To_T, To_Size> 
    {
        if constexpr (std::is_convertible_v<From_T*, To_T*>)
            return {cast(To_T*) slice.data, cast(To_Size) slice.size};
        else
            return {cast(To_T*) cast(void*) slice.data, cast(To_Size) (byte_size(slice) / cast(tsize) sizeof(To_T))};
    }

    template<typename T>
    func are_aliasing(Slice<const T> left, Slice<const T> right) noexcept -> bool
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

    template<typename T>
    func are_one_way_aliasing(Slice<const T> before, Slice<const T> after) noexcept -> bool
    { 
        return (before.data + before.size > after.data) && (after.data > before.data);
    }

}


#include "undefs.h"