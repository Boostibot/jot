struct Type_Id_Value
{
    unsigned int size;
};

template<typename T>
static constexpr Type_Id_Value _type_id_value = {sizeof(T)};

using type_id = const Type_Id_Value*;

template <typename T>
constexpr type_id get_type_id() noexcept  {
    return &_type_id_value<T>;
}

#define type_id_of(...) get_type_id<__VA_ARGS__>()
