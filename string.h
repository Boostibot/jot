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


    //adds null termination to the string if it
    // already doesnt have one
    nodisc inline 
    State null_terminate(String_Builder* string) noexcept
    {
        State state = reserve(string, size(*string) + 1);
        if(state == ERROR)
            return state;

        data(string)[size(*string)] = '\0';
        return state;
    }

    nodisc
    cstring to_cstr(String_Builder* string)
    {
        *null_terminate(string);
        return data(*string);
    }
    
    String_Builder to_builder(String string)
    {
        String_Builder builder;
    
        *push_multiple(&builder, string);
        *null_terminate(&builder);

        return builder;
    }
}

#include "undefs.h"