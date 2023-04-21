//THIS IS A LITERAL TEXT SUBSTITUION FILE
// DONT INCLUDE IT ON ITS OWN!

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
    
    constexpr CHAR_T const& operator[](isize index) const noexcept  
    { 
        assert(0 <= index && index < size && "index out of range"); return data[index]; 
    }
};
    
constexpr
Slice<const CHAR_T> slice(const CHAR_T* str) noexcept
{
    return Slice<const CHAR_T>(str);
}
    
template<isize N> constexpr
Slice<const CHAR_T> slice(const CHAR_T (&a)[N]) noexcept
{
    return Slice<const CHAR_T>(a, N - 1);
}
    
bool operator ==(Slice<const CHAR_T> const& a, Slice<const CHAR_T> const& b) noexcept 
{
    return are_bytes_equal(a, b);
}
    
bool operator !=(Slice<const CHAR_T> const& a, Slice<const CHAR_T> const& b) noexcept 
{
    return are_bytes_equal(a, b) == false;
}

#undef CHAR_T
