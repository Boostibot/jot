#include "utils.h"

#include "defines.h"

namespace jot
{
    template <class Alloc, typename T = Alloc::value_type>
    [[nodiscard]] proc allocate(Alloc* alloc, size_t size, size_t align) -> T* 
    {
        return alloc->allocate(size);
    }

    template <class Alloc, typename T = Alloc::value_type>
    [[nodiscard]] proc allocate(Alloc* alloc, size_t size) -> T* 
    {
        return allocate(alloc, size, max(alignof(std::max_align_t), alignof(T)));
    }

    template <class Alloc, typename T = Alloc::value_type>
    proc deallocate(Alloc* alloc, T* ptr) -> void
    {
        return alloc->deallocate(size);
    }

    template <class Alloc, typename T = Alloc::value_type>
    proc grow(Alloc* alloc, T* ptr, size_t new_size) -> bool 
    {
        return false;
    }

    template <class Alloc, typename T = Alloc::value_type>
    proc shrink(Alloc* alloc, T* ptr, size_t new_size) -> bool 
    {
        return false;
    }

    template <class Alloc>
    proc deallocate_all(Alloc* alloc) -> void
    {
        cast(void) alloc;
    }
}

#include "undefs.h"