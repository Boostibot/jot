#pragma once 

#include <cstddef>
#include <cassert>
#include <utility>
#include <ranges>
//@TODO: How many do i need?

#include <limits>


#include "meta.h"
#include "types.h"
#include "defines.h"

namespace jot
{


    namespace stdr { using namespace std::ranges; };
    namespace stdv { using namespace std::views;  };

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
    //func begin(jot::direct_container auto&& arr) noexcept {return arr.data;}
    //func end(jot::direct_container auto&& arr) noexcept {return arr.data + arr.size;}

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
    struct tuple_element<I, Cont> 
    {using type = typename Cont::value_type;};

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


namespace jot
{
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
        func addr(It&& it) noexcept -> decltype(&(*it))
        {
            return &(*it);
        } 

        template <class Range>
        func range_addr(Range&& range) noexcept -> decltype(&(*std::begin(range)))
        {
            return &(*std::begin(range));
        } 

        template<class It>
        concept addressable_iter = requires(It it)
        {   
            addr(it);
        };
        template<class Range>
        concept addressable_range = requires(Range range)
        {   
            range_addr(range);
        };


        template <class R>
        concept cont_range = stdr::contiguous_range<R>;// && addressable_range<R>;
        template <class I>
        concept cont_iter = std::contiguous_iterator<I>;// && addressable_iter<I>;

        template <class Begin, class End>
        func dist(Begin&& begin, End&& end) noexcept -> size_t
        {
            return cast(size_t) (addr(end) - addr(begin));
        }

        template <class R>
        func dist(R&& range) noexcept -> size_t
        {
            return dist(std::begin(range), std::end(range));
        }

        template <typename It>
        using Iter_Value = std::remove_reference_t<decltype(*It())>;

        template <typename R>
        using Range_Value = std::remove_reference_t<decltype(*std::begin(R()))>;

        template<class It, typename T>
        concept matching_iter = are_same_v<Iter_Value<It>, T> || are_same_v<std::remove_const_t<Iter_Value<It>>, T>;

        template<class R, typename T>
        concept matching_range = are_same_v<Range_Value<R>, T> || are_same_v<std::remove_const_t<Range_Value<R>>, T>;
    }

    template<typename T, typename Size = size_t, auto extent = DYNAMIC_EXTENT>
    struct Slice : detail::SliceData<T, Size, Const<extent, decltype(extent)>>
    {
        using SliceData = detail::SliceData<T, Size, Const<extent, decltype(extent)>>;
        using slice_type = Slice<T, Size>;
        using const_slice_type = Slice<const T, Size>;

        constexpr static bool is_static = !are_same_v<decltype(extent), Extent>;

        constexpr Slice() = default;

        //Dynamic
        template <detail::cont_iter It> 
            requires (!is_static) && detail::matching_iter<It, T>
        constexpr Slice(It it, Size size) noexcept
            : SliceData{ detail::addr(it), size } {};

        template <detail::cont_iter Begin, detail::cont_iter End>  
            requires (!is_static) && detail::matching_iter<Begin, T>
        constexpr Slice(Begin begin, End end) noexcept
            : SliceData{ detail::addr(begin), cast(Size) detail::dist(begin, end)} {};

        template <detail::cont_range R> 
            requires (!is_static) // && detail::matching_range<R, T>
        constexpr Slice(R& range) noexcept
            : SliceData{ detail::addr(std::begin<R>(range)), cast(Size) detail::dist(std::begin<R>(range), std::end<R>(range)) } 
        {};

        //Static
        template <detail::cont_iter It> 
            requires (is_static) && detail::matching_iter<It, T>
        constexpr Slice(It it) noexcept
            : SliceData{ detail::addr(it) } {};

        template <detail::cont_range R> 
            requires (is_static) && detail::matching_range<R, T>
        constexpr Slice(R&& range) noexcept
            : SliceData{ detail::addr<R>(std::begin<R>(range)) } 
        {
            assert(this->size == detail::dist<R>(std::forward<R>(range)));
        };

        template <detail::cont_iter Begin, detail::cont_iter End>  
            requires (is_static) && detail::matching_iter<Begin, T>
        constexpr Slice(Begin begin, End end) noexcept
            : SliceData{ detail::addr(begin)} 
        {
            assert(this->size == detail::dist(begin, end));
        };

        constexpr operator Slice<T, Size>() const noexcept requires (is_static) { return Slice<T, Size>{this->data, this->size}; }
        constexpr operator Slice<T, Size>()       noexcept requires (is_static) { return Slice<T, Size>{this->data, this->size}; }

        #include "slice_op_text.h"
    };

    //deduction guides
    template <detail::cont_iter Begin, class Size>
        requires (!detail::cont_iter<Size>)
    Slice(Begin begin, Size) -> Slice<detail::Iter_Value<Begin>>;

    template <detail::cont_iter Begin, class End>
        requires (detail::cont_iter<End>)
    Slice(Begin begin, End) -> Slice<detail::Iter_Value<Begin>>;

    template <detail::cont_range Range>
    Slice(Range) -> Slice<detail::Range_Value<Range>>;

    template <class T, size_t size>
    Slice(T (&elem)[size]) -> Slice<T, size_t, size>;
}


#include "undefs.h"