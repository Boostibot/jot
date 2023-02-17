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
    isize first_index_of(String in_str, String search_for, isize from = 0)
    {
        if(search_for.size == 0)
            return 0;

        if(in_str.size < search_for.size)
            return -1;

        isize to = min(in_str.size - search_for.size + 1, in_str.size);
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

    //adds null termination to the string if it
    // already doesnt have one
    nodisc inline 
    State null_terminate(String_Builder* string)
    {
        if(size(*string) > 0 && last(*string) == '\0')
            return OK_STATE;

        return push(string, '\0');
    }
}

#include "undefs.h"