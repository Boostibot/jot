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

    struct String
    {
        const char* data;
        int size;
    };
    
    template<typename T>
    constexpr String type_name_string() 
    {
        #if defined(__clang__)
            //clang: meta::String meta::type_name_string() [T = int]
            char prefix[] = "meta::String meta::type_name_string() [T = ";
            char postfix[] = "]";
        #elif defined(__GNUC__)
            //gcc: constexpr meta::String meta::type_name_string() [with T = int]
            char prefix[] = "constexpr meta::String meta::type_name_string() [with T = ";
            char postfix[] = "]";
        #elif defined(_MSC_VER)
            //msvc: struct meta::String __cdecl meta::type_name_string<int>(void)
            char prefix[] = "struct meta::String __cdecl meta::type_name_string<";
            char postfix[] = ">(void)";
        #endif

        const char* whole_name_str = __FUNCTION_NAME__;
        int function_size = 0;
        while(whole_name_str[function_size] != '\0')
            function_size ++;

        int prefix_size = sizeof(prefix) - 1;
        int postfix_size = sizeof(postfix) - 1;

        int name_size = function_size - prefix_size - postfix_size;
        return String{whole_name_str + prefix_size, name_size};
    }

    /*
    constexpr int strlen(const char* string)
    {
        int size = 0;
        while(string[size] != '\0')
            size ++;

        return size;
    }
    */

    template<int string_size>
    struct Static_String
    {
        char string[string_size + 1] = {0};

        constexpr Static_String(const char* data) noexcept 
        {
            for(int i = 0; i < string_size && data[i]; i++)
                string[i] = data[i];
        };
    };

    template<typename T>
    struct Name
    {
        static constexpr String name_str = type_name_string<T>();
        static constexpr const Static_String<name_str.size> name = name_str.data;

        static constexpr const char* type_name() noexcept {
            return name.string;
        }
    };

    template<typename T>
    constexpr const char* type_name() noexcept 
    {
        return Name<T>::type_name();
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

    // #define MINIMAL_TYPE_ID

    #ifdef MINIMAL_TYPE_ID
        struct Type_Id {
            unsigned char size;
        };
    #else
        struct Type_Id {
            const char* name;
            int size;
        };
    #endif // MINIMAL_TYPE_INFO

    template<typename T>
    struct Type_Id_Instance {
        static const Type_Id info;
    };

    #ifdef MINIMAL_TYPE_ID
        template<typename T>
        const Type_Id Type_Id_Instance<T>::info = Type_Id{sizeof(T) > 255 ? 255 : sizeof(T)};
    #else
        template<typename T>
        const Type_Id Type_Id_Instance<T>::info = Type_Id{type_name<T>(), sizeof(T)};
    #endif // MINIMAL_TYPE_INFO
}

using type_id = const meta::Type_Id*;

template <typename T>
constexpr type_id get_type_id() noexcept  {
    return (type_id) &meta::Type_Id_Instance<T>::info;
}

constexpr const char* type_name(type_id id) noexcept 
{
    #ifdef MINIMAL_TYPE_ID
        return "<minimal type id>";
    #else
        return id->name;
    #endif // MINIMAL_TYPE_INFO
}
using meta::type_name;

#define type_id_of(...) get_type_id<__VA_ARGS__>()
