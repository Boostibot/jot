#pragma once

#include "utils.h"
#include "types.h"
#include "slice.h"
#include "allocator.h"
#include "defines.h"

namespace jot
{
    template<tsize mult_, tsize add_, tsize base_elems_>
    struct Def_Grow
    {
        static constexpr tsize mult = mult_;
        static constexpr tsize add = add_;
        static constexpr tsize base_elems = base_elems_;

        static constexpr tsize run(tsize to_fit, tsize capacity, tsize size, tsize static_capacity, tsize elem_size)
        {
            cast(void) size;
            cast(void) static_capacity;
            cast(void) elem_size;

            tsize realloc_to = capacity 
                ? capacity * mult + add 
                : base_elems;

            while(realloc_to < to_fit)
                realloc_to = realloc_to * mult + add;

            return realloc_to;
        }
    };

    namespace detail
    {   
        template<non_void T, tsize static_capacity>
        struct Stack_Data 
        {
            //has to be bytes or else will default construct all elements 
            // (even if we provide explicit default constructor and leave the array uninit)
            //@NOTE: This also prevents Stack_Data with non zero static_capacity_ from being used by constexpr functions
            //@NOTE: For now zero initializing the static_data_ as well; Not sure if it makes any sense to do so
            byte static_data[static_capacity * sizeof(T)] = {0};
        };

        template<non_void T>
        struct Stack_Data<T, 0> {};
    }

    template <
        typename T, 
        tsize static_capacity_ = 0, 
        typename Size_ = tsize, 
        typename Alloc_ = Poly_Allocator, 
        typename Grow = Def_Grow<2, 0, 8>
    >
    struct Stack : Alloc_, detail::Stack_Data<T, static_capacity_>
    {   
        using Size = Size_;
        using Alloc = Alloc_;

        using allocator_type   = Alloc;
        using grow_type        = Grow;
        using Stack_Data       = detail::Stack_Data<T, static_capacity_>;

        constexpr static Size static_capacity = cast(Size) static_capacity_; 
        constexpr static bool has_static_storage = static_capacity != 0;
        constexpr static bool has_stateful_alloc = !std::is_empty_v<Alloc>;

        T* data = nullptr;
        Size size = 0;
        Size capacity = 0;

        constexpr Stack() noexcept = default;
        constexpr Stack(Alloc alloc) noexcept
            : Alloc{alloc} {}

        constexpr Stack(Stack moved other, Alloc alloc = Alloc()) noexcept;

        constexpr Stack(T* data, Size size, Size capacity, Alloc alloc = Alloc()) noexcept
            : Stack_Data{}, Alloc{alloc}, data(data), size(size), capacity(capacity) {}

        constexpr ~Stack() noexcept;
        constexpr Stack& operator=(Stack moved other) noexcept;

        #include "slice_op_text.h"
    };

    #define STACK_TEMPL class T, ::jot::tsize static_cap, class Size, class Alloc, class Grow
    #define Stack_T Stack<T, static_cap, Size, Alloc, Grow>

    template<STACK_TEMPL>
    func slice(Stack_T in stack) -> Slice<const T>{
        return Slice<const T>{stack.data, stack.size};
    }

    template<STACK_TEMPL>
    func slice(Stack_T* stack) -> Slice<T>{
        return Slice<T>{stack->data, stack->size};
    }

    template<STACK_TEMPL>
    func capacity_slice(Stack_T in stack) -> Slice<const T>{
        return Slice<const T>{stack.data, stack.capacity};
    }

    template<STACK_TEMPL>
    func capacity_slice(Stack_T* stack) -> Slice<T>{
        return Slice<T>{stack->data, stack->capacity};
    }

    template <STACK_TEMPL>
    func alloc(Stack_T in stack) -> Alloc in { 
        return *cast(const Alloc*) &stack; 
    }

    template <STACK_TEMPL>
    func alloc(Stack_T* stack) -> Alloc* { 
        return cast(Alloc*) stack; 
    }

    template <STACK_TEMPL>
    func static_data(Stack_T* stack) -> T*
    {
        if constexpr(static_cap != 0)
            return cast(T*) cast(void*) stack->static_data;
        else
            return nullptr;
    }

    template <STACK_TEMPL>
    func static_data(Stack_T in stack) -> const T*
{
        if constexpr(static_cap != 0)
            return cast(T*) cast(void*) stack.static_data;
        else
            return nullptr;
    }

