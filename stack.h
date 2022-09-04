#pragma once

#include <memory>
#include <bit>
#include <vector>

#include "utils.h"
#include "slice.h"
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
    struct Stack : detail::Stack_Data<T, Size, Alloc, static_capacity_>
    {   
        using allocator_type   = Alloc;
        using slice_type       = Slice<T, Size>;
        using const_slice_type = Slice<const T, Size>;
        using grow_type        = Grow;
        using Stack_Data       = detail::Stack_Data<T, Size, Alloc, static_capacity_>;

        using value_type      = T;
        using size_type       = Size;
        using difference_type = ptrdiff_t;
        using pointer         = T*;
        using const_pointer   = const T*;
        using reference       = T&;
        using const_reference = const T&;

        constexpr static Size static_capacity = cast(Size) static_capacity_; 
        constexpr static bool has_static_storage = static_capacity != 0;
        constexpr static bool has_stateful_alloc = !std::is_empty_v<Alloc>;

        constexpr Stack() = default;
        constexpr Stack(Stack&& other) noexcept { swap(this, &other); }
        constexpr Stack(Size size) { alloc_data(size); }

        constexpr Stack(
            T* data, Size size, Size capacity, 
            Alloc alloc, const T (&static_data)[static_capacity]
        ) noexcept requires (has_static_storage)
            : Stack_Data{alloc, data, size, capacity} 
        {
            for(Size i = 0; i < static_capacity; i++)
                this->static_data()[i] = static_data[i];
        }

        constexpr Stack(T* data, Size size, Size capacity, Alloc alloc = Alloc()) noexcept
            : Stack_Data{alloc, data, size, capacity} {}

        constexpr Stack(Alloc alloc) noexcept
            : Stack_Data{alloc} {}

        constexpr Stack(const Stack& other)
            requires (std::is_copy_constructible_v<T>) : Stack_Data{other.alloc()}
        {
            alloc_data(this, other.size);
            copy_construct_elems(this, other);
            this->size = other.size;
        }

        constexpr ~Stack() noexcept
        {
            destroy_elems(this);
            dealloc_data(this);
        }

        constexpr Stack& operator=(const Stack& other)
            requires (std::is_copy_assignable_v<T> || std::is_copy_constructible_v<T>)
        {
            if(this->capacity >= other.size)
            {
                //If the elems are copy constructible we can save potential allocations of elems
                // by using copy assignment instead (for example with Stack<Stack<>...>)
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

        constexpr Stack& operator=(Stack&& other) noexcept
        {
            swap(this, &other);
            return *this;
        }

        //this is pretty pointless since its impossible to create a by byte copy...
        func operator<=>(const Stack&) const noexcept = default;
        constexpr bool operator==(const Stack&) const noexcept = default;

        proc alloc() noexcept -> Alloc& {return *cast(Alloc*)this; }
        proc alloc() const noexcept -> Alloc const& {return *cast(Alloc*)this; }

        static proc is_static_alloced(const Stack& vec) noexcept -> bool 
        { 
            return vec.capacity == static_capacity && has_static_storage; 
        }

        static proc swap(Stack* left, Stack* right) noexcept -> void
        {
            constexpr let transfer_static = [](Stack* from, Stack* to, Size from_i, Size to_i)
            {
                for (Size i = from_i; i < to_i; i++)
                {
                    std::construct_at(to->static_data() + i, std::move(from->static_data()[i]));
                    from->static_data()[i].~T();
                }
            };

            constexpr let transfer_half_static = [=](Stack* from, Stack* to)
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
                    std::swap((*left)[i], (*right)[i]);
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
                std::swap(left->alloc(), right->alloc());
        }

        static proc alloc_data(Stack* vec, Size capacity) -> void
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
            vec->data = vec->allocate(sizeof(T) * vec->capacity);
        }

        static proc dealloc_data(Stack* vec) -> void
        {
            if(vec->capacity > static_capacity)
                vec->deallocate(vec->data, sizeof(T) * vec->capacity);
        }

        //can be used for arbitrary growing/shrinking of data
        // when called on uninit stack acts as alloc_data
        // when called with new_capacity = 0 acts as dealloc_data
        // Destroys elements when shrinking but does not construct new ones when growing 
        //  (because doesnt know how to)
        static proc set_capacity(Stack* vec, Size new_capacity) -> void
        {
            T* new_data = nullptr;

            if(new_capacity <= static_capacity)
            {
                new_data = vec->static_data();
                new_capacity = vec->static_capacity;
            }
            else
                new_data = vec->allocate(sizeof(T) * new_capacity);

            let copy_to = std::min(vec->size, new_capacity);
            for (Size i = 0; i < copy_to; i++)
                std::construct_at(new_data + i, std::move(vec->data[i]));

            destroy_elems(vec);
            dealloc_data(vec);

            vec->data = new_data;
            vec->capacity = new_capacity;
        }

        static proc destroy_elems(Stack* vec) -> void
        {
            for (Size i = 0; i < vec->size; i++)
                vec->data[i].~T();
        }

        static proc copy_construct_elems(Stack* vec, const Stack& other) -> void
        {
            for (Size i = 0; i < other.size; i++)
                std::construct_at(vec->data + i, other.data[i]);
        }

        #include "slice_op_text.h"
    };

    template<typename Stack>
    concept Stack_Concept = requires(Stack stack)
    {
        Stack::set_capacity(&stack, 0);
        Stack::destroy_elems(&stack);

        stack.data = nullptr; 
        stack.size = 0; 
        stack.capacity = 0; 

        { Stack::allocator_type    };
        { Stack::slice_type        };
        { Stack::const_slice_type  };
        { Stack::grow_type         };
        { Stack::size_type         };
        //{ Stack::Stack_Data        };
        { stack.static_capacity    } -> std::convertible_to<size_t>;
        { stack.has_static_storage } -> std::convertible_to<bool>;
        { stack.has_stateful_alloc } -> std::convertible_to<bool>; 
    };

    static_assert(Stack_Concept<Stack<i32, 0>> == true);
    static_assert(Stack_Concept<Stack<i32, 2>> == true);
    static_assert(Stack_Concept<i32> == false);
}


