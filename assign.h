#pragma once

#include "meta.h"
#include "option.h"
#include "defines.h"

namespace jot
{

    struct Default_Assign {};

    template<typename T, typename select = True> 
    struct Assign : Default_Assign
    {
        // select is used for conditional implementations like with std::conditional_t. 
        // For example Result<Val, Err> only requires specialization if the value or err are not trivial
        //  without this we would require for every combination of types (even trivial ones) a implementation.
        //  We could also select it using std::conditional_t but that would still result in an instantiation 
        //  (because std::conditional_t accepts the types as arguments) which would slow down compilation needlessly
        // 
        // Only value of True used for assign and copy procs so every other value will not get selected.
        // (we could have used bool switch but that would work if the value would be dependant on previous template types
        //  for to me unknown c++ reasons)

        static_assert(innert_type<T>, "is not elligable for default! provide a custom specialization of this template");

        using Error_Type = Unit;
        static proc perform(T* to, T in from, Error_Type* error) noexcept -> bool
        {
            *to = from;
            return true;
        }
    };

    template<typename T>
    constexpr bool has_default_assign = std::is_base_of_v<Default_Assign, Assign<T>>;

    template<typename T>
    constexpr bool has_bit_by_bit_assign = has_default_assign<T> && std::is_trivially_copyable_v<T>;

    template <typename T>
    using Assign_Error = Assign<T>::Error_Type;

    template<typename T>
    func assign(T* to, no_infer(T) in from) -> Result<T*, Assign_Error<T>>
    {   
        static_assert(regular_type<T> && "type must be regular");
        static_assert(innert_type<Assign_Error<T>> && "error must be not error prone");

        Assign_Error<T> error = {};
        bool ok = Assign<T, True>::perform(to, from, &error);

        return {move(&ok), move(&to), move(&error)};
    }

    template<typename T>
    func construct_assign_at(T* to, no_infer(T) in from) -> Result<T*, Assign_Error<T>>
    {
        std::construct_at(to);
        return assign<T>(to, from);
    }

    template<typename T>
    func copy(T in from) -> Result<T, Assign_Error<T>>
    {
        T value = {};
        Assign_Error<T> error = {};
        bool ok = Assign<T, True>::perform(&value, from, &error);

        return {move(&ok), move(&value), move(&error)};
    }
    
    //assign for Result: (because in option we couldnt yet include assing.h since assign.h includes option.h)
    template<typename Value_T, typename Error_T>
    struct Assign<
        Result<Value_T, Error_T>, 
        std::conditional_t<Result<Value_T, Error_T>::is_trivial, False, True>
    >
    {
        using Result = Result<Value_T, Error_T>;

        struct Error_Type  
        {
            Assign_Error<Value_T> value_error;
            Assign_Error<Error_T> error_error; //Wow such name
        };
        static proc perform(Result* to, Result in from, Error_Type* error) noexcept -> bool
        {
            if(from == Value())
            {
                mut val_res = copy(value(from));
                if(val_res == Error())
                {
                    *error.value_error = error(move(&val_res));
                    return false;
                }
                
                to = Value(value(move(&val_res)));
                return true;
            }
            else
            {
                mut err_res = copy(error(from));
                if(err_res == Error())
                {
                    *error.error_error = error(move(&err_res));
                    return false;
                }
                
                to = Error(value(move(&err_res)));
                return true;
            }
        }
    };

}

#include "undefs.h"