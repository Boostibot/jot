#pragma once 

#include <cstddef>
#include <cassert>
#include <utility>
//@TODO: How many do i need?

#include <limits>


#include "meta.h"
#include "types.h"
#include "defines.h"

namespace jot
{
    template<class Tag>
    struct BeginEnd
    {
        i64 val = 0;

        template <typename T> 
            requires requires() {cast(T) val;}
        constexpr explicit operator T() const {return cast(T)val;}

        constexpr BeginEnd operator +(let& value) const { return {cast(i64)(val + value)}; } 
        constexpr BeginEnd operator -(let& value) const { return {cast(i64)(val - value)}; }  
    };

    struct PerElementDummy {};
    struct StaticContainerTag {};

    using Begin = BeginEnd<void>;
    using End = BeginEnd<char>;

    constexpr static Begin BEGIN;
    constexpr static End   END;

    enum class Extent : u8 {Dynamic};
    constexpr static Extent DYNAMIC_EXTENT = Extent::Dynamic; 
    
    namespace detail
    {
        using ExtentConst = Const<DYNAMIC_EXTENT, Extent>;

        template<typename T, typename Size, typename Extent>
        struct SliceData
        {
            using Tag = StaticContainerTag;

            T* data = nullptr;
            constexpr static Size size = cast(Size) Extent::value;
            constexpr static Size capacity = cast(Size) Extent::value;

            constexpr bool operator==(const SliceData&) const noexcept = default;
        };

        template<typename T, typename Size>
        struct SliceData<T, Size, ExtentConst>
        {
            using Tag = void;

            T* data = nullptr;
            Size size = 0;
            constexpr static Size capacity = std::numeric_limits<Size>::max();

            constexpr bool operator==(const SliceData&) const noexcept = default;
        };

        template <class It>
        constexpr auto addr(It&& it) noexcept -> decltype(&(*it))
        {
            return &(*it);
        } 

        template<class It>
        concept addressable = requires(It it)
        {   
            addr(it);
        };

        template<class Begin, class End>
        concept iter_range = requires(Begin begin, End end)
        {
            addr(begin) - addr(end);
        };

        template <typename It>
        using IterValue = std::remove_reference_t<decltype(*It())>;

        template<class It, typename T>
        concept matching_iter = addressable<It> && (are_same_v<IterValue<It>, T> || are_same_v<std::remove_const_t<IterValue<It>>, T>);
    }

    template<typename T, typename Size = i64, auto extent = DYNAMIC_EXTENT>
    struct Slice : detail::SliceData<T, Size, Const<extent, decltype(extent)>>
    {
        using SliceData = detail::SliceData<T, Size, Const<extent, decltype(extent)>>;
        using slice_type = Slice<T, Size>;
        using const_slice_type = Slice<const T, Size>;

        constexpr static bool is_static = !are_same_v<decltype(extent), Extent>;

        constexpr Slice() = default;

        template <class It> requires (is_static) && detail::matching_iter<It, T>
        constexpr Slice(It it) noexcept
            : SliceData{ detail::addr(it) } {};

        template <class It> requires (!is_static) && detail::matching_iter<It, T>
        constexpr Slice(It it, Size size) noexcept
            : SliceData{ detail::addr(it), size } {};

        template <class Begin, class End>  
            requires (!is_static) && detail::matching_iter<Begin, T> && detail::iter_range<Begin, End>
        constexpr Slice(Begin begin, End end) noexcept
            : SliceData{ detail::addr(begin), detail::addr(begin) - detail::addr(end)} {};

        constexpr operator Slice<T, Size>() const noexcept requires (is_static) { return Slice<T, Size>{this->data, this->size}; }
        constexpr operator Slice<T, Size>()       noexcept requires (is_static) { return Slice<T, Size>{this->data, this->size}; }

        #include "span_array_shared_text.h"
    };

    //deduction guide
    template <class Begin, class Size>
        requires (!detail::addressable<Size>)
    Slice(Begin begin, Size) -> Slice<detail::IterValue<Begin>>;

    template <class Begin, class End>
        requires (detail::addressable<End>)
    Slice(Begin begin, End) -> Slice<detail::IterValue<Begin>>;

    template <class T, size_t size>
    Slice(T (&elem)[size]) -> Slice<T, i64, size>;

    template<typename Container>
    concept direct_container = requires(Container container)
    {
        container[0];
        container.size;
    };

    template<typename Container>
    concept static_direct_container = direct_container<Container> && Tagged<Container, StaticContainerTag>;
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

    template<jot::static_direct_container Cont>
    struct tuple_size<Cont> : integral_constant<size_t, Cont::size> {};

    template<size_t I, jot::static_direct_container Cont>
    func get(Cont& arr) noexcept -> typename Cont::value_type&
    {
        static_assert(I < arr.size, "access out of bounds");
        return arr[I];
    }
    template<size_t I, jot::static_direct_container Cont>
    func get(Cont&& arr) noexcept -> typename Cont::value_type&&
    {
        static_assert(I < arr.size, "access out of bounds");
        return move(arr[I]);
    }
    template<size_t I, jot::static_direct_container Cont>
    func get(const Cont& arr) noexcept -> const typename Cont::value_type&
    {
        static_assert(I < arr.size, "access out of bounds");
        return arr[I];
    }
    template<size_t I, jot::static_direct_container Cont>
    func get(const Cont&& arr) noexcept -> const typename Cont::value_type&&
    {
        static_assert(I < arr.size, "access out of bounds");
        return move(arr[I]);
    }

    template<class T, jot::static_direct_container Cont>
    func get(Cont& arr) noexcept -> T&
    {
        static_assert(is_same_v<T, typename Cont::value_type> && arr.size == 1, "exactly one element needs to satisfy the type");
        return arr[0];
    }
    template<class T, jot::static_direct_container Cont>
    func get(Cont&& arr) noexcept -> T&&
    {
        static_assert(is_same_v<T, typename Cont::value_type> && arr.size == 1, "exactly one element needs to satisfy the type");
        return move(arr[0]);
    }

    template<class T, jot::static_direct_container Cont>
    func get(const Cont& arr) noexcept -> const T&
    {
        static_assert(is_same_v<T, typename Cont::value_type> && arr.size == 1, "exactly one element needs to satisfy the type");
        return arr[0];
    }
    template<class T, jot::static_direct_container Cont>
    func get(const Cont&& arr) noexcept -> const T&&
    {
        static_assert(is_same_v<T, typename Cont::value_type> && arr.size == 1, "exactly one element needs to satisfy the type");
        return move(arr[0]);
    }
}

#include "undefs.h"