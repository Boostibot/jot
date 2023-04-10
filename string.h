#pragma once
#include "types.h"
#include "stack.h"

#define nodisc [[nodiscard]]

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

    template<> constexpr bool is_string_char<char> = true;
    template<> constexpr bool is_string_char<wchar_t> = true;
    template<> constexpr bool is_string_char<char16_t> = true;
    template<> constexpr bool is_string_char<char32_t> = true;

    #define CHAR_T char
    #include "string_type_text.h"
    
    #define CHAR_T wchar_t
    #include "string_type_text.h"
    
    #define CHAR_T char16_t
    #include "string_type_text.h"
    
    #define CHAR_T char32_t
    #include "string_type_text.h"
    
    #ifdef __cpp_char8_t
    #define CHAR_T char8_t
    #include "string_type_text.h";
    
    template<> static constexpr bool is_string_char<char8_t> = true;
    #endif

    using String = Slice<const char>;
    using Mutable_String = Slice<char>;
    using String_Builder = Stack<char>;

    //For windows stuff...
    using wString = Slice<const wchar_t>;
    using wMutable_String = Slice<wchar_t>;
    using wString_Builder = Stack<wchar_t>;

    //Panic for String_Builder
    struct String_Builder_Panic : Panic
    {
        String_Builder message;

        String_Builder_Panic(Line_Info line_info, String_Builder message) 
            : Panic(line_info), message(move(&message)) {}

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

    nodisc static isize first_index_of(String in_str, String search_for, isize from = 0) noexcept;
    
    nodisc static isize last_index_of(String in_str, String search_for, isize from = (isize) 1 << 62) noexcept;

    nodisc static isize split_into(Slice<String> parts, String string, String separator, isize* optional_next_index = nullptr) noexcept;
    
           static void split_into(Stack<String>* parts, String string, String separator, isize max_parts = (isize) 1 << 62);
    
           static void join_into(String_Builder* stack, Slice<const String> parts, String separator = "");
    
    nodisc static Stack<String> split(String string, String separator, isize max_parts = (isize) 1 << 62, Allocator* alloc = default_allocator());

    nodisc static String_Builder join(Slice<const String> parts, String separator = "", Allocator* alloc = default_allocator());

    nodisc inline String_Builder concat(String a1, String a2)
    {
        String strings[] = {a1, a2};
        Slice<const String> parts = {strings, 2};
        return join(parts);
    }

    nodisc inline String_Builder concat(
        String a1, String a2, String a3, String a4 = "", String a5 = "", 
        String a6 = "", String a7 = "", String a8 = "", String a9 = "", String a10 = "")
    {
        String strings[] = {a1, a2, a3, a4, a5, a6, a7, a8, a9, a10};
        Slice<const String> parts = {strings, 10};
        return join(parts);
    }

    nodisc inline String_Builder own(const char* string, Allocator* alloc = default_allocator())
    {
        return own(String(string), alloc);
    }

    nodisc inline String_Builder own_scratch(const char* string)
    {
        return own(String(string), scratch_allocator());
    }
}

namespace jot
{
    nodisc static  
    isize first_index_of(String in_str, String search_for, isize from) noexcept
    {
        if(search_for.size == 0)
            return 0;

        isize to = in_str.size - search_for.size + 1;
        for(isize i = from; i < to; i++)
        {
            bool found = true;
            for(isize j = 0; j < search_for.size; j++)
            {
                if(in_str[i + j] != search_for[j])
                {
                    found = false;
                    break;
                }
            }

            if(found)
                return i;
        };

        return -1;
    }

    nodisc static  
    isize last_index_of(String in_str, String search_for, isize from) noexcept
    {
        if(search_for.size == 0)
            return 0;

        if(from > search_for.size)
            from = search_for.size;

        isize to = in_str.size - from + 1;
        for(isize i = to; i-- > 0; )
        {
            bool found = true;
            for(isize j = 0; j < search_for.size; j++)
            {
                if(in_str[i + j] != search_for[j])
                {
                    found = false;
                    break;
                }
            }

            if(found)
                return i;
        };

        return -1;
    }
    
    static 
    void join_into(String_Builder* stack, Slice<const String> parts, String separator)
    {
        isize size_sum = 0;
        for(isize i = 0; i < parts.size; i++)
            size_sum += parts[i].size;

        if(parts.size > 0)
            size_sum += separator.size * (parts.size - 1);

        reserve(stack, size(stack) + size_sum);
        if(parts.size > 0)
            push_multiple(stack, parts[0]);

        for(isize i = 1; i < parts.size; i++)
        {
            push_multiple(stack, separator);
            push_multiple(stack, parts[i]);
        }
    }
    
    nodisc static 
    isize split_into(Slice<String> parts, String string, String separator, isize* optional_next_index) noexcept
    {
        isize from = 0;
        for(isize i = 0; i < parts.size; i++)
        {
            isize to = first_index_of(string, separator, from);
            if(to == -1)
            {
                parts[i] = slice_range(string, from, string.size);
                return i;
            }

            parts[i] = slice_range(string, from, to);
            from = to + separator.size;
        }

        if(optional_next_index != nullptr)
            *optional_next_index = first_index_of(string, separator, from);

        return parts.size;
    }

    nodisc static 
    String_Builder join(Slice<const String> parts, String separator, Allocator* alloc)
    {
        String_Builder stack(alloc);
        join_into(&stack, parts, separator);
        return stack;
    }
    
    nodisc static 
    void split_into(Stack<String>* parts, String string, String separator, isize max_parts)
    {
        isize from = 0;
        for(isize i = 0; i < max_parts; i++)
        {
            isize to = first_index_of(string, separator, from);
            if(to == -1)
                break;

            String part = slice_range(string, from, to);
            push(parts, part);
            from = to + separator.size;
        }
        
        String part = slice_range(string, from, string.size);
        if(part.size != 0)
            push(parts, part);
    }
    
    nodisc static 
    Stack<String> split(String string, String separator, isize max_parts, Allocator* alloc)
    {
        Stack<String> parts(alloc);
        split_into(&parts, string, separator, max_parts);
        return parts;
    }
}