    template <STACK_TEMPL>
    func static_slice(Stack_T in stack) -> Slice<const T> {
        return Slice<const T>{static_data(stack), static_cap};
    }

    template <STACK_TEMPL>
    func static_slice(Stack_T* stack) -> Slice<T> {
        return Slice<T>{static_data(stack), static_cap};
    }

    template <STACK_TEMPL>
    func is_invariant(Stack_T in stack) -> bool
    {
        const bool size_inv = stack.capacity >= stack.size;
        const bool data_inv = (stack.capacity == 0) == (stack.data == nullptr);
        const bool capa_inv = stack.capacity != 0 
            ? stack.capacity >= stack.static_capacity
            : true;

        return size_inv && capa_inv && data_inv;
    }

    template <STACK_TEMPL>
    func is_static_alloced(Stack_T in stack) noexcept -> bool
    {
        assert(is_invariant(stack));
        if constexpr(stack.has_static_storage == false)
            return false;
        else
            return stack.capacity == stack.static_cap; 
    }

    template <STACK_TEMPL>
    func calculate_growth(Stack_T in stack, no_infer(Size) to_fit) -> Size
    {
        return cast(Size) Stack_T::grow_type::run(
            cast(tsize) to_fit, 
            cast(tsize) stack.capacity, 
            cast(tsize) stack.size, 
            cast(tsize) stack.static_capacity,
            sizeof(T)
        );
    }

    template<STACK_TEMPL>
    proc swap(Stack_T* left, Stack_T* right) noexcept -> void 
    {
        using Stack = Stack_T;
        constexpr let transfer_static = [](Stack* to, Stack* from, Size to_i, Size from_i)
        {
            T* no_alias to_data = static_data(to);
            T* no_alias from_data = static_data(from);

            for (Size i = from_i; i < to_i; i++)
            {
                std::construct_at(to_data + i, move(from_data + i));
                from_data[i].~T();
            }
        };

        constexpr let transfer_half_static = [=](Stack* to, Stack* from)
        {
            transfer_static(to, from, from->size, 0);

            from->data = to->data;
            to->data = static_data(to);
        };

        if (is_static_alloced(*left) && is_static_alloced(*right))
        {
            Size to_size = 0;
            if (left->size < right->size)
            {
                to_size = left->size;
                transfer_static(left, right, right->size, left->size);
            }
            else
            {
                to_size = right->size;
                transfer_static(right, left, left->size, right->size);
            }

            T* no_alias left_data = left->data;
            T* no_alias right_data = right->data;

            for (Size i = 0; i < to_size; i++)
                swap(left_data + i, right_data + i);
        }
        else if (is_static_alloced(*left))
            transfer_half_static(right, left);
        else if (is_static_alloced(*right))
            transfer_half_static(left, right);
        else
            swap(&left->data, &right->data);

        swap(&left->size, &right->size);
        swap(&left->capacity, &right->capacity);

        if constexpr(left->has_stateful_alloc)
            swap(alloc(left), alloc(right));
    }

    namespace detail 
    {
        template<STACK_TEMPL>
        proc destroy_items(Stack_T* vec)
        {
            if constexpr(std::is_trivially_destructible_v<T> == false)
                for(Size i = 0; i < vec->size; i++)
                    vec->data[i].~T();
        }

        template<STACK_TEMPL>
        proc alloc_data(Stack_T* vec, Size capacity) -> Alloc_State 
        {
            if(capacity <= vec->static_capacity)
            {
                //We could also have Stack_Data with static capacity > 0 default init to these values
                // - that however would make it so that default init is not zero init which could be a slow down
                //The reallocating path is expensive anyways and this check will fire in so little cases 
                // (actually only once) that it doesnt matter
                if(vec->capacity < vec->static_capacity)
                {
                    vec->capacity = vec->static_capacity;
                    vec->data = static_data(vec);
                }

                return;
            }

            mut new_capacity = calculate_growth(*vec, capacity);
            mut new_info = make_alloc_info<T>(new_capacity);
            mut allocator = Comptime_Allocator<T, Alloc>(alloc(vec));
            let res = allocator.allocate(new_info);
            if(res.state != Alloc_State::OK)
                return res.state;

            vec->data = res.slice.data;
            vec->capacity = new_capacity;
        }

