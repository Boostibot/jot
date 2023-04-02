#pragma once

#define PP_STRINGIFY2(name) #name
#define PP_STRINGIFY(name) PP_STRINGIFY2(name)

#define PP_CONCAT2(a, b) a ## b
#define PP_CONCAT(a, b) PP_CONCAT2(a, b)

//A numeric value that behaves like an enum but is:
// 1) Freely extendible
// 2) Guaranteed to be unique
// 3) Type and name strings acessible at runtime 
//    (only using compile time know string literals - see impl)

//Is extremely useful for returning error codes or extending
// interfaces in a transparent way. 

//Example usage:
#if 0

//primary declrarion using the OPEN_ENUM_DECLARE(name) macro
open_enum Allocator_Action
{
    OPEN_ENUM_DECLARE("jot::Allocator_Action");
    OPEN_ENUM_ENTRY(ALLOCATE);
    OPEN_ENUM_ENTRY(DEALLOCATE);
    OPEN_ENUM_ENTRY(RESIZE);
    OPEN_ENUM_ENTRY(RESET);
    OPEN_ENUM_ENTRY(RELEASE_EXTRA_MEMORY);
}

//arbitrary number of extensions
open_enum Allocator_Action
{
    OPEN_ENUM_ENTRY(ADDED_VALUE);
}

//Instantiation using the ::Type field 
Allocator_Action::Type my_enum = Allocator_Action::ADDED_VALUE;

std::cout << my_enum->value_name; //"ADDED_VALUE"
std::cout << my_enum->type_name; //"jot::Allocator_Action"

#endif

//It is also possible to form subset/superset relationships using the
// OPEN_ENUM_DECLARE_DERIVED(Name, Parent) macro. This makes it so that
// values from this open_enum can be freely casted to parent but not vice 
// versa. ie. parent is a superset of values from this open_enum.
// We use this to declare a State_Type type to which all other state types cast.
// All open_enum types freely cast to Open_Enum::Type.
#define open_enum namespace

open_enum Open_Enum
{
    namespace open_enum_info {                  
        constexpr const char* TYPE_NAME = "Open_Enum"; 
    }       

    struct Holder
    {
        const char* value_name;
        const char* type_name;

        constexpr Holder(const char* value_name, const char* type_name) 
            : value_name(value_name), type_name(type_name) 
        {}
    };

    using Type = const Holder*;
}

#define OPEN_ENUM_DECLARE_DERIVED(Name, Parent) \
    namespace open_enum_info {                  \
        constexpr const char* TYPE_NAME = Name; \
    }                                           \
                                                \
    struct Holder : Parent::Holder              \
    {                                           \
        constexpr Holder(const char* value_name, const char* type_name) \
            : Parent::Holder(value_name, type_name) {} \
    };                                          \
    using Type = const Holder*;                 \


#define OPEN_ENUM_ENTRY(Name)                                       \
    namespace open_enum_info                                        \
    {                                                               \
        static constexpr Holder Name(PP_STRINGIFY(Name), TYPE_NAME);\
    }                                                               \
    static constexpr const Holder* Name = &open_enum_info::Name;    \

#define OPEN_ENUM_DECLARE(Name) OPEN_ENUM_DECLARE_DERIVED(Name, ::Open_Enum)