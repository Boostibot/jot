#pragma once

#include "stack.h"
#include "defines.h"

namespace jot
{
    template<typename T> nodisc constexpr 
    isize strlen(const T* str, isize max_size = (isize) 1 << 62)
    {
        isize size = 0;
        while(size < max_size && str[size] != 0)
        {
            size++;
        }
        return size;
    };

    template<>
    struct Slice<const char>
    {
        const char* data = nullptr;
        isize size = 0;

        constexpr Slice() = default;
        constexpr Slice(const char* data, isize size) 
            : data(data), size(size) {}

        constexpr Slice(const char* str)
            : data(str), size(strlen(str)) {}
        
        using T = const char;
        #include "slice_operator_text.h"
    };
    
    Slice(const char*) -> Slice<const char>;
    
    nodisc constexpr
    Slice<const char> slice(const char* str) 
    {
        return Slice<const char>(str);
    }
    
    //necessary for disambiguation
    template<isize N> nodisc constexpr
    Slice<const char> slice(const char (&a)[N])
    {
        return Slice<const char>(a, N - 1);
    }
    
    template <> struct String_Character_Type<char>     { static constexpr bool is_string_char = true;  };
    /*template <> struct String_Character_Type<wchar_t>  { static constexpr bool is_string_char = true;  };
    template <> struct String_Character_Type<char16_t> { static constexpr bool is_string_char = true;  };
    template <> struct String_Character_Type<char32_t> { static constexpr bool is_string_char = true;  };
    #ifdef __cpp_char8_t
    template <> struct String_Character_Type<char8_t>  { static constexpr bool is_string_char = true;  };
    #endif*/

    using String = Slice<const char>;
    using Mutable_String = Slice<char>;
    using String_Builder = Stack<char>;
    using String_Appender = Stack_Appender<char>;

    nodisc inline  
    isize first_index_of(String in_str, String search_for, isize from = 0) noexcept
    {
        if(search_for.size == 0)
            return 0;

        isize to = in_str.size - search_for.size + 1;
        for(isize i = from; i < to; i++)
        {
            const auto execute = [&](){
                for(isize j = 0; j < search_for.size; j++)
                {
                    if(in_str[i + j] != search_for[j])
                        return false;
                }

                return true;
            };

            if(execute())
                return i;
        };

        return -1;
    }

    nodisc inline  
    isize last_index_of(String in_str, String search_for, isize from = 0) noexcept
    {
        if(search_for.size == 0)
            return 0;

        isize to = in_str.size - search_for.size + 1;
        for(isize i = to; i-- > from; )
        {
            const auto execute = [&](){
                for(isize j = 0; j < search_for.size; j++)
                {
                    if(in_str[i + j] != search_for[j])
                        return false;
                }

                return true;
            };

            if(execute())
                return i;
        };

        return -1;
    }
    
    template <typename T> inline 
    void append_multiple(Stack<T>* stack, Slice<const Slice<const T>> parts, Slice<const T> separator = {})
    {
        isize base_size = size(stack);
        isize size_sum = 0;
        for(isize i = 0; i < parts.size; i++)
            size_sum += parts[i].size;

        if(parts.size > 0)
            size_sum += separator.size * (parts.size - 1);

        resize_for_overwrite(stack, base_size + size_sum);
        isize curr_size = base_size;
        if(separator.size == 0)
        {
            for(isize i = 0; i < parts.size; i++)
            {
                Slice<T> copy_to = tail(slice(stack), curr_size);
                copy_items(copy_to, parts[i]);
                curr_size += parts[i].size;
            }
        }
        else
        {
            isize i = 0;
            if(parts.size > 0)
            {
                Slice<T> part_to = tail(slice(stack), curr_size);
                copy_items(part_to, parts[i]);
                curr_size += parts[i].size;
            }

            for(i = 1; i < parts.size; i++)
            {
                Slice<T> part_to = tail(slice(stack), curr_size);
                copy_items(part_to, parts[i]);
                curr_size += parts[i].size;
                
                Slice<T> sep_to = tail(slice(stack), curr_size);
                copy_items(part_to, separator);
                curr_size += separator.size;
            }
        }
    }
    
    template <typename T> nodisc inline 
    Stack<T> concat(Slice<const Slice<const T>> parts, Slice<const T> separator = {}, memory_globals::Default_Alloc alloc = {})
    {
        Stack<T> stack(alloc.val);
        append_multiple(&stack, parts, separator);
        return stack;
    }
    
    template <typename T> nodisc inline 
    Stack<T> concat(Slice<const T> a1, Slice<const T> a2)
    {
        Slice<const T> strings[] = {a1, a2};
        Slice<const Slice<const T>> parts = slice(strings);
        return concat(parts);
    }
    
    template <typename T> nodisc inline 
    Stack<T> concat(Slice<const T> a1, Slice<const T> a2, Slice<const T> a3)
    {
        Slice<const T> strings[] = {a1, a2, a3};
        Slice<const Slice<const T>> parts = slice(strings);
        return concat(parts);
    }

    template <typename T> nodisc inline 
    Stack<T> concat(
        Slice<const T> a1, Slice<const T> a2, Slice<const T> a3, Slice<const T> a4, Slice<const T> a5 = {}, 
        Slice<const T> a6 = {}, Slice<const T> a7 = {}, Slice<const T> a8 = {}, Slice<const T> a9 = {}, Slice<const T> a10 = {})
    {
        Slice<const T> strings[] = {a1, a2, a3, a4, a5, a6, a7, a8, a9, a10};
        Slice<const Slice<const T>> parts = slice(strings);
        return concat(parts);
    }

    String_Builder own(cstring string)
    {
        return own(String(string));
    }
    
    String_Builder own_tmp(cstring string)
    {
        return own(String(string), scratch_allocator());
    }
}

#include "undefs.h"