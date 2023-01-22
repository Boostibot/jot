#pragma once
namespace meta
{
    #if defined(__clang__)
        #define __FUNCTION_NAME__ __PRETTY_FUNCTION__
    #elif defined(__GNUC__)
        #define __FUNCTION_NAME__ __PRETTY_FUNCTION__
    #elif defined(_MSC_VER)
        #define __FUNCTION_NAME__ __FUNCSIG__
    #else
        #error "Unsupported compiler"
    #endif

    #define nodisc [[nodiscard]]

    struct String
    {
        const char* data;
        int from;
        int to;
    };

    template<int string_size>
    struct Static_String
    {
        char string[string_size + 1] = {0};

        constexpr Static_String(const char* data, int from = 0, int to = string_size) noexcept 
        {
            for(int i = from; i < to && data[i]; i++)
                string[i - from] = data[i];
        }
    };

    template<typename T> nodisc constexpr
    String function_name_string() 
    {
        const char* whole_name_str = __FUNCTION_NAME__;
        int function_size = 0;
        while(whole_name_str[function_size] != '\0')
            function_size ++;

        return String{whole_name_str, 0, function_size};
    }

    template<typename T> nodisc constexpr
    String type_name_string() 
    {
        String function_name = function_name_string<T>();
        
        #if defined(__clang__)
            //clang: meta::String meta::function_name_string() [T = int]
            char prefix[] = "meta::String meta::function_name_string() [T = ";
            char postfix[] = "]";
        #elif defined(__GNUC__)
            //gcc: constexpr meta::String meta::function_name_string() [with T = int]
            char prefix[] = "constexpr meta::String meta::function_name_string() [with T = ";
            char postfix[] = "]";
        #elif defined(_MSC_VER)
            //msvc: struct meta::String __cdecl meta::function_name_string<int>(void)
            char prefix[] = "struct meta::String __cdecl meta::function_name_string<";
            char postfix[] = ">(void)";
        #endif

        int prefix_size = sizeof(prefix) - 1;
        int postfix_size = sizeof(postfix) - 1;

        return String{function_name.data, function_name.from + prefix_size, function_name.to - postfix_size};
    }

    //type_name examples:
    //MSVC:
    //int
    //class std::basic_iostream<char,struct std::char_traits<char> >
    //struct meta::String

    //GCC:
    //int
    //std::basic_iostream<char>
    //String

    //CLANG
    //int
    //std::basic_iostream<char>
    //meta::String

    //we need to create static constexpr variable to return pointer to - 
    // - static constexpr locals are not alloved in standard c++ so we need to create 
    // a helper struct that will hold our string

    template<typename Dummy_Struct, int dummy_struct_name_size> nodisc constexpr
    String namespace_name_string() noexcept
    {
        String type_name = type_name_string<Dummy_Struct>();

        char additional_postfix[] = "::";

        #if defined(__clang__)  
            char prefix[] = "";
        #elif defined(__GNUC__) 
            char prefix[] = "";
        #elif defined(_MSC_VER) 
            char prefix[] = "struct ";
        #endif

        int additional_postfix_size = sizeof(additional_postfix) - 1;
        int prefix_size = sizeof(prefix) - 1;
        int postfix_size = dummy_struct_name_size + additional_postfix_size;

        String namespace_name = {type_name.data, type_name.from + prefix_size, type_name.to - postfix_size};
        return namespace_name;
    }

    template<String str>
    struct Static_Holder
    {
        static constexpr const Static_String<str.to - str.from> static_str = {str.data, str.from, str.to};
    };

    template<String str> nodisc constexpr
    const char* to_const_char() noexcept 
    {
        using Holder = Static_Holder<str>;
        return Holder::static_str.string;
    }

    template<typename T> nodisc constexpr
    const char* type_name() noexcept 
    {
        constexpr String name_str = type_name_string<T>();
        return to_const_char<name_str>();
    }

    template<typename Dummy_Struct, int dummy_struct_name_size = 20> nodisc constexpr
    const char* namespace_name() noexcept
    {
        constexpr String name_str = namespace_name_string<Dummy_Struct, dummy_struct_name_size>();
        return to_const_char<name_str>();
    }
}

#undef nodisc