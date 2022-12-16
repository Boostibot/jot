#pragma once

#include <memory>

#include "utils.h"
#include "types.h"
#include "slice.h"
#include "allocator.h"
#include "defines.h"

namespace jot
{
    namespace detail
    {   
        template<non_void T, integral Size, allocator Alloc, tsize static_capacity_>
        struct Stack_Data : Alloc
        {
            T* data = nullptr;
            Size size = 0;
            Size capacity = 0;

            //has to be bytes or else will default construct all elements 
            // (even if we provide explicit default constructor and leave the array uninit)
            //@NOTE: This also prevents Stack_Data with non zero static_capacity_ from being used by constexpr functions
            //@NOTE: For now zero initializing the static_data_ as well; Not sure if it makes any sense to do so
            byte static_data_[static_capacity_ * sizeof(T)] = {0};

            constexpr auto operator<=>(const Stack_Data&) const noexcept = default;
            constexpr bool operator==(const Stack_Data&) const noexcept = default;
        };

        template<non_void T, integral Size, allocator Alloc>
        struct Stack_Data<T, Size, Alloc, 0> : Alloc
        {
            T* data = nullptr;
            Size size = 0;
            Size capacity = 0;

            constexpr auto operator<=>(const Stack_Data&) const noexcept = default;
            constexpr bool operator==(const Stack_Data&) const noexcept = default;
        };
    }


    template <typename Grow>
    concept grower = requires(tsize to_fit, tsize capacity, tsize size, tsize static_capacity, tsize elem_size)
    {
        { Grow::run(to_fit, capacity, size, static_capacity, elem_size) } -> std::convertible_to<tsize>;
    };

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

    template <
        non_void T, 
        tsize static_capacity_, 
        integral Size_, 
        allocator Alloc_, 
        grower Grow
    >
    struct Stack;

    #define STACK_TEMPL class T, ::jot::tsize scap, class Size, class Alloc, class Grow
    #define Stack_T Stack<T, scap, Size, Alloc, Grow>


    template<STACK_TEMPL>
    func slice(Stack_T in data) -> Slice<T>;

    template<STACK_TEMPL>
    func static_data(Stack_T* data) -> T*;

    template<STACK_TEMPL>
    func static_data(Stack_T in data) -> const T*;

    template <STACK_TEMPL>
    func alloc(Stack_T in vec) -> Alloc in { return *cast(const Alloc*) &vec; }

    template <STACK_TEMPL>
    func alloc(Stack_T* vec) -> Alloc* { return cast(Alloc*) vec; }

    template<STACK_TEMPL>
    proc assign(Stack_T* to, Stack_T in other) -> void;

    template<STACK_TEMPL>
    proc swap(Stack_T* left, Stack_T* right) noexcept -> void;

    namespace detail 
    {
        template<STACK_TEMPL>
        proc dealloc_data(Stack_T* vec) -> void;

        template<STACK_TEMPL>
        proc destroy(Stack_T* vec) -> void;
    }

    static_assert(grower<Def_Grow<2, 0, 8>>);

    template <
        non_void T, 
        tsize static_capacity_ = 0, 
        integral Size_ = tsize, 
        allocator Alloc_ = Poly_Allocator, 
        grower Grow = Def_Grow<2, 0, 8>
    >
    struct Stack : detail::Stack_Data<T, Size_, Alloc_, static_capacity_>
    {   
        using Value = T;
        using Size = Size_;
        using Alloc = Alloc_;

        using allocator_type   = Alloc;
        using slice_type       = Slice<Value, Size>;
        using const_slice_type = Slice<const Value, Size>;
        using grow_type        = Grow;
        using Stack_Data       = detail::Stack_Data<Value, Size, Alloc, static_capacity_>;

        constexpr static Size static_capacity = cast(Size) static_capacity_; 
        constexpr static bool has_static_storage = static_capacity != 0;
        constexpr static bool has_stateful_alloc = !std::is_empty_v<Alloc>;

        constexpr Stack() = default;
        constexpr Stack(Stack&& other, Alloc alloc = Alloc()) noexcept
            : Stack_Data{alloc} { detail::swap(this, &other); }