#define STACK_TEMPL class T, size_t scap, class Size, class Alloc, class Grow
#define Stack_T Stack<T, scap, Size, Alloc, Grow>

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
    namespace stack
    {
        template <STACK_TEMPL>
        proc realloc(Stack_T* stack, Size cap)
        {
            stack->set_capacity(cap);
        }
    }

    template <STACK_TEMPL>
    proc reserve(Stack_T* stack, Size to_fit) -> bool
    {
        if (stack->capacity >= to_fit)
            return false;

        Size realloc_to = 0;
        //@NOTE: The below line could be merged with the same check in set_capacity
        // (ie. there could be a version of set_capacity that doesnt have this check)
        // but the improvemnt is so minor I have excluded it
        if (to_fit <= stack->static_capacity)
            realloc_to = stack->static_capacity;
        else
            realloc_to = cast(Size) Stack_T::grow_type::run(
                cast(size_t) to_fit, 
                cast(size_t) stack->capacity, 
                cast(size_t) stack->size, 
                cast(size_t) stack->static_capacity,
                sizeof(T)
            );

        assert(realloc_to > to_fit);
        Stack_T::set_capacity(stack, realloc_to);
        return true;
    }

    template <STACK_TEMPL>
    proc clear(Stack_T* stack)
    {
        Stack_T::destroy_elems(stack);
        stack.size = 0;
    }

    template <STACK_TEMPL>
    func empty(Stack_T const& stack) noexcept
    {
        return stack->size == 0;
    }

    template <STACK_TEMPL>
    func is_empty(Stack_T const& stack) noexcept
    {
        return stack->size == 0;
    }

    template <STACK_TEMPL>
    func is_static_alloced(Stack_T const& stack) noexcept
    {
        return Stack_T::is_static_alloced(stack);
    }

    template <STACK_TEMPL>
    proc push(Stack_T* stack, T what) -> T*
    {
        reserve(stack, stack->size + 1);
        T* ptr = stack->data + stack->size;
        std::construct_at(ptr, std::move(what));
        stack->size++;
        return stack->data + stack->size - 1;
    }

    template <STACK_TEMPL>
    proc back(Stack_T* stack) -> T&
    {
        assert(stack->size > 0);
        return stack->data[stack->size - 1];
    }

    template <STACK_TEMPL>
    proc back(Stack_T const& stack) -> T const&
    {
        assert(stack.size > 0);
        return stack.data[stack.size - 1];
    }

    template <STACK_TEMPL>
    proc front(Stack_T* stack) -> T&
    {
        assert(stack->size > 0);
        return stack->data[0];
    }

    template <STACK_TEMPL>
    proc front(Stack_T const& stack) -> T const&
    {
        assert(stack.size > 0);
        return stack.data[0];
    }

    template <STACK_TEMPL>
    proc pop(Stack_T* stack) -> T
    {
        assert(stack->size != 0);

        stack->size--;

        T ret = std::move(stack->data[stack->size]);
        stack->data[stack->size].~T();
        return ret;
    }

    template <STACK_TEMPL>
    proc resize(Stack_T* stack, Size to, T fillWith = T())
    {
        reserve(stack, to);
        for (Size i = stack->size; i < to; i++)
            std::construct_at(stack->data + i, fillWith);

        for (Size i = to; i < stack->size; i++)
            stack->data[i].~T();
        stack->size = to;
    }

    template <STACK_TEMPL>
    proc remove(Stack_T* stack, Size at) -> T
    {
        assert(at < stack->size);

        T removed = std::move(stack->data[at]);

        for (Size i = at; i < stack->size; i++)
            stack->data[i] = std::move(stack->data[i + 1]);

        pop(stack);
        return removed;
    }

    template <STACK_TEMPL>
    proc insert(Stack_T* stack, Size at, T what) -> T*
    {
        assert(at < stack->size);
        reserve(stack, stack->size + 1);

        for (Size i = stack->size - 1; i > at; i--)
            stack->data[i + 1] = std::move(stack->data[i]);

        stack->data[at] = std::move(what);
        return stack->data + at;
    }

    template <STACK_TEMPL>
    proc unordered_remove(Stack_T* stack, Size at) -> T
    {
        std::swap(stack->data[at], back(stack));
        return pop(stack);
    }

    template <STACK_TEMPL>
    proc unordered_insert(Stack_T* stack, Size at, T what) -> T*
    {
        push(stack, what);
        std::swap(stack->data[at], back(stack));
        return stack->data + at;
    }

    #undef STACK_TEMPL
    #undef Stack_T
}


#include "undefs.h"