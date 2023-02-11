struct Type_Id_Value
{
    unsigned int size;
};

template<typename T>
static constexpr Type_Id_Value _type_id_value = {sizeof(T)};

using type_id = const Type_Id_Value*;

//Adds a unique 'numeric' value to each type at compile time
// this doesnt need RTTI and will only get specialized for the types it
// is used with!
template <typename T> [[nodiscard]] constexpr
type_id get_type_id() {
    return &_type_id_value<T>;
}

#define type_id_of(...) get_type_id<__VA_ARGS__>()
