#pragma once

#include <memory>
//#include <bit>

#include "utils.h"
#include "slice.h"
#include "allocator.h"
#include "defines.h"

namespace jot
{

    typedef size_t (*Grow_Fn)(size_t, size_t, size_t, size_t);

    namespace detail
    {   
        template<typename T, typename Size, class Alloc, size_t static_capacity_>
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

            func static_data() noexcept -> T* 
                {return cast(T*) cast(void*) static_data_;}
            func static_data() const noexcept -> const T* 
                {return cast(const T*) cast(const void*) static_data_;}

            constexpr auto operator<=>(const Stack_Data&) const noexcept = default;
            constexpr bool operator==(const Stack_Data&) const noexcept = default;
        };

        template<typename T, typename Size, class Alloc>
        struct Stack_Data<T, Size, Alloc, 0> : Alloc
        {
            T* data = nullptr;
            Size size = 0;
            Size capacity = 0;

            func static_data() noexcept -> T* {return nullptr;}
            func static_data() const noexcept -> const T* {return nullptr;}

            constexpr auto operator<=>(const Stack_Data&) const noexcept = default;
            constexpr bool operator==(const Stack_Data&) const noexcept = default;
        };
    }

    using Def_Size = size_t;

    template <class T>
    using Def_Alloc = std::allocator<T>;

    template<size_t MULT_, size_t ADD_, size_t BASE_ELEMS_>
    struct Def_Grow
    {
        static constexpr size_t MULT = MULT_;
        static constexpr size_t ADD = ADD_;
        static constexpr size_t BASE_ELEMS = BASE_ELEMS_;

        static constexpr size_t run(size_t to_fit, size_t capacity, size_t size, size_t static_capacity, size_t elem_size)
        {
            cast(void) size;
            cast(void) static_capacity;
            cast(void) elem_size;

            size_t realloc_to = capacity 
                ? capacity * MULT + ADD 
                : BASE_ELEMS;

            while(realloc_to < to_fit)
                realloc_to = realloc_to * MULT + ADD;

            return realloc_to;
        }
    };


    template <
        typename T, 
        size_t static_capacity_ = 0, 
        typename Size = Def_Size, 
        typename Alloc = Def_Alloc<T>, 
        typename Grow = Def_Grow<2, 0, 8>
    >
    struct Stack_ : detail::Stack_Data<T, Size, Alloc, static_capacity_>
    {   
        using allocator_type   = Alloc;
        using slice_type       = Slice_<T, Size>;
        using const_slice_type = Slice_<const T, Size>;
        using grow_type        = Grow;
        using Stack_Data       = detail::Stack_Data<T, Size, Alloc, static_capacity_>;

        using Tag = std::conditional_t<static_capacity_ != 0, StaticContainerTag, void>;

        constexpr static Size static_capacity = cast(Size) static_capacity_; 
        constexpr static bool has_static_storage = static_capacity != 0;
        constexpr static bool has_stateful_alloc = !std::is_empty_v<Alloc>;

        constexpr Stack_() = default;
        constexpr Stack_(Stack_&& other) noexcept { swap(this, &other); }

        explicit constexpr Stack_(Size size) { alloc_data(this, size); }

        explicit constexpr Stack_(slice_type slice) 
        { 
            alloc_data(this, slice.size);
            copy_construct_elems(this, slice);
        }

        explicit constexpr Stack_(const_slice_type slice) 
        { 
            alloc_data(this, slice.size);
            copy_construct_elems(this, slice);
        }

        constexpr Stack_(
            T* data, Size size, Size capacity, 
            Alloc alloc, const T (&static_data)[static_capacity]
        ) noexcept requires (has_static_storage)
            : Stack_Data{alloc, data, size, capacity} 
        {
            for(Size i = 0; i < static_capacity; i++)
                this->static_data()[i] = static_data[i];
        }

        constexpr Stack_(T* data, Size size, Size capacity, Alloc alloc = Alloc()) noexcept
            : Stack_Data{alloc, data, size, capacity} {}

        constexpr Stack_(Alloc alloc) noexcept
            : Stack_Data{alloc} {}

        constexpr Stack_(const Stack_& other)
            requires (std::is_copy_constructible_v<T>) : Stack_Data{*other.alloc()}
        {
            puts("copy stack\n");
            alloc_data(this, other.size);
            copy_construct_elems(this, other);
            this->size = other.size;
        }

        constexpr ~Stack_() noexcept
        {
            destroy_elems(this);
            dealloc_data(this);
        }

        constexpr Stack_& operator=(const Stack_& other)
            requires (std::is_copy_assignable_v<T> || std::is_copy_constructible_v<T>)
        {
            if(this->capacity >= other.size)
            {
                //If the elems are copy constructible we can save potential allocations of elems
                // by using copy assignment instead (for example with Stack_<Stack_<>...>)
                if constexpr (std::is_copy_assignable_v<T>)
                {
                    //destroy extra
                    for (Size i = other.size; i < this->size; i++)
                        this->data[i].~T();

                    //construct missing
                    for (Size i = this->size; i < other.size; i++)
                        std::construct_at(this->data + i, other.data[i]);

                    //copy rest
                    Size to_size = std::min(this->size, other.size);
                    for (Size i = 0; i < to_size; i++)
                        this->data[i] = other.data[i];

                    this->size = other.size;
                }
                else
                {
                    destroy_elems(this);
                    copy_construct_elems(this, other);

                    this->size = other.size;
                }
            }
            else
            {
                destroy_elems(this);
                dealloc_data(this);
                
                alloc_data(this, other.size);
                copy_construct_elems(this, other);
                
                this->size = other.size;
            }

            return *this;
        }

        constexpr Stack_& operator=(Stack_&& other) noexcept
        {
            swap(this, &other);
            return *this;
        }

        //this is pretty pointless since its impossible to create a by byte copy...
        func operator<=>(const Stack_&) const noexcept = default;
        constexpr bool operator==(const Stack_&) const noexcept = default;

        constexpr operator Slice_<T, Size>() const noexcept 
        {
            return Slice_<T, Size>{this->data, this->size};
        }

        proc alloc() noexcept -> Alloc* {return cast(Alloc*)this; }
        proc alloc() const noexcept -> Alloc const* {return cast(Alloc*)this; }

        static proc is_static_alloced(const Stack_& vec) noexcept -> bool 
        { 
            return vec.capacity == static_capacity && has_static_storage; 
        }

        static proc swap(Stack_* left, Stack_* right) noexcept -> void
        {
            constexpr let transfer_static = [](Stack_* from, Stack_* to, Size from_i, Size to_i)
            {
                for (Size i = from_i; i < to_i; i++)
                {
                    std::construct_at(to->static_data() + i, std::move(from->static_data()[i]));
                    from->static_data()[i].~T();
                }
            };

            constexpr let transfer_half_static = [=](Stack_* from, Stack_* to)
            {
                transfer_static(from, to, 0, from->size);

                from->data = to->data;
                to->data = to->static_data();
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
                    std::swap(left_item, right_item);
                }
            }
            else if (is_static_alloced(*left))
                transfer_half_static(left, right);
            else if (is_static_alloced(*right))
                transfer_half_static(right, left);
            else
                std::swap(left->data, right->data);

            std::swap(left->size, right->size);
            std::swap(left->capacity, right->capacity);

            if constexpr(has_stateful_alloc)
                std::swap(*left->alloc(), *right->alloc());
        }

        static proc alloc_data(Stack_* vec, Size capacity) -> void
        {
            if(capacity <= static_capacity)
            {
                //We could also have Stack_Data with static capacity > 0 default init to these values
                // - that however would make it so that default init is not zero init which could be a slow down
                //The reallocating path is expensive anyways and this check will fire in so little cases 
                // (actually only once) that it doesnt matter
                if(vec->capacity < static_capacity)
                {
                    vec->capacity = static_capacity;
                    vec->data = vec->static_data();
                }

                return;
            }

            let realloc_to = cast(Size) grow_type::run(
                cast(size_t) capacity,
                cast(size_t) vec->capacity,
                cast(size_t) vec->size,
                cast(size_t) vec->static_capacity,
                sizeof(T)
            );

            vec->capacity = realloc_to;
            //vec->data = allocate(vec->alloc(), vec->capacity);
            vec->data = vec->allocate(vec->capacity);
        }

        static proc dealloc_data(Stack_* vec) -> void
        {
            if(vec->capacity > static_capacity)
                vec->deallocate(vec->data, vec->capacity);
        }

        //can be used for arbitrary growing/shrinking of data
        // when called on uninit stack acts as alloc_data
        // when called with new_capacity = 0 acts as dealloc_data
        // Destroys elements when shrinking but does not construct new ones when growing 
        //  (because doesnt know how to)
        static proc set_capacity(Stack_* vec, Size new_capacity) -> void
        {
            T* new_data = nullptr;

            if(new_capacity <= static_capacity)
            {
                new_data = vec->static_data();
                new_capacity = vec->static_capacity;
            }
            else
                //new_data = allocate(vec->alloc(), new_capacity);
                new_data = vec->allocate(new_capacity);

            let copy_to = std::min(vec->size, new_capacity);
            for (Size i = 0; i < copy_to; i++)
                std::construct_at(new_data + i, std::move(vec->data[i]));

            destroy_elems(vec);
            dealloc_data(vec);

            vec->data = new_data;
            vec->capacity = new_capacity;
        }

        static proc destroy_elems(Stack_* vec) -> void
        {
            for (Size i = 0; i < vec->size; i++)
                vec->data[i].~T();
        }

        static proc copy_construct_elems(Stack_* vec, slice_type other) -> void
        {
            for (Size i = 0; i < other.size; i++)
                std::construct_at(vec->data + i, other.data[i]);
        }

        #include "slice_op_text.h"
    };

    template<typename Stack_>
    concept Stack_Concept = requires(Stack_ stack)
    {
        Stack_::set_capacity(&stack, 0);
        Stack_::destroy_elems(&stack);

        stack.data = nullptr; 
        stack.size = 0; 
        stack.capacity = 0; 

        { Stack_::allocator_type    };
        { Stack_::slice_type        };
        { Stack_::const_slice_type  };
        { Stack_::grow_type         };
        { Stack_::size_type         };
        //{ Stack_::Stack_Data        };
        { stack.static_capacity    } -> std::convertible_to<size_t>;
        { stack.has_static_storage } -> std::convertible_to<bool>;
        { stack.has_stateful_alloc } -> std::convertible_to<bool>; 
    };

    static_assert(Stack_Concept<Stack_<i32, 0>> == true);
    static_assert(Stack_Concept<Stack_<i32, 2>> == true);
    static_assert(Stack_Concept<i32> == false);
}


