//THIS IS A LITERAL TEXT SUBSTITUION FILE
// DONT INCLUDE IT ON ITS OWN!

using value_type      = T;
using size_type       = Size;
using difference_type = ptrdiff_t;
using pointer         = T*;
using const_pointer   = const T*;
using reference       = T&;
using const_reference = const T&;

//@NOTE: dont know why but without std::allocator or these containers doesnt satisfy range concept
// (begin and end are defined outside the class and work for ranged based for loops
//  but not for range concept?)
using iterator       = T*;
using const_iterator = const T*;

using reverse_iterator       = std:: reverse_iterator<iterator>;
using const_reverse_iterator = std:: reverse_iterator<const_iterator>;

constexpr iterator begin() noexcept {
    return this->data;
}

constexpr const_iterator begin() const noexcept {
    return this->data;
}

constexpr iterator end() noexcept {
    return this->data + this->size;
}

constexpr const_iterator end() const noexcept {
    return this->data + this->size;
}

//per element access
proc operator()() const 
    requires requires() { custom_invoke(*this, PerElementDummy()); } 
{ 
    return custom_invoke(*this, PerElementDummy()); 
}

proc operator()() 
    requires requires() { custom_invoke(*this, PerElementDummy()); } 
{ 
    return custom_invoke(*this, PerElementDummy());
}

template<class T>
proc operator()(T&& ts) const 
    requires requires() { custom_invoke(*this, std::forward<T>(ts)); } 
{
    return custom_invoke(*this, std::forward<T>(ts));
}
template<class T>
proc operator()(T&& ts) 
    requires requires() { custom_invoke(*this, std::forward<T>(ts)); } 
{
    return custom_invoke(*this, std::forward<T>(ts));
}

func check_slice_bounds(Size from, Size to) const noexcept 
{
    assert(from <= to && "slice must be a valid range");
    assert(0 <= from && from <= this->size && "from must be inside valid data");
    assert(0 <= to && to <= this->size && "to must be inside valid data");
}


func& operator[](Size index) const noexcept { assert(0 <= index && index < this->size && "index out of range"); return this->data[index]; }
func& operator[](Size index) noexcept       { assert(0 <= index && index < this->size && "index out of range"); return this->data[index]; }

//slicing
func operator()(Size from, Size to) const noexcept   
{ 
    check_slice_bounds(from, to); 
    return const_slice_type{this->data + from, to - from};
}
func operator()(Begin begin, Size to) const noexcept 
{ 
    check_slice_bounds(cast(Size)(begin), to); 
    return const_slice_type{this->data + cast(Size)(begin), to};
}
func operator()(Size from, End end) const noexcept   
{ 
    check_slice_bounds(from, from + cast(Size)(end)); 
    return const_slice_type{this->data + from, this->size - from + cast(Size)(end)};
}
func operator()(Begin begin, End end) const noexcept 
{ 
    check_slice_bounds(cast(Size)(begin), cast(Size)(begin) + cast(Size)(end)); 
    return const_slice_type{this->data + cast(Size)(begin), this->size - cast(Size)(begin) + cast(Size)(end)};
}

func operator()(Size from, Size to) noexcept   
{ 
    check_slice_bounds(from, to); 
    return slice_type{this->data + from, to - from};
}
func operator()(Begin begin, Size to) noexcept 
{ 
    check_slice_bounds(cast(Size)(begin), to); 
    return slice_type{this->data + cast(Size)(begin), to};
}
func operator()(Size from, End end) noexcept   
{ 
    check_slice_bounds(from, from + cast(Size)(end)); 
    return slice_type{this->data + from, this->size - from + cast(Size)(end)};
}
func operator()(Begin begin, End end) noexcept 
{ 
    check_slice_bounds(cast(Size)(begin), cast(Size)(begin) + cast(Size)(end)); 
    return slice_type{this->data + cast(Size)(begin), this->size - cast(Size)(begin) + cast(Size)(end)};
}

//func operator()(Size from, Size to) noexcept         { return slice_type{this->data + from, to - from};}
//func operator()(Begin begin, Size to) noexcept       { return slice_type{this->data + cast(Size)(begin), to};}
//func operator()(Size from, End end) noexcept         { return slice_type{this->data + from, this->size - from + cast(Size)(end)};}
//func operator()(Begin begin, End end) noexcept       { return slice_type{this->data + cast(Size)(begin), this->size - cast(Size)(begin) + cast(Size)(end)};}