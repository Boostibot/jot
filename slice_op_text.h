//THIS IS A LITERAL TEXT SUBSTITUION FILE
// DONT INCLUDE IT ON ITS OWN!

using value_type      = T;
using size_type       = isize;
using difference_type = ptrdiff_t;
using pointer         = T*;
using const_pointer   = const T*;
using reference       = T&;
using const_reference = const T&;
using iterator_category = std::contiguous_iterator_tag;

using iterator       = T*;
using const_iterator = const T*;

using reverse_iterator       = std::reverse_iterator<iterator>;
using const_reverse_iterator = std::reverse_iterator<const_iterator>;

[[nodiscard]] constexpr 
iterator begin() noexcept 
{
    return this->DATA;
}

[[nodiscard]] constexpr 
const_iterator begin() const noexcept 
{
    return this->DATA;
}

[[nodiscard]] constexpr 
iterator end() noexcept 
{
    return this->DATA + this->SIZE;
}

[[nodiscard]] constexpr 
const_iterator end() const noexcept
{
    return this->DATA + this->SIZE;
}

//for isize
[[nodiscard]] constexpr  
T const& operator[](isize index) const noexcept  
{ 
    assert(0 <= index && index < this->SIZE && "index out of range"); 
    return this->DATA[index]; 
}
[[nodiscard]] constexpr  
T& operator[](isize index) noexcept 
{ 
    assert(0 <= index && index < this->SIZE && "index out of range"); 
    return this->DATA[index]; 
}