#define STACK_TEMPL class T, size_t scap, class Size, class Alloc, class Grow
#define Stack_T Stack_<T, scap, Size, Alloc, Grow>

namespace std 
{
    template <STACK_TEMPL>
    proc swap(jot::Stack_T& stack1, jot::Stack_T& stack2)
    {
        return jot::Stack_T::swap(&stack1, &stack2);
    }
}

namespace jot
{
    template <STACK_TEMPL>
    func is_invariant(const Stack_T& stack) -> bool
    {
        const bool size_inv = stack.capacity >= stack.size;
        const bool data_inv = (stack.capacity == 0) == (stack.data == nullptr);
        const bool capa_inv = stack.capacity != 0 
            ? stack.capacity >= stack.static_capacity
            : true;

        return size_inv && capa_inv && data_inv;
    }

    namespace stack
    {
        template <STACK_TEMPL, typename _Size>
        proc realloc(Stack_T* stack, _Size cap)
            requires std::convertible_to<_Size, Size>
        {
            assert(is_invariant(*stack));

            stack->set_capacity(cap);

            assert(is_invariant(*stack));
        }
    }

    template <STACK_TEMPL, typename _Size>
    proc call_grow_function(const Stack_T& stack, _Size to_fit) -> Size
        requires std::convertible_to<_Size, Size>
    {
        return cast(Size) Stack_T::grow_type::run(
            cast(size_t) to_fit, 
            cast(size_t) stack.capacity, 
            cast(size_t) stack.size, 
            cast(size_t) stack.static_capacity,
            sizeof(T)
        );
    }

