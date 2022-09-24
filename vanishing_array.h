#pragma once

#include "utils.h"
#include "defines.h"
namespace jot
{
    //A another array impl that when empty compiles into an empty struct (with only static constexpr members)
    //  this is useful when using arrays as memebrs of struct with size dependent on a template argument
    //  use with [[no_unique_address]] to let the compiler know its safe to do this
    //
    //Has one dissadvantage over regular arrays: when zero sized we cannot take a non const ptr to its data
    //  this can can cause our code to not compile when calling functions depending on it such as the following code: 
    //  1.    memset(vanishing_arr.data, 0, vanishing_arr.size) 
    // 
    //We can fix this by wrapping it in constexpr if:
    //  1.    if constexpr(vanishing_arr.size > 0)
    //  2.        memset(vanishing_arr.data, 0, vanishing_arr.size) 
    namespace detail
    {
        template<typename T, typename SizeVal, typename Size>
        struct Array_Data
        {
            static constexpr Size size = cast(Size) SizeVal::value;
            static constexpr Size capacity = size;
            T data[size];

            func operator<=>(const Array_Data&) const noexcept = default;
        };

        template<typename T, typename Size>
        struct Array_Data<T, Const<0, size_t>, Size>
        {
            static constexpr Size size = 0;
            static constexpr Size capacity = 0;
            static constexpr T data[1] = {T()};

            func operator<=>(const Array_Data&) const noexcept = default;
        };
    }

    template<typename T, size_t size, typename Size = Def_Size>
    struct Vanishing_Array_ : detail::Array_Data<T, Const<size, size_t>, Size>
    {
        using Tag = StaticContainerTag;
        using Array_Data = detail::Array_Data<T, Const<size>, Size>;

        constexpr Vanishing_Array_() = default;
        constexpr Vanishing_Array_(const T&) noexcept requires (size == 0) : Array_Data{} {}

        template <typename ...Args> 
            requires (size != 0) && (std::is_integral_v<T> == false) && are_same_v<Args...>
        constexpr Vanishing_Array_(Args&&... args) noexcept : Array_Data{std::forward<Args>(args)...} {}

        template <typename ...Args> 
            requires (size != 0) && (std::is_integral_v<T> == true) && (std::is_integral_v<Args> && ...)
        constexpr Vanishing_Array_(Args... args) noexcept : Array_Data{cast(T)(args) ...} {}

        constexpr operator T*()             noexcept { return this->data; }
        constexpr operator const T*() const noexcept { return this->data; }

        using slice_type = Slice<T, Size>;

        #include "slice_op_text.h"
    };

    //deduction guide
    template <class First, class... Rest>
    Vanishing_Array_(First, Rest...) -> Vanishing_Array_<First, 1 + sizeof...(Rest)>;

    //Adds self to span
    template <class T, size_t size, class Size>
    Slice(Vanishing_Array_<T, size, Size>) -> Slice<T, Size, size>;
}

#include "undefs.h"