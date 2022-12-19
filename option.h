#pragma once
#include <cassert>
#include "types.h"
#include "meta.h"
#include "move.h"
#include "defines.h"

namespace jot
{

    template<typename Value_T>
    struct Value {
        Value_T value = Value_T{};
    };

    template<typename Value_T>
    Value(Value_T) -> Value<Value_T>;
    Value() -> Value<Unit>;


    template<typename Error_T>
    struct Error {
        Error_T error = Error_T{};
    };

    template<typename Error_T>
    Error(Error_T) -> Error<Error_T>;
    Error() -> Error<Unit>;

    namespace detail
    {
        template<typename Value, typename Error>
        struct Pointer_Payload
        {
            Value value = Value();
            Error error = Error();

            constexpr Pointer_Payload(bool has, Value value = Value(), Error error = Error())  noexcept
                : value{has ? value : nullptr}, error{error} {}
        };

        template<typename Value, typename Error>
        struct Pointer_Payload_No_Error : Error
        {
            Value value = Value();

            constexpr Pointer_Payload_No_Error(bool has, Value value = Value(), Error = Error()) noexcept
                : value{has ? value : nullptr} {}
        };

        template<typename Value, typename Error>
        struct Regular_Payload
        {
            address_alias Value value = Value();
            address_alias Error error = Error();
            bool has;

            constexpr Regular_Payload(bool has, Value value = Value(), Error error = Error())  noexcept
                : has{has}, value{value}, error{error} {}
        };
    }

    template<typename Value_, typename Error_>
    struct Result
    {
        using Value = Value_;
        using Error = Error_;

        static_assert(regular_type<Value> && regular_type<Error> && "must be a regular type");

        static constexpr bool has_empty_value = std::is_empty_v<Value>;
        static constexpr bool has_empty_error = std::is_empty_v<Error>;
        static constexpr bool is_poiter_specialized = std::is_pointer_v<Value>;
        static constexpr bool is_pointer_no_error_specialized = is_poiter_specialized && has_empty_error;

        static_assert(is_pointer_no_error_specialized ? std::is_class_v<Error> : true);

        static constexpr bool is_trivial_constructible = 
            std::is_trivially_copy_constructible_v<Value> 
            && std::is_trivially_copy_constructible_v<Error>;

        static constexpr bool is_trivial_assignable = 
            std::is_trivially_copy_assignable_v<Value> 
            && std::is_trivially_copy_assignable_v<Error>;

        static constexpr bool is_trivial = is_trivial_constructible && is_trivial_assignable;

        using Selected_Pointer_Payload = std::conditional_t<has_empty_error, 
            detail::Pointer_Payload_No_Error<Value, Error>,
            detail::Pointer_Payload<Value, Error>>;

        using Payload = std::conditional_t<is_poiter_specialized, 
            Selected_Pointer_Payload, 
            detail::Regular_Payload<Value, Error>>;

        Payload payload;

        constexpr Result() noexcept 
            : payload{false} {}

        constexpr Result(Value value) noexcept
            : payload{true, move(&value)} {}

        constexpr Result(jot::Error<Error> error) noexcept
            : payload{true, Value{}, move(&error.error)} {}

        constexpr Result(jot::Value<Value> value) noexcept
            : payload{true, move(&value.value), Error{}} {}

        constexpr Result(bool has, Value value, Error error) noexcept
            : payload{has, move(&value), move(&error)} {}

        constexpr Result(Result moved) noexcept = default;
        constexpr Result(Result in) noexcept requires(is_trivial_constructible) = default;

        constexpr Result& operator =(Result moved) noexcept = default;
        constexpr Result& operator =(Result in) noexcept requires(is_trivial_assignable) = default;
    };

    template <typename Value>
    using Option = Result<Value, Unit>;

    template <typename Value>
    using Error_Option = Result<Unit, Value>;

    template <typename Value>
    Result(Value) -> Option<Value>;


    #define templ_func template <typename Value, typename Error> func

    func has(bool flag) noexcept -> bool {
        return flag;
    }

    templ_func has(Result<Value, Error> in res) noexcept -> bool {
        if constexpr(Result<Value, Error>::is_poiter_specialized)
            return res.payload.value != nullptr;
        else
            return res.payload.has;
    }

    templ_func value(Result<Value, Error> in res) noexcept -> Value in {
        assert(has(res));
        return res.payload.value;
    }

    templ_func value(Result<Value, Error>* res) noexcept -> Value* {
        assert(has(*res));
        return &res.payload.value;
    }

    templ_func value(Result<Value, Error> moved res) noexcept -> Value moved {
        return move(value(&res));
    }

    templ_func error(Result<Value, Error> in res) noexcept -> Error in {
        assert(has(res) == false);
        if constexpr(Result<Value, Error>::is_pointer_no_error_specialized)
            return cast(Error const&) res;
        else
            return &res.payload.error;
    }

    templ_func error(Result<Value, Error>* res) noexcept -> Error* {
        assert(has(*res) == false);
        if constexpr(Result<Value, Error>::is_pointer_no_error_specialized)
            return cast(Error*) res;
        else
            return &res.payload.error;
    }

    templ_func error(Result<Value, Error> moved res) noexcept -> Error moved {
        return move(error(&res));
    }

    templ_func operator==(Result<Value, Error> in left, Result<Value, Error> in right) noexcept -> bool {
        if(has(left) != has(right))
            return false;

        if(has(left))
            return value(left) == value(right);
        else
            return error(left) == error(right);
    }

    templ_func operator!=(Result<Value, Error> in left, Result<Value, Error> in right) noexcept -> bool {
        if(has(left) != has(right))
            return true;

        if(has(left))
            return value(left) != value(right);
        else
            return error(left) != error(right);
    }


    template <typename T>
    concept hasable = requires(T in t) {
        { has(t) } -> std::convertible_to<bool>;
    };

    template <typename T>
    concept unhasable = !hasable<T>;

    #undef templ_func
    #define templ_func template <hasable T> func
    #define templ_proc template <hasable T> proc

    templ_func operator==(T in left, jot::Value<Unit>) noexcept -> bool { return has(left); }
    templ_func operator!=(T in left, jot::Value<Unit>) noexcept -> bool { return has(left) == false; }
    templ_func operator==(T in left, jot::Error<Unit>) noexcept -> bool { return has(left) == false; }
    templ_func operator!=(T in left, jot::Error<Unit>) noexcept -> bool { return has(left); }

    templ_proc force(T in value) -> void {
        if(has(value) == false)
            throw value;
    }

    templ_proc force_error(T in value) -> void
    {
        if(has(value))
            throw value;
    }

    /*
    struct Force {};

    templ_proc operator << (Force, T in value) -> void
    {
        if(has(value) == false)
            throw value;
    }

    templ_proc operator >> (T in value, Force) -> void
    {
        if(has(value) == false)
            throw value;
    }

    static constexpr Force force = Force{};
    */
    #undef templ_func
    #undef templ_proc



}
#include "undefs.h"