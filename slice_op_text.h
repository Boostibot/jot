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

constexpr [[nodiscard]] 
iterator begin() noexcept 
{
    return this->data;
}

constexpr [[nodiscard]] 
const_iterator begin() const noexcept 
{
    return this->data;
}

constexpr [[nodiscard]] 
iterator end() noexcept 
{
    return this->data + this->size;
}

constexpr [[nodiscard]] 
const_iterator end() const noexcept
{
    return this->data + this->size;
}

constexpr [[nodiscard]]  
T const& operator[](isize index) const noexcept  
{ 
    assert(0 <= index && index < this->size && "index out of range"); 
    return this->data[index]; 
}
constexpr [[nodiscard]]  
T& operator[](isize index) noexcept 
{ 
    assert(0 <= index && index < this->size && "index out of range"); 
    return this->data[index]; 
}