        constexpr Stack(
            Value* data, Size size, Size capacity, 
            Alloc alloc, const Value (&static_data)[static_capacity]
        ) noexcept requires (has_static_storage)
            : Stack_Data{alloc, data, size, capacity} 
        {
            for(Size i = 0; i < static_capacity; i++)
                static_data(this)[i] = static_data[i];
        }

        constexpr Stack(Value* data, Size size, Size capacity, Alloc alloc = Alloc()) noexcept
            : Stack_Data{alloc, data, size, capacity} {}

        constexpr Stack(Alloc alloc) noexcept
            : Stack_Data{alloc} {}

        #if 0
        explicit constexpr Stack(Size size, Alloc alloc = Alloc()) 
            : Stack_Data{alloc} { detail::alloc_data(this, size); }

        explicit constexpr Stack(slice_type slice, Alloc alloc = Alloc()) 
            : Stack_Data{alloc} { detail::to_owned(this, slice); }

        explicit constexpr Stack(const_slice_type slice, Alloc alloc = Alloc()) 
            : Stack_Data{alloc} { detail::to_owned(this, slice); }

        constexpr Stack(const Stack& other)
            requires (std::is_copy_constructible_v<Value> && std::is_copy_constructible_v<Alloc>) 
            : Stack_Data{alloc(other)}
        {
            let slice = cast(slice_type) other;
            detail::to_owned(this, slice);
        }
        #endif

        constexpr ~Stack() noexcept
        {
            detail::destroy(this);
            detail::dealloc_data(this);
        }

        constexpr Stack& operator=(const Stack& other)
            requires (std::is_copy_assignable_v<Value> || std::is_copy_constructible_v<Value>)
        {
            assign(this, other);
            return *this;
        }

        constexpr Stack& operator=(Stack&& other) noexcept
        {
            detail::swap(this, &other);
            return *this;
        }

        #include "slice_op_text.h"
    };
}

namespace std 
{
    template <STACK_TEMPL>
    proc swap(jot::Stack_T& stack1, jot::Stack_T& stack2)
    {
        return jot::swap(&stack1, &stack2);
    }
}

namespace jot
{
    template<STACK_TEMPL>
    func slice(Stack_T in stack) -> Slice<T>{
        return Slice<T>{stack.data, stack.size};
    }

    template <STACK_TEMPL>
    func static_data(Stack_T* data) -> T*
    {
        if constexpr(scap != 0)
            return cast(T*) cast(void*) data->static_data_;
        else
            return nullptr;
    }