        template<STACK_TEMPL>
        proc dealloc_data(Stack_T* vec) -> bool 
        {
            if(vec->capacity > vec->static_capacity)
            {
                mut old_info = make_alloc_info<T>(vec->capacity);
                mut allocator = Comptime_Allocator<T, Alloc>(alloc(vec));
                return allocator.deallocate(capacity_slice(vec), old_info);
            }

            return true;
        }

        //can be used for arbitrary growing/shrinking of data
        // when called on uninit stack acts as alloc_data
        // when called with new_capacity = 0 acts as dealloc_data
        // Destroys elements when shrinking but does not construct new ones when growing 
        //  (because doesnt know how to)
        template<STACK_TEMPL>
        proc set_capacity(Stack_T* vec, Size new_capacity) -> Alloc_State 
        {
            T* no_alias new_data = nullptr;
            T* no_alias old_data = vec->data;

            if(new_capacity <= vec->static_capacity)
            {
                new_data = static_data(vec);
                new_capacity = vec->static_capacity;
            }
            else
            {
                let my_slice = Slice{vec->data, vec->capacity};
                let new_info = make_alloc_info<T>(new_capacity);
                let prev_info = make_alloc_info<T>(vec->capacity);

                mut allocator = Comptime_Allocator<T, Alloc>(alloc(vec));

                let resize_res = allocator.action(
                    Alloc_Actions::RESIZE, 
                    nullptr, 
                    my_slice, 
                    new_info, prev_info, 
                    nullptr);
                if(resize_res.state == Alloc_State::OK)
                {
                    let cast_res = cast_slice<T>(resize_res.slice);
                    assert(cast_res.size == new_capacity);
                    assert(cast_res.data == nullptr ? new_capacity == 0 : true);

                    vec->data = cast_res.data;
                    vec->capacity = cast_res.size;
                    return Alloc_State::OK;
                }
                else
                {
                    let res = allocator.allocate(new_info);
                    if(res.state != Alloc_State::OK)
                        return res.state;

                    let cast_res = cast_slice<T>(res.slice);
                    new_data = cast_res.data;
                }
            }

            assert(are_aliasing<T>(slice(vec), Slice<T>{new_data, cast(tsize) new_capacity}) == false);

            {
                Size copy_to = std::min(vec->size, new_capacity);
                for (Size i = 0; i < copy_to; i++)
                    std::construct_at(new_data + i, move(old_data + i));
            }

            destroy_items(vec);
            cast(void) dealloc_data(vec);

            vec->data = new_data;
            vec->capacity = new_capacity;

            return Alloc_State::OK;
        }

        template<STACK_TEMPL>
        proc by_byte_assign(Stack_T* to, Stack_T in from) -> void
        {
            to->data = from.data;
            to->size = from.size;
            to->capacity = from.capacity;
            *alloc(to) = alloc(from);
        }
    }

    template<STACK_TEMPL>
    struct Assign<Stack_T>
    {
        using Stack = Stack_T;

        struct Error_Type
        {
            address_alias Assign_Error<T> item_assign_eror;
            Size assigned_to = 0;
            Alloc_State alloc_error = Alloc_State::OK;
        };

        static proc perform(Stack* to, Stack in from, Error_Type* error) noexcept -> bool
        {
            if(to == &from)
                return true;

            if(to->capacity < from.size)
            {
                let res = detail::set_capacity(to, from.size);
                if(res == Error())
                {
                    error->alloc_error = res;
                    return false;
                }
            }

            Size to_size = std::min(to->size, from.size);

            T* no_alias to_data = to->data;
            const T* no_alias from_data = from.data;

            //destroy extra
            if constexpr(std::is_trivially_destructible_v<T> == false)
                for (Size i = from.size; i < to->size; i++)
                    to->data[i].~T();

            if(has_bit_by_bit_assign<T> && is_const_eval() == false)
            {
                //construct missing
                if(to->size < from.size)
                    memcpy(to_data + to->size, from_data + to->size, (from.size - to->size) * sizeof(T));

                //copy rest
                //if(to_size != 0)
                memcpy(to_data, from_data, to_size * sizeof(T));
            }
            else
            {
                //construct missing
                for (Size i = to->size; i < from.size; i++)
                {
                    mut state = construct_assign_at(to_data + i, from_data[i]);
                    if(state == Error())
                    {
                        error->item_assign_eror = jot::error(state);
                        error->assigned_to = i;
                        return false;
                    }
                }

                //copy rest
                for (Size i = 0; i < to_size; i++)
                {
                    mut state = assign(to_data + i, from_data[i]);
                    if(state == Error())
                    {
                        error->item_assign_eror = jot::error(state);
                        error->assigned_to = i;
                        return false;
                    }
                }
            }
            
            to->size = from.size;
            return true;
        }
    };
    

