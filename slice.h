#pragma once 

//#include <cstddef>
#include <cassert>
//#include <utility>
#include <ranges>
//@TODO: How many do i need?

//#include <limits>


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

    template<class Tag>
    struct Begin_End
    {
        i64 val = 0;

        template <typename T> 
            requires requires() {cast(T) val;}
        constexpr explicit operator T() const {return cast(T)val;}

        constexpr Begin_End operator +(let& value) const { return {cast(i64)(val + value)}; } 
        constexpr Begin_End operator -(let& value) const { return {cast(i64)(val - value)}; }  
    };

    struct PerElementDummy {};
    struct Static_Container_Tag {};

    using Begin = Begin_End<void>;
    using End = Begin_End<char>;

    constexpr static Begin BEGIN;
    constexpr static End   END;

    enum class Extent : u8 {Dynamic};
    constexpr static Extent DYNAMIC_EXTENT = Extent::Dynamic; 

    using Def_Size = size_t;

    template <class T>
    using Def_Alloc = std::allocator<T>;

    template<typename Container>
    concept direct_container = requires(Container container)
    {
        { container.data } -> std::convertible_to<void*>;
        { container.size } -> std::convertible_to<size_t>;
        requires(!same<decltype(container.data), void>);
    };

    template<typename Container>
    concept static_direct_container = direct_container<Container> && Tagged<Container, Static_Container_Tag>;
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
        using Extent_Const = Const<DYNAMIC_EXTENT, Extent>;

        template<typename T, typename Size, typename Extent>
        struct Slice_Data
        {
            using tag_type = Static_Container_Tag;

            T* data = nullptr;
            constexpr static Size size = cast(Size) Extent::value;
            constexpr static Size capacity = cast(Size) Extent::value;

            constexpr bool operator==(const Slice_Data&) const noexcept = default;
        };

        template<typename T, typename Size>
        struct Slice_Data<T, Size, Extent_Const>
        {
            using tag_type = void;

            T* data = nullptr;
            Size size = 0;
            constexpr static Size capacity = std::numeric_limits<Size>::max();

            constexpr bool operator==(const Slice_Data&) const noexcept = default;
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
        concept matching_iter = same<Iter_Value<It>, T> || same<Iter_Value<It>, std::remove_const_t<T>>;

        template<class R, typename T>
        concept matching_range = same<Range_Value<R>, T> || same<Range_Value<R>, std::remove_const_t<T>>;


        template <typename T> 
        concept literal_compatible = same<T, const char8_t> || same<T, const char>;
    }

    func strlen(cstring str) noexcept 
    {
        size_t size = 0;
        while(str[size] != '\0')
        {
            size++;
        }
        return size;
    };

    template<typename T, std::integral Size = Def_Size, auto extent = DYNAMIC_EXTENT>
    struct Slice_ : detail::Slice_Data<T, Size, Const<extent, decltype(extent)>>
    {
        using Slice_Data = detail::Slice_Data<T, Size, Const<extent, decltype(extent)>>;
        using slice_type = Slice_<T, Size>;
        using const_slice_type = Slice_<const T, Size>;

        constexpr static bool is_static = !same<decltype(extent), Extent>;

        constexpr Slice_() = default;

        //Dynamic
        constexpr Slice_(cstring str) noexcept
            requires detail::literal_compatible<T>
        : Slice_Data{str, strlen(str)} {}

        template <detail::cont_iter It> 
            requires (!is_static) && detail::matching_iter<It, T>
        constexpr Slice_(It it, Size size) noexcept
            : Slice_Data{ detail::addr(it), size } {};

        template <detail::cont_iter Begin, detail::cont_iter End>  
            requires (!is_static) && detail::matching_iter<Begin, T>
        constexpr Slice_(Begin begin, End end) noexcept
            : Slice_Data{ detail::addr(begin), cast(Size) detail::dist(begin, end)} {};

        template <detail::cont_range R> 
            requires (!is_static) && detail::matching_range<R, T>
        constexpr Slice_(R& range) noexcept
            : Slice_Data{ detail::addr(std::begin<R>(range)), cast(Size) detail::dist(std::begin<R>(range), std::end<R>(range)) } 
        {};

        //Static
        template <detail::cont_iter It> 
            requires (is_static) && detail::matching_iter<It, T>
        constexpr Slice_(It it) noexcept
            : Slice_Data{ detail::addr(it) } {};

        template <detail::cont_range R> 
            requires (is_static) && detail::matching_range<R, T>
        constexpr Slice_(R& range) noexcept
            : Slice_Data{ detail::addr<R>(std::begin<R>(range)) } 
        {
            assert(this->size == detail::dist<R>(range));
        };

        template <detail::cont_iter Begin, detail::cont_iter End>  
            requires (is_static) && detail::matching_iter<Begin, T>
        constexpr Slice_(Begin begin, End end) noexcept
            : Slice_Data{ detail::addr(begin)} 
        {
            assert(this->size == detail::dist(begin, end));
        };

        constexpr operator Slice_<T, Size>() const noexcept requires (is_static) { return Slice_<T, Size>{this->data, this->size}; }
        constexpr operator Slice_<T, Size>()       noexcept requires (is_static) { return Slice_<T, Size>{this->data, this->size}; }
        constexpr operator Slice_<const T, Size, extent>() const noexcept        { return Slice_<const T, Size, extent>{this->data, this->size}; }

        #include "slice_op_text.h"
    };

    //deduction guides
    template <detail::cont_iter Begin, class Size>
        requires (!detail::cont_iter<Size>)
    Slice_(Begin begin, Size) -> Slice_<detail::Iter_Value<Begin>>;

    template <detail::cont_iter Begin, class End>
        requires (detail::cont_iter<End>)
    Slice_(Begin begin, End) -> Slice_<detail::Iter_Value<Begin>>;

    template <detail::cont_range Range>
    Slice_(Range) -> Slice_<detail::Range_Value<Range>>;

    template <class T, size_t size>
    Slice_(T (&elem)[size]) -> Slice_<T, size_t, size>;
}


#include "undefs.h"