    template <STACK_TEMPL>
    func static_data(Stack_T in data) -> const T*
    {
        if constexpr(scap != 0)
            return cast(T*) cast(void*) data.static_data_;
        else
            return nullptr;
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
    func is_static_alloced(Stack_T in stack) noexcept
    {
        assert(is_invariant(stack));
        return stack.capacity == stack.static_capacity && stack.has_static_storage; 
    }

    template <STACK_TEMPL>
    proc call_grow_function(Stack_T in stack, no_infer(Size) to_fit) -> Size
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
        constexpr let transfer_static = [](Stack* from, Stack* to, Size from_i, Size to_i)
        {
            for (Size i = from_i; i < to_i; i++)
            {
                std::construct_at(static_data(to) + i, std::move(static_data(from)[i]));
                static_data(from)[i].~T();
            }
        };

        constexpr let transfer_half_static = [=](Stack* from, Stack* to)
        {
            transfer_static(from, to, 0, from->size);

            from->data = to->data;
            to->data = static_data(to);
        };

        if (is_static_alloced(*left) && is_static_alloced(*right))
        {
            Size to_size = 0;
            if (left->size < right->size)
            {
                to_size = left->size;
                transfer_static(right, left, left->size, right->size);
            }
            else
            {
                to_size = right->size;
                transfer_static(left, right, right->size, left->size);
            }

            for (Size i = 0; i < to_size; i++)
            {
                T& left_item = (*left)[i];
                T& right_item = (*right)[i];
                swap(&left_item, &right_item);
            }
        }
        else if (is_static_alloced(*left))
            transfer_half_static(left, right);
        else if (is_static_alloced(*right))
            transfer_half_static(right, left);
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

            mut new_capacity = call_grow_function(*vec, capacity);
            mut new_info = make_def_alloc_info<T>(new_capacity);
            mut my_alloc = Comptime_Allocator_Adaptor<Alloc, T>(alloc(vec));
            let res = my_alloc.allocate(new_info);
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
                mut old_info = make_def_alloc_info<T>(vec->capacity);
                mut my_alloc = Comptime_Allocator_Adaptor<Alloc, T>(alloc(vec));
                mut my_slice = Slice<T>{vec->data, vec->capacity};
                return my_alloc.deallocate(my_slice, old_info);
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
            T* new_data = nullptr;

            if(new_capacity <= vec->static_capacity)
            {
                new_data = static_data(vec);
                new_capacity = vec->static_capacity;
            }
            else
            {
                let my_slice = Slice{vec->data, vec->capacity};
                let new_info = make_def_alloc_info<T>(new_capacity);
                let prev_info = make_def_alloc_info<T>(vec->capacity);

                mut my_alloc = Comptime_Allocator_Adaptor<T, Alloc>(alloc(vec));

                let resize_res = my_alloc.action(
                    Alloc_Actions::RESIZE, 
                    nullptr, 
                    my_slice, 
                    new_info, prev_info, 
                    nullptr);
                if(resize_res.state == Alloc_State::OK)
                {
                    vec->data = resize_res.slice.data;
                    vec->capacity = resize_res.slice.size;
                    return;
                }
                else
                {
                     let res = my_alloc.allocate(new_info);
                     if(res.state != Alloc_State::OK)
                        return res.state;

                     new_data = res.slice.data;
                }
            }

            let copy_to = std::min(vec->size, new_capacity);
            Slice<T, Size> new_filled_buff = {new_data, copy_to};
            Slice<T, Size> old_filled_buff = {vec->data, copy_to};
            move_construct(&new_filled_buff, old_filled_buff);

            //for (Size i = 0; i < copy_to; i++)
                //std::construct_at(new_data + i, std::move(vec->data[i]));

            Slice<T, Size> whole_old_buff = slice(*vec);
            destroy(&whole_old_buff);
            dealloc_data(vec);

            vec->data = new_data;
            vec->capacity = new_capacity;

            Alloc_State::OK;
        }

        template<STACK_TEMPL>
        proc by_byte_assign(Stack_T* to, Stack_T in from) -> void
        {
            to->data = from.data;
            to->size = from.size;
            to->capacity = from.capacity;
            *alloc(to) = alloc(from);
        }