    template<STACK_TEMPL>
    constexpr Stack_T::~Stack() noexcept 
    {
        if(data != nullptr)
        {
            detail::destroy_items(this);
            cast(void) detail::dealloc_data(this);
        }
    }

    template<STACK_TEMPL>
    constexpr Stack_T::Stack(Stack moved other, Alloc alloc) noexcept 
        : Alloc{alloc}
    {
        swap(this, &other);
    }

    template<STACK_TEMPL>
    constexpr Stack_T& Stack_T::operator=(Stack_T moved other) noexcept 
    {
        swap(this, *other);
        return *this;
    }

    template <STACK_TEMPL>
    proc reserve(Stack_T* stack, no_infer(Size) to_fit) -> Alloc_State
    {
        assert(is_invariant(*stack));

        if (stack->capacity >= to_fit)
            return Alloc_State::OK;

        Size realloc_to = 0;
        if (to_fit <= stack->static_capacity)
            realloc_to = stack->static_capacity;
        else
            realloc_to = calculate_growth(*stack, to_fit);

        assert(realloc_to >= to_fit);
        let res = detail::set_capacity(stack, realloc_to);

        assert(is_invariant(*stack));
        return res;
    }

    template <STACK_TEMPL>
    proc clear(Stack_T* stack) -> void
    {
        assert(is_invariant(*stack));
        detail::destroy_items(stack);
        stack->size = 0;
        assert(is_invariant(*stack));
    }

    template <STACK_TEMPL>
    func empty(Stack_T in stack) noexcept -> bool
    {
        assert(is_invariant(stack));
        return stack.size == 0;
    }

    template <STACK_TEMPL>
    func is_empty(Stack_T in stack) noexcept -> bool
    {
        assert(is_invariant(stack));
        return stack.size == 0;
    }

    template <STACK_TEMPL, stdr::forward_range Inserted>
    proc splice(Stack_T* stack, no_infer(Size) at, no_infer(Size) replace_size, Inserted moved inserted) -> Error_Option<Assign_Error<Stack_T>>
    {       
        static_assert(std::convertible_to<stdr::range_value_t<Inserted>, T>, "the types must be comaptible");
        using Error_Type = Assign_Error<Stack_T>;

        Stack_T& stack_ref = *stack; //for bounds checks
        assert(is_invariant(*stack));

        constexpr bool do_move_construct = std::is_rvalue_reference_v<decltype(inserted)>;
        constexpr bool do_copy_construct = std::is_trivially_copy_constructible_v<T>;

        let inserted_size = cast(Size) stdr::size(inserted);
        let insert_to = at + inserted_size;
        let replace_to = at + replace_size;
        let remaining = stack_ref.size - replace_to;
        let final_size = stack_ref.size + inserted_size - replace_size;

        assert(0 <= at && at <= stack_ref.size);
        assert(0 <= replace_to && replace_to <= stack_ref.size);

        Size move_inserted_size = 0;

        if(inserted_size > replace_size)
        {
            mut reserve_res = reserve(stack, final_size);
            if(reserve_res == Error())
                return Error(Error_Type{{}, {}, reserve_res});

            let constructed = inserted_size - replace_size;
            let constructed_in_shift = min(constructed, remaining); 
            let construction_gap = constructed - constructed_in_shift;

            move_inserted_size = inserted_size - construction_gap;

            //construct some elements by shifting right previous elements into them
            let to = final_size - constructed_in_shift;
            for (Size i = final_size; i-- > to; )
            {
                if constexpr(do_move_construct)
                    std::construct_at(stack->data + i, move(&stack_ref[i - constructed]));
                else if(do_copy_construct)
                    std::construct_at(stack->data + i, stack_ref[i - constructed]);
                else
                {
                    mut res = construct_assign_at(stack->data + i, stack_ref[i - constructed]);
                    if(res == Error())
                        return Error(Error_Type{error(move(&res)), 0});
                }
            }

            //shifting right rest of the elems to make space for insertion
            for(Size i = stack_ref.size; i-- > insert_to; )
                stack_ref[i] = move(&stack_ref[i - constructed]);
        }
        else
        {
            move_inserted_size = inserted_size; 

            //move elems left to the freed space
            let removed = replace_size - inserted_size;
            for (Size i = insert_to; i < final_size; i++)
                stack_ref[i] = move(&stack_ref[i + removed]);

            //Destroy all excess elems
            if constexpr(std::is_trivially_destructible_v<T> == false)
                for (Size i = final_size; i < stack_ref.size; i++)
                    stack_ref[i].~T();
        }

        stack->size = final_size;

        //insert the added elems into constructed slots
        mut it = stdr::begin(inserted);
        let move_assign_to = at + move_inserted_size;
        for (Size i = at; i < move_assign_to; i++, ++it)
        {
            if constexpr(do_move_construct)
            {
                T in val = *it; //either copy or reference 
                //either way now ve can actually get ptr to it and pass it to move
                stack_ref[i] = move(&val);
            }
            else if(do_copy_construct)
                stack_ref[i] = *it;
            else
            {
                mut res = assign(&stack_ref[i], *it);
                if(res == Error())
                    return Error(Error_Type{error(move(&res)), 0});
            }
        }

        //insert construct leftover elements
        for (Size i = move_assign_to; i < insert_to; i++, ++it)
        {
            T* prev = stack->data + i;
            if constexpr(do_move_construct)
            {
                T in val = *it; //either copy or reference 
                std::construct_at(stack->data + i, move(&val));
            }
            else if(do_copy_construct)
                std::construct_at(stack->data + i, *it);
            else
            {
                mut res = construct_assign_at(stack->data + i, *it);
                if(res == Error())
                    return Error(Error_Type{error(move(&res)), 0});
            }
        }

        assert(is_invariant(*stack));
        return Value();
    }

