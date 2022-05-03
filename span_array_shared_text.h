//THIS IS A LITERAL TEXT SUBSTITUION FILE
// DONT INCLUDE IT ON ITS OWN!

using value_type      = T;
using size_type       = Size;
using difference_type = ptrdiff_t;
using pointer         = T*;
using const_pointer   = const T*;
using reference       = T&;
using const_reference = const T&;

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


func& operator[](Size index) const noexcept { assert(index < this->size); return this->data[index]; }
func& operator[](Size index) noexcept       { assert(index < this->size); return this->data[index]; }

//slicing
func operator()(Size from, Size to) const noexcept   {return Slice<T, Size>{this->data + from, to - from};}
func operator()(Begin begin, Size to) const noexcept {return Slice<T, Size>{this->data + cast(Size)(begin), to};}
func operator()(Size from, End end) const noexcept   {return Slice<T, Size>{this->data + from, this->size - from + cast(Size)(end)};}
func operator()(Begin begin, End end) const noexcept {return Slice<T, Size>{this->data + cast(Size)(begin), this->size - cast(Size)(begin) + cast(Size)(end)};}

func operator()(Size from, Size to) noexcept         {return Slice<T, Size>{this->data + from, to - from};}
func operator()(Begin begin, Size to) noexcept       {return Slice<T, Size>{this->data + cast(Size)(begin), to};}
func operator()(Size from, End end) noexcept         {return Slice<T, Size>{this->data + from, this->size - from + cast(Size)(end)};}
func operator()(Begin begin, End end) noexcept       {return Slice<T, Size>{this->data + cast(Size)(begin), this->size - cast(Size)(begin) + cast(Size)(end)};}