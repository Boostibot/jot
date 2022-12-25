#pragma once
#include "meta.h"

namespace meta
{
    //We can use the same trick we used to obtain type_id (ptr to static variable is guaranteed to be unique)
    // to ensure a unique valued constants. This can be used to create 'open enums' - enums that are freely 
    // extended outside of their definitions but still function as unique conectors between the constant name and its value
    //for example if we just used plain values for this purpose
    #if 0
    namespace open_enum_example
    {
        namespace my_open_enum
        {
            static const int OK = 0;
            static const int ERROR = 1;
        }

        //someone could later add another constant with the same value
        //which could result in the code breaking in unexpected ways
        // (or more probably we would change the original value constant and create 
        //  a conflict with something else)
        namespace my_open_enum
        {
            static const int OUT_OF_MEM = 1;
        }
    }
    #endif


    struct Enum_Info
    {
        const char* value_name;
        const char* type_name;
    };

    struct Open_Enum 
    {
        const ::meta::Enum_Info* info = nullptr;                                        
        constexpr static const char* type_name = ::meta::namespace_name<Open_Enum, 9>();
                        
        constexpr bool operator==(Open_Enum const&) const noexcept = default;               
        constexpr bool operator!=(Open_Enum const&) const noexcept = default;                
    };

    #if 0
    //Instead we do the following: For each value we add we generate some constexpr static data (and save its name becasue we always wanted to do that)
    // and use a pointer to it as identifier
    namespace open_enum_example
    {
        namespace my_enum
        {
            //is just a wrapper around the Enum_Info* ptr - this is so that different enums are not implicitly convertible
            // (still can be easily converted by creating a new Type with info of different enum)
            struct Type
            {
                const Enum_Info* info = nullptr;
                constexpr static const char* type_name = namespace_name<Type, 4>(); //4 is the len of "Type" string

                constexpr bool operator==(Type const&) const noexcept = default;
                constexpr bool operator!=(Type const&) const noexcept = default;
            };
            constexpr static Type OK = {nullptr}; //null state - can also be nothing

            //Save info about added value into its private namespace (so that it doesnt polute too much)
            // and add pointer to it used as the enum value
            namespace _info { 
                constexpr static Enum_Info ERROR = {"ERROR", Type::type_name}; 
            }
            constexpr static Type ERROR = {&_info::ERROR};
        }
    }
    #endif

    //This might seem like alot but really its the minimum required to do the job.
    // The Type struct gets instantiated only once per Open enum declaration. This is the only place where any compile time code execution occurs (namespace_name())
    // The constants get instantiated on each type addition and are just that: constants. There is no compile time code execution.
    //This means this should be as almost fast as declaring global constansts (using constexpr static)

    #define PP_STRINGIFY2(name) #name
    #define PP_STRINGIFY(name) PP_STRINGIFY2(name)

    #define PP_OPEN_ENUM_DECLARE(NULL_STATE, CHILD_OF)                                      \
        struct Type : CHILD_OF                                                              \
        {                                                                                   \
            constexpr static const char* type_name = ::meta::namespace_name<Type, 4>();     \
                                                                                            \
            constexpr bool operator==(Type const&) const noexcept = default;                \
            constexpr bool operator!=(Type const&) const noexcept = default;                \
        };                                                                                  \
        constexpr static Type NULL_STATE = {{nullptr}};                                     \

    #define OPEN_ENUM_DECLARE(NULL_STATE)                   PP_OPEN_ENUM_DECLARE(NULL_STATE, ::meta::Open_Enum);
    #define OPEN_ENUM_DECLARE_DERIVED(NULL_STATE, BASE)     PP_OPEN_ENUM_DECLARE(NULL_STATE, BASE);

    #define OPEN_ENUM_ENTRY(NAME)                                       \
        namespace _info {                                               \
            constexpr static ::meta::Enum_Info NAME =                   \
                {PP_STRINGIFY(NAME), Type::type_name};                  \
        }                                                               \
        constexpr static Type NAME = {{&_info::NAME}};                  \

    //now we use like so (feel free to uncomment)
    #if 0
    namespace open_enum_example
    {

        namespace my_enum_2
        {
            OPEN_ENUM_DECLARE(OK);
            OPEN_ENUM_ENTRY(ERROR);
        }

        namespace my_enum_2
        {
            OPEN_ENUM_ENTRY(OUT_OF_MEM);
        }

        static void instantiation()
        {
            //can be used exactly how normal enums would
            my_enum_2::Type ok = my_enum_2::OK;
            my_enum_2::Type error = my_enum_2::ERROR;

            bool are_not_equal = ok != error;

            my_enum_2::Type third = are_not_equal ? ok : error;

            //their names can be obtained by the folowing:
            const char* my_name = third.info->value_name;
            const char* my_type = third.info->type_name;
        }
    }
    #endif
}