    template <STACK_TEMPL, stdr::forward_range Removed, stdr::forward_range Inserted>
    proc splice(Stack_T* stack, no_infer(Size) at, Removed* removed, Inserted moved inserted) -> Error_Option<Assign_Error<Stack_T>>
    {       
        static_assert(std::convertible_to<stdr::range_value_t<Inserted>, T>, "the types must be comaptible");
        static_assert(std::convertible_to<stdr::range_value_t<Removed>, T>, "the types must be comaptible");
        Stack_T& stack_ref = *stack; //for bounds checks

        mut it = stdr::begin(*removed);
        let end = stdr::end(*removed);
        Size i = at;
        for (; it != end; i++, ++it)
            *it = move(&stack_ref[i]);

        return splice(stack, at, i - at, forward(Inserted, inserted));
    }

    template <STACK_TEMPL>
    proc splice(Stack_T* stack, no_infer(Size) at, no_infer(Size) replace_size) -> Error_Option<Assign_Error<Stack_T>>
    {
        Slice<T, Size> empty;
        return splice(stack, at, replace_size, move(&empty));
    }

    template <STACK_TEMPL>
    proc push(Stack_T* stack, no_infer(T) what) -> Alloc_State
    {
        assert(is_invariant(*stack));
        using Error_Type = Assign_Error<Stack_T>;

        mut reserve_res = reserve(stack, stack->size + 1);
        if(reserve_res == Error())
            return reserve_res;
        
        
        T* ptr = stack->data + stack->size;
        std::construct_at(ptr, move(&what));
        stack->size++;

        assert(is_invariant(*stack));
        return Alloc_State::OK;
    }

    template <STACK_TEMPL>
    proc pop(Stack_T* stack) -> T
    {
        assert(is_invariant(*stack));
        assert(stack->size != 0);

        stack->size--;

        T ret = move(&stack->data[stack->size]);
        stack->data[stack->size].~T();
        assert(is_invariant(*stack));
        return ret;
    }

    template <STACK_TEMPL, stdr::forward_range Inserted> requires (!same<Inserted, T>) 
    proc push(Stack_T* stack, Inserted moved inserted) -> Error_Option<Assign_Error<Stack_T>>
    {
        static_assert(std::convertible_to<stdr::range_value_t<Inserted>, T>, "the types must be comaptible");
        return splice(stack, stack->size, 0, std::forward<Inserted>(inserted));
    }

    template <STACK_TEMPL>
    proc pop(Stack_T* stack, no_infer(Size) count) -> void
    {
        let res = splice(stack, stack->size - count, count);
        assert(res == Value());
    }

