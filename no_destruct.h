#pragma once

namespace jot
{
    //A rather nieche but necessary struct that only constructs and never destructs
    // This is necessary if we want to for example: allocate many std::strings in a row 
    // and let the allocator do the cleaning up all at once (arena allocation). 
    //Breaks all invarinats about the type! Use with caution! 
    template <typename T>
    struct No_Destruct
    {
        union {T val;};

        ~No_Destruct() {}
    };
}
