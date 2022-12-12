//THIS IS A LITERAL TEXT SUBSTITUION FILE
// DONT INCLUDE IT ON ITS OWN!

using value_type      = T;
using size_type       = Size;
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

func begin() noexcept -> iterator {
    return this->data;
}

func begin() const noexcept -> const_iterator {
    return this->data;
}

func end() noexcept -> iterator{
    return this->data + this->size;
}

func end() const noexcept -> const_iterator {
    return this->data + this->size;
}

func operator[](Size index) const noexcept -> T const& { 
    assert(0 <= index && index < this->size && "index out of range"); 
    return this->data[index]; 
}
func operator[](Size index) noexcept -> T& { 
    assert(0 <= index && index < this->size && "index out of range"); 
    return this->data[index]; 
}