    template <STACK_TEMPL>
    proc back(Stack_T* stack) -> T*
    {
        assert(is_invariant(*stack));
        assert(stack->size > 0);
        return &stack->data[stack->size - 1];
    }

    template <STACK_TEMPL>
    proc back(Stack_T in stack) -> T in
    {
        assert(is_invariant(stack));
        assert(stack.size > 0);
        return stack.data[stack.size - 1];
    }

    template <STACK_TEMPL>
    proc front(Stack_T* stack) -> T*
    {
        assert(is_invariant(*stack));
        assert(stack->size > 0);
        return &stack->data[0];
    }

    template <STACK_TEMPL>
    proc front(Stack_T in stack) -> T in
    {
        assert(is_invariant(stack));
        assert(stack.size > 0);
        return stack.data[0];
    }

    template <STACK_TEMPL, bool is_zero = false>
    proc resize(Stack_T* stack, no_infer(Size) to, no_infer(T) fillWith) -> Error_Option<Assign_Error<Stack_T>>
    {
        assert(is_invariant(*stack));
        assert(0 <= to);
        using Error_Type = Assign_Error<Stack_T>;

        mut reserve_res = reserve(stack, to);
        if(reserve_res == Error())
            return Error(Error_Type{{}, {}, reserve_res});

        if(is_zero && std::is_scalar_v<T> && is_const_eval() == false)
        {
            if(stack->size < to)
                memset(stack->data + stack->size, 0, (to - stack->size)*sizeof(T));
        }
        else
        {
            for (Size i = stack->size; i < to; i++)
            {
                if constexpr(std::is_trivially_copy_constructible_v<T>)
                    stack->data[i] = fillWith;
                else
                {
                    mut res = construct_assign_at(stack->data + i, fillWith);
                    if(res == Error())
                        return Error(Error_Type{error(move(&res)), 0});
                }
            }

            if constexpr(std::is_trivially_destructible_v<T>)
                for (Size i = to; i < stack->size; i++)
                    stack->data[i].~T();
        }

        stack->size = to;
        assert(is_invariant(*stack));
        return Value();
    }

    template <STACK_TEMPL>
    proc resize(Stack_T* stack, no_infer(Size) to) -> Error_Option<Assign_Error<Stack_T>>
    {
        return resize<T, static_cap, Size, Alloc, Grow, true>(stack, to, T());
    }

    template <STACK_TEMPL>
    proc resize_for_overwrite(Stack_T* stack, no_infer(Size) to) -> Alloc_State
    {
        static_assert(std::is_trivially_constructible_v<T>, "type must be POD!");
        mut reserve_res = reserve(stack, to);
        if(reserve_res == Error())
            return reserve_res;

        stack->size = to;
        assert(is_invariant(*stack));
        return Alloc_State::OK;
    }

    template <STACK_TEMPL>
    proc insert(Stack_T* stack, no_infer(Size) at, no_infer(T) what) -> T*
    {
        assert(is_invariant(*stack));
        assert(0 <= at && at <= stack->size);

        Slice<T> view = {&what, 1};
        splice(stack, at, 0, move(*view));
        return stack->data + at;
    }

    template <STACK_TEMPL>
    proc remove(Stack_T* stack, no_infer(Size) at) -> T
    {
        assert(is_invariant(*stack));
        assert(0 <= at && at < stack->size);
        assert(0 < stack->size);

        T removed = move(&stack->data[at]);
        splice(stack, at, 1);
        return removed;
    }

    template <STACK_TEMPL>
    proc unordered_remove(Stack_T* stack, no_infer(Size) at) -> T
    {
        assert(0 <= at && at < stack->size);
        assert(0 < stack->size);

        swap(&stack->data[at], back(stack));
        return pop(stack);
    }

    template <STACK_TEMPL>
    proc unordered_insert(Stack_T* stack, no_infer(Size) at, no_infer(T) what) -> Alloc_State
    {
        assert(0 <= at && at <= stack->size);

        mut res = push(stack, move(&what));
        if(res == Error())
            return res;

        swap(&stack->data[at], back(stack));
        return Alloc_State::OK;
    }
}

namespace std 
{
    template <STACK_TEMPL>
    proc swap(jot::Stack_T& stack1, jot::Stack_T& stack2) -> void
    {
        jot::swap(&stack1, &stack2);
    }
}

#undef STACK_TEMPL
#undef Stack_T
#include "undefs.h"