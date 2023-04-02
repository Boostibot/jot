#pragma once
#include "stack.h"

namespace jot
{
    template<typename T> [[nodiscard]] constexpr 
    isize strlen(const T* str, isize max_size = (isize) 1 << 62) noexcept
    {
        isize size = 0;
        while(size < max_size && str[size] != 0)
            size++;

        return size;
    };

    #define CHAR_T char
    #include "string_type_text.h"
    
    #define CHAR_T wchar_t
    #include "string_type_text.h"
    
    #define CHAR_T char16_t
    #include "string_type_text.h"
    
    #define CHAR_T char32_t
    #include "string_type_text.h"
    
    template <> struct String_Character_Type<char>     { static constexpr bool is_string_char = true;  };
    template <> struct String_Character_Type<wchar_t>  { static constexpr bool is_string_char = true;  };
    template <> struct String_Character_Type<char16_t> { static constexpr bool is_string_char = true;  };
    template <> struct String_Character_Type<char32_t> { static constexpr bool is_string_char = true;  };
    #ifdef __cpp_char8_t
    #define CHAR_T char8_t
    #include "string_type_text.h";

    template <> struct String_Character_Type<char8_t>  { static constexpr bool is_string_char = true;  };
    #endif

    using String = Slice<const char>;
    using Mutable_String = Slice<char>;
    using String_Builder = Stack<char>;
    using String_Appender = Stack_Appender<char>;

    //For windows stuff...
    using wString = Slice<const wchar_t>;
    using wMutable_String = Slice<wchar_t>;
    using wString_Builder = Stack<wchar_t>;
    using wString_Appender = Stack_Appender<wchar_t>;

    //Panic for String_Builder
    struct String_Builder_Panic : Panic
    {
        String_Builder message;

        String_Builder_Panic(Line_Info line_info, String_Builder message) 
            : Panic(line_info, 
                "<this is a String_Builder_Panic and you are looking at the message from Panic! "
                "The catching code should catch Panic const& not just Panic!>"), 
            message(move(&message)) {}

        virtual const char* what() const noexcept 
        {
            return data(message);
        }
        virtual ~String_Builder_Panic() noexcept {}
    };
    
    static
    String_Builder_Panic make_panic(Line_Info line_info, String_Builder message) noexcept
    {
        return String_Builder_Panic(line_info, move(&message));
    }
}
