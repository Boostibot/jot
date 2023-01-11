#pragma once

#define PP_STRINGIFY2(name) #name
#define PP_STRINGIFY(name) PP_STRINGIFY2(name)

#define PP_CONCAT2(a, b) a ## b
#define PP_CONCAT(a, b) PP_CONCAT2(a, b)

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
};

using Type = const Holder*;
}

#define OPEN_ENUM_DECLARE_DERIVED(Name, Parent) \
    namespace open_enum_info {                  \
        constexpr const char* TYPE_NAME = Name; \
    }                                           \
                                                \
    struct Holder : Parent {};                  \
    using Type = const Holder*;                 \

#define OPEN_ENUM_DECLARE(Name) OPEN_ENUM_DECLARE_DERIVED(Name, ::Open_Enum::Holder)

#define OPEN_ENUM_ENTRY(Name)                                       \
    namespace open_enum_info {                                      \
        static constexpr Holder Name = Holder(                      \
            ::Open_Enum::Holder{PP_STRINGIFY(Name), TYPE_NAME});    \
    }                                                               \
    static constexpr Type Name = &open_enum_info::Name;                              \


//For pure C
#ifdef OPEN_ENUM_C

#define PP_STRINGIFY2(name) #name
#define PP_STRINGIFY(name) PP_STRINGIFY2(name)

#define PP_CONCAT2(a, b) a ## b
#define PP_CONCAT(a, b) PP_CONCAT2(a, b)

#define C_OPEN_ENUM_TYPE(Name)     \
        struct PP_CONCAT(_holder_, Name) {    \
            const char* value_name;           \
            const char* type_name;            \
        };                                    \
        typedef const struct PP_CONCAT(_holder_, Name)* Name; 

#define C_OPEN_ENUM_ENTRY(Type, NAME)                                          \
        static const struct PP_CONCAT(_holder_, Type) PP_CONCAT(_value_, NAME) =   \
            {PP_STRINGIFY(NAME), PP_STRINGIFY(Type)};                              \
        const Type NAME = &PP_CONCAT(_value_, NAME)                                \

//use like so:
C_OPEN_ENUM_TYPE(My_Enum);
C_OPEN_ENUM_ENTRY(My_Enum, FIRST);

My_Enum val = FIRST;
const char* name = val->name;

#endif