    template <STACK_TEMPL, typename _Size>
    proc reserve(Stack_T* stack, _Size to_fit) -> bool
        requires std::convertible_to<_Size, Size>
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
        Stack_T::set_capacity(stack, realloc_to);

        assert(is_invariant(*stack));
        return true;
    }

    template <STACK_TEMPL>
    proc clear(Stack_T* stack)
    {
        assert(is_invariant(*stack));
        Stack_T::destroy_elems(stack);
        stack.size = 0;
        assert(is_invariant(*stack));
    }

    template <STACK_TEMPL>
    func empty(Stack_T const& stack) noexcept
    {
        assert(is_invariant(stack));
        return stack.size == 0;
    }

    template <STACK_TEMPL>
    func is_empty(Stack_T const& stack) noexcept
    {
        assert(is_invariant(stack));
        return stack.size == 0;
    }

    template <STACK_TEMPL>
    func is_static_alloced(Stack_T const& stack) noexcept
    {
        assert(is_invariant(stack));
        return Stack_T::is_static_alloced(stack);
    }

    template<typename T, typename Size, auto extent>
    proc shift_left(Slice_<T, Size, extent>* slice, Size by)
    {
        for(Size i = slice->size; i--> 0; )
            slice->data[i + by] = std::move(slice->data[i]);
    }

    template<typename T, typename Size, auto extent>
    proc shift_right(Slice_<T, Size, extent>* slice, Size by)
    {
        for(Size i = 0; i < slice->size; )
            slice->data[i] = std::move(slice->data[i + by]);
    }


    template <STACK_TEMPL, typename _Size1, typename _Size2, stdr::forward_range Inserted>
    proc splice(Stack_T* stack, _Size1 at, _Size2 replace_size, Inserted&& inserted) -> void
        requires std::convertible_to<_Size1, Size> && std::convertible_to<_Size2, Size>
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

    template <STACK_TEMPL, typename _Size, stdr::forward_range Removed, stdr::forward_range Inserted>
    proc splice(Stack_T* stack, _Size at, Removed& removed, Inserted&& inserted) -> void
        requires std::convertible_to<_Size, Size>
    {       
        Stack_T& stack_ref = *stack; //for bounds checks

        mut it = stdr::begin(removed);
        let end = stdr::end(removed);
        Size i = at;
        for (; it != end; i++, ++it)
            *it = std::move(stack_ref[i]);

        return splice(stack, at, i - at, std::forward<Inserted>(inserted));
    }

    template <STACK_TEMPL, typename _Size1, typename _Size2>
    proc splice(Stack_T* stack, _Size1 at, _Size2 replace_size) -> void
        requires std::convertible_to<_Size1, Size> && std::convertible_to<_Size2, Size>
    {
        Slice_<T, Size> empty;
        return splice(stack, at, replace_size, empty);
    }

    template <STACK_TEMPL, typename _T>
    proc push(Stack_T* stack, _T what) -> T*
        requires std::convertible_to<_T, T>
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
    proc push(Stack_T* stack, _Ts ... what) -> void
    {
        splice(stack, stack->size, 0, Array_<T, sizeof...(_Ts)>{std::move(what)...});
    }

    template <STACK_TEMPL, stdr::forward_range Inserted>
        requires std::convertible_to<stdr::range_value_t<Inserted>, T>
    proc push(Stack_T* stack, Inserted&& inserted) -> void
    {
        splice(stack, stack->size, 0, std::forward<Inserted>(inserted));
    }

    //@TODO: add element type checks for ranges

    template <STACK_TEMPL, typename _Size>
    proc pop(Stack_T* stack, _Size count) -> void
        requires std::convertible_to<_Size, Size>
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
    proc back(Stack_T const& stack) -> T const&
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
    proc front(Stack_T const& stack) -> T const&
    {
        assert(is_invariant(stack));
        assert(stack.size > 0);
        return stack.data[0];
    }

    template <STACK_TEMPL, typename _Size, typename Fn>
    proc resize(Stack_T* stack, _Size to, Fn filler_fn) -> void
        requires std::convertible_to<_Size, Size> 
            && requires() { {filler_fn(0)} -> std::convertible_to<T>; }
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

    template <STACK_TEMPL, typename _Size, typename _T>
    proc resize(Stack_T* stack, _Size to, _T fillWith) -> void
        requires std::convertible_to<_Size, Size> && std::convertible_to<_T, T>
    {
        return resize(stack, to, [&](Size){return fillWith; });
    }

    template <STACK_TEMPL, typename _Size>
    proc resize(Stack_T* stack, _Size to) -> void
        requires std::convertible_to<_Size, Size>
    {
        return resize(stack, to, T());
    }

    #ifndef SLICE_OWN_INSERT
        template <STACK_TEMPL, typename _Size, typename _T>
        proc insert(Stack_T* stack, _Size at, _T what) -> T*
            requires std::convertible_to<_Size, Size> && std::convertible_to<_T, T>
        {
            assert(is_invariant(*stack));
            assert(0 <= at && at <= stack->size);

            Slice_<T> view = {&what, 1};
            splice(stack, at, 0, view);
            return stack->data + at;
        }

        template <STACK_TEMPL, typename _Size>
        proc remove(Stack_T* stack, _Size at) -> T
            requires std::convertible_to<_Size, Size>
        {
            assert(is_invariant(*stack));
            assert(0 <= at && at < stack->size);
            assert(0 < stack->size);

            T removed = std::move(stack->data[at]);
            splice(stack, at, 1);
            return removed;
        }
    #else
        template <STACK_TEMPL, typename _Size, typename _T>
        proc insert(Stack_T* stack, _Size at, _T what) -> T*
            requires std::convertible_to<_Size, Size> && std::convertible_to<_T, T>
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

        template <STACK_TEMPL, typename _Size>
        proc remove(Stack_T* stack, _Size at) -> T
            requires std::convertible_to<_Size, Size>
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

    template <STACK_TEMPL, typename _Size>
    proc unordered_remove(Stack_T* stack, _Size at) -> T
        requires std::convertible_to<_Size, Size>
    {
        assert(0 <= at && at < stack->size);
        assert(0 < stack->size);

        std::swap(stack->data[at], back(stack));
        return pop(stack);
    }

    template <STACK_TEMPL, typename _Size, typename _T>
    proc unordered_insert(Stack_T* stack, _Size at, T what) -> T*
        requires std::convertible_to<_Size, Size> && std::convertible_to<_T, T>
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