        template<STACK_TEMPL, typename T_>
        proc to_owned(Stack_T* vec, Slice<T_, Size> in other) -> void 
        {
            alloc_data(vec, other.size);
            copy_construct(vec, other);
            vec->size = other.size;
        }
    }


    template<STACK_TEMPL>
    proc assign(Stack_T* to, Stack_T in other, Assign_Proc<T> set_fn = assign<T>) -> Alloc_State
    {
        if(to->capacity < other.size)
        {
            let res = detail::set_capacity(to, other.size);
            if(res != Alloc_State::OK)
                return res;

            //@TODO
            copy_construct(to, other);
            to->size = other.size;
            return res;
        }

        //If the elems are copy constructible we can save potential allocations of elems
        // by using copy assignment instead (for example with Stack<Stack<>...>)
        if constexpr (std::is_copy_assignable_v<T>)
        {
            //destroy extra
            for (Size i = other.size; i < to->size; i++)
                to->data[i].~T();

            //construct missing
            for (Size i = to->size; i < other.size; i++)
                std::construct_at(to->data + i, other.data[i]);

            //copy rest
            Size to_size = std::min(to->size, other.size);
            for (Size i = 0; i < to_size; i++)
            {
                let res = set_fn(&to->data[i], other.data[i]);
                assert(res == Alloc_State::OK && "TEMP!");
            }

            to->size = other.size;
        }
        else
        {
            destroy(to);
            copy_construct(to, other);

            to->size = other.size;
        }
      
        return Alloc_State::OK;
    }


    template <STACK_TEMPL>
    proc reserve(Stack_T* stack, no_infer(Size) to_fit) -> bool
    {
        assert(is_invariant(*stack));

        if (stack->capacity >= to_fit)
            return false;

        Size realloc_to = 0;
        //@NOTE: The below line could be merged with the same check in set_capacity
        // (ie. there could be a version of set_capacity that doesnt have this check)
        // but the improvemnt is so minor I have excluded it
        if (to_fit <= stack->static_capacity)
            realloc_to = stack->static_capacity;
        else
            realloc_to = call_grow_function(*stack, to_fit);

        assert(realloc_to >= to_fit);
        detail::set_capacity(stack, realloc_to);

        assert(is_invariant(*stack));
        return true;
    }

    template <STACK_TEMPL>
    proc clear(Stack_T* stack) -> void
    {
        assert(is_invariant(*stack));
        detail::destroy(stack);
        stack.size = 0;
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
    proc splice(Stack_T* stack, no_infer(Size) at, no_infer(Size) replace_size, Inserted&& inserted) -> void
    {       
        Stack_T& stack_ref = *stack; //for bounds checks
        assert(is_invariant(*stack));

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
            reserve(stack, final_size);
            let constructed = inserted_size - replace_size;
            let constructed_in_shift = min(constructed, remaining); 
            let construction_gap = constructed - constructed_in_shift;

            move_inserted_size = inserted_size - construction_gap;

            //construct some elements by shifting right previous elements into them
            let to = final_size - constructed_in_shift;
            for (Size i = final_size; i-- > to; )
            {
                if constexpr(std::is_rvalue_reference_v<decltype(inserted)>)
                    std::construct_at(stack->data + i, std::move(stack_ref[i - constructed]));
                else
                    std::construct_at(stack->data + i, stack_ref[i - constructed]);
            }

            //shifting right rest of the elems to make space for insertion
            for(Size i = stack_ref.size; i-- > insert_to; )
                stack_ref[i] = std::move(stack_ref[i - constructed]);
        }
        else
        {
            move_inserted_size = inserted_size; 

            //move elems left to the freed space
            let removed = replace_size - inserted_size;
            for (Size i = insert_to; i < final_size; i++)
                stack_ref[i] = std::move(stack_ref[i + removed]);

            //Destroy all excess elems
            for (Size i = final_size; i < stack_ref.size; i++)
                stack_ref[i].~T();
        }

        stack->size = final_size;

        //insert the added elems into constructed slots
        mut it = stdr::begin(inserted);
        let move_assign_to = at + move_inserted_size;
        for (Size i = at; i < move_assign_to; i++, ++it)
        {
            if constexpr(std::is_rvalue_reference_v<decltype(inserted)>)
                stack_ref[i] = std::move(*it);
            else
                stack_ref[i] = *it;
        }

        //insert construct leftover elements
        for (Size i = move_assign_to; i < insert_to; i++, ++it)
        {
            if constexpr(std::is_rvalue_reference_v<decltype(inserted)>)
                std::construct_at(stack->data + i, std::move(*it));
            else
                std::construct_at(stack->data + i, *it);
        }

        assert(is_invariant(*stack));
    }

    template <STACK_TEMPL, stdr::forward_range Removed, stdr::forward_range Inserted>
    proc splice(Stack_T* stack, no_infer(Size) at, Removed& removed, Inserted&& inserted) -> void
    {       
        Stack_T& stack_ref = *stack; //for bounds checks

        mut it = stdr::begin(removed);
        let end = stdr::end(removed);
        Size i = at;
        for (; it != end; i++, ++it)
            *it = std::move(stack_ref[i]);

        return splice(stack, at, i - at, std::forward<Inserted>(inserted));
    }

    template <STACK_TEMPL>
    proc splice(Stack_T* stack, no_infer(Size) at, no_infer(Size) replace_size) -> void
    {
        Slice<T, Size> empty;
        return splice(stack, at, replace_size, std::move(empty));
    }

    template <STACK_TEMPL>
    proc push(Stack_T* stack, no_infer(T) what) -> T*
    {
        assert(is_invariant(*stack));

        reserve(stack, stack->size + 1);
        T* ptr = stack->data + stack->size;
        std::construct_at(ptr, std::move(what));
        stack->size++;

        assert(is_invariant(*stack));
        return stack->data + stack->size - 1;
    }

    template <STACK_TEMPL>
    proc pop(Stack_T* stack) -> T
    {
        assert(is_invariant(*stack));
        assert(stack->size != 0);

        stack->size--;

        T ret = std::move(stack->data[stack->size]);
        stack->data[stack->size].~T();
        assert(is_invariant(*stack));
        return ret;
    }

    template <STACK_TEMPL, typename ... _Ts>
        requires (std::convertible_to<_Ts, T> && ...)
    proc push(Stack_T* stack, _Ts&& ... what) -> void
    {
        splice(stack, stack->size, 0, Array<T, sizeof...(_Ts)>{cast(T)(what)...});
    }

    template <STACK_TEMPL, stdr::forward_range Inserted>
        requires std::convertible_to<stdr::range_value_t<Inserted>, T>
    proc push(Stack_T* stack, Inserted&& inserted) -> void
    {
        splice(stack, stack->size, 0, std::forward<Inserted>(inserted));
    }

    //@TODO: add element type checks for ranges

    template <STACK_TEMPL>
    proc pop(Stack_T* stack, no_infer(Size) count) -> void
    {
        splice(stack, stack->size - count, count);
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

    template <STACK_TEMPL, typename Fn>
    proc resize(Stack_T* stack, no_infer(Size) to, Fn filler_fn) -> void
        requires requires() { {filler_fn(0)} -> std::convertible_to<T>; }
    {
        assert(is_invariant(*stack));
        assert(0 <= to);

        reserve(stack, to);
        for (Size i = stack->size; i < to; i++)
            std::construct_at(stack->data + i, filler_fn(i));

        for (Size i = to; i < stack->size; i++)
            stack->data[i].~T();
        stack->size = to;

        assert(is_invariant(*stack));
    }

    template <STACK_TEMPL>
    proc resize(Stack_T* stack, no_infer(Size) to, no_infer(T) fillWith) -> void
    {
        return resize(stack, to, [&](Size){return fillWith; });
    }

    template <STACK_TEMPL>
    proc resize(Stack_T* stack, no_infer(Size) to) -> void
    {
        return resize(stack, to, T());
    }

    #ifndef SLICE_OWN_INSERT
        template <STACK_TEMPL>
        proc insert(Stack_T* stack, no_infer(Size) at, no_infer(T) what) -> T*
        {
            assert(is_invariant(*stack));
            assert(0 <= at && at <= stack->size);

            Slice<T> view = {&what, 1};
            splice(stack, at, 0, std::move(view));
            return stack->data + at;
        }

        template <STACK_TEMPL>
        proc remove(Stack_T* stack, no_infer(Size) at) -> T
        {
            assert(is_invariant(*stack));
            assert(0 <= at && at < stack->size);
            assert(0 < stack->size);

            T removed = std::move(stack->data[at]);
            splice(stack, at, 1);
            return removed;
        }
    #else
        template <STACK_TEMPL>
        proc insert(Stack_T* stack, no_infer(Size) at, no_infer(T) what) -> T*
        {
            assert(is_invariant(*stack));
            assert(0 <= at && at <= stack->size);

            if(stack->size == at)
                return push(stack, what);

            push(stack, std::move(back(stack)));
            for (Size i = stack->size - 1; i-- > at; )
                stack->data[i + 1] = std::move(stack->data[i]);

            stack->data[at] = std::move(what);
            assert(is_invariant(*stack));
            return stack->data + at;
        }

        template <STACK_TEMPL>
        proc remove(Stack_T* stack, no_infer(Size) at) -> T
        {
            assert(is_invariant(*stack));
            assert(0 <= at && at < stack->size);
            assert(0 < stack->size);

            T removed = std::move(stack->data[at]);

            for (Size i = at; i < stack->size - 1; i++)
                stack->data[i] = std::move(stack->data[i + 1]);

            pop(stack);

            assert(is_invariant(*stack));
            return removed;
        }
    #endif // SLICE_SPLICE_INSERT

    template <STACK_TEMPL>
    proc unordered_remove(Stack_T* stack, no_infer(Size) at) -> T
    {
        assert(0 <= at && at < stack->size);
        assert(0 < stack->size);

        std::swap(stack->data[at], back(stack));
        return pop(stack);
    }

    template <STACK_TEMPL>
    proc unordered_insert(Stack_T* stack, no_infer(Size) at, no_infer(T) what) -> T*
    {
        assert(0 <= at && at <= stack->size);

        push(stack, what);
        std::swap(stack->data[at], back(stack));
        return stack->data + at;
    }

    #undef STACK_TEMPL
    #undef Stack_T
}


#include "undefs.h"