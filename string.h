#pragma once

#include "stack.h"
#include "defines.h"

//@NOTE: We could maybe fuse this file with Slice and Stack
// but its unclear wheter its good idea to "polute" the jot namespace
// with concepts of Strings even though the user never requested such things
namespace jot
{
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
    void append_multiple(Stack<T>* stack, Slice<const Slice<const T>> parts)
    {
        isize base_size = size(stack);
        isize size_sum = 0;
        for(isize i = 0; i < parts.size; i++)
            size_sum += parts[i].size;

        resize_for_overwrite(stack, base_size + size_sum);
        isize curr_size = base_size;
        for(isize i = 0; i < parts.size; i++)
        {
            Slice<T> copy_to = slice(slice(stack), curr_size);
            copy_items(&copy_to, parts[i]);
            curr_size += parts[i].size;
        }
    }
    template <typename T> nodisc inline 
    Stack<T> concat(Slice<const Slice<const T>> parts)
    {
        Stack<T> stack;
        append_multiple(&stack, parts);
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
    
    nodisc inline 
    String_Builder concat(
        cstring a1 = {}, cstring a2 = {}, cstring a3 = {}, cstring a4 = {}, cstring a5 = {}, 
        cstring a6 = {}, cstring a7 = {}, cstring a8 = {}, cstring a9 = {}, cstring a10 = {})
    {
        String strings[] = {a1, a2, a3, a4, a5, a6, a7, a8, a9, a10};
        Slice<const String> parts = slice(strings);
        return concat(parts);
    }

    String_Builder own(cstring string)
    {
        return own(String(string));
    }
}

#include "undefs.h"