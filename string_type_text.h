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
        
    using T = const CHAR_T;
    using value_type      = T;
    using size_type       = size_t;
    using difference_type = ptrdiff_t;
    using reference       = T&;
    using const_reference = const T&;
    using iterator       = T*;
    using const_iterator = const T*;
        
    constexpr iterator       begin() noexcept       { return data; }
    constexpr const_iterator begin() const noexcept { return data; }
    constexpr iterator       end() noexcept         { return data + size; }
    constexpr const_iterator end() const noexcept   { return data + size; }

    constexpr T const& operator[](isize index) const noexcept  
    { 
        assert(0 <= index && index < size && "index out of range"); 
        return data[index]; 
    }
    constexpr T& operator[](isize index) noexcept 
    { 
        assert(0 <= index && index < size && "index out of range"); 
        return data[index]; 
    }
        
    constexpr operator Slice<const T>() const noexcept 
    { 
        return Slice<const T>{this->data, this->size}; 
    }
};
    
constexpr
Slice<const CHAR_T> slice(const CHAR_T* str) noexcept
{
    return Slice<const CHAR_T>(str);
}
    
//necessary for disambiguation
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
