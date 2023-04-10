#pragma once
namespace meta
{
    //returns compiler specific (but very readable!) name at compile time
    template<typename T> constexpr
    const char* type_name() noexcept ;

    //takes a type declared in a namespace and returns that namespace (is useful for macros that tell where they are)
    template<typename Dummy_Struct> constexpr
    const char* namespace_name() noexcept;

    // type_name() examples:
    // MSVC:
    //  int
    //  class std::basic_iostream<char,struct std::char_traits<char> >
    //  struct meta::String
    //
    // GCC:
    //  int
    //  std::basic_iostream<char>
    //  String
    //
    // CLANG:
    //  int
    //  std::basic_iostream<char>
    //  meta::String

    #if defined(__clang__)
        #define __FUNCTION_NAME__ __PRETTY_FUNCTION__
    #elif defined(__GNUC__)
        #define __FUNCTION_NAME__ __PRETTY_FUNCTION__
    #elif defined(_MSC_VER)
        #define __FUNCTION_NAME__ __FUNCSIG__
    #else
        #error "Unsupported compiler"
    #endif

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

    template<typename T> constexpr
    String function_name_string() 
    {
        const char* whole_name_str = __FUNCTION_NAME__;
        int function_size = 0;
        while(whole_name_str[function_size] != '\0')
            function_size ++;

        return String{whole_name_str, 0, function_size};
    }

    template<typename T> constexpr
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


    template<typename Dummy_Struct> constexpr
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

        bool is_global = true;
        int i = type_name.to - 1;
        for(; i-- > type_name.from; )
        {
            
            if(type_name.data[i] == ':' && type_name.data[i + 1] == ':')
            {
                is_global = false;
                break;
            }
        };
        
        if(is_global)
            return String{"", 0, 0};

        String namespace_name = {type_name.data, type_name.from + prefix_size, i};
        return namespace_name;
    }
    
    //we need to create static constexpr variable to return pointer to - 
    // - static constexpr locals are not alloved in standard c++ so we need to create 
    // a helper struct that will hold our string
    namespace internal
    {
        template<typename T>
        struct Type_Name_Holder
        {
            constexpr static String str = type_name_string<T>();
            constexpr static Static_String<str.to - str.from> static_str = Static_String<str.to - str.from>(
                str.data, str.from, str.to
            );
        };

        template<typename Dummy_Struct>
        struct Namespace_Name_Holder
        {
            constexpr static String str = namespace_name_string<Dummy_Struct>();
            constexpr static Static_String<str.to - str.from> static_str = Static_String<str.to - str.from>(
                str.data, str.from, str.to
            );
        };
    }

    template<typename Dummy_Struct> constexpr
    const char* namespace_name() noexcept
    {
        return internal::Namespace_Name_Holder<Dummy_Struct>::static_str.string;
    }

    template<typename T> constexpr
    const char* type_name() noexcept 
    {
        return internal::Type_Name_Holder<T>::static_str.string;
    }
}
