//THIS IS A LITERAL TEXT SUBSTITUION FILE
// DONT INCLUDE IT ON ITS OWN!

#define _nodisc [[nodiscard]]

template<>
struct Slice<const CHAR_T>
{
    const CHAR_T* data = nullptr;
    isize size = 0;

    constexpr Slice() noexcept = default;
    constexpr Slice(const CHAR_T* data, isize size) noexcept
        : data(data), size(size) {}

    constexpr Slice(const CHAR_T* str) noexcept
        : data(str), size(strlen(str)) {}
        
    using T = const CHAR_T;
    #include "slice_members_text.h"
};
    
Slice(const CHAR_T*) -> Slice<const CHAR_T>;

_nodisc constexpr
Slice<const CHAR_T> slice(const CHAR_T* str) noexcept
{
    return Slice<const CHAR_T>(str);
}
    
//necessary for disambiguation
template<isize N> _nodisc constexpr
Slice<const CHAR_T> slice(const CHAR_T (&a)[N]) noexcept
{
    return Slice<const CHAR_T>(a, N - 1);
}
    
_nodisc constexpr 
bool operator ==(Slice<const CHAR_T> const& a, Slice<const CHAR_T> const& b) noexcept 
{
    return compare(a, b) == 0;
}
    
_nodisc constexpr 
bool operator !=(Slice<const CHAR_T> const& a, Slice<const CHAR_T> const& b) noexcept 
{
    return compare(a, b) != 0;
}

#undef CHAR_T
#undef _nodisc