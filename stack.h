#pragma once

#include <memory>

#include "utils.h"
#include "slice.h"
#include "defines.h"

namespace jot
{
    namespace detail
    {
        template<typename Alloc>
        struct Alloc_Data {Alloc allocator = Alloc();};

        template<typename T, typename Size, size_t static_capacity_, class Alloc>
        struct Stack_Data : Alloc
        {
            static constexpr Size static_capacity = cast(Size) static_capacity_;

            T* data = nullptr;
            Size size = 0;
            Size capacity = static_capacity;
            //Alloc allocator = Alloc();

            T static_data[static_capacity];

            func operator<=>(const Stack_Data&) const noexcept = default;
        };

        template<typename T, typename Size, class Alloc>
        struct Stack_Data<T, Size, 0, Alloc> : Alloc
        {
            static constexpr Size static_capacity = 0;

            T* data = nullptr;
            Size size = 0;
            Size capacity = static_capacity;
            //Alloc allocator = Alloc();

            static constexpr T static_data[1] = {T()};

            func operator<=>(const Stack_Data&) const noexcept = default;
        };

        func calc_static_size(size_t def_size, size_t reserved, size_t elem_size) -> size_t
        {
            if(def_size < reserved)
                return 0;

            size_t remaining = def_size - reserved;
            return remaining - remaining % elem_size;
        }

    }

    static constexpr size_t STACK_DEFAULT_TOTAL_BYTE_SIZE = 0;

    template <class T, class Size, class Alloc>
    static constexpr size_t STACK_STATIC_BYTES = 
        detail::calc_static_size(STACK_DEFAULT_TOTAL_BYTE_SIZE, sizeof(detail::Stack_Data<T, Size, 0, Alloc>), sizeof(T));

    template <class T>
    using Def_Allocator = std::allocator<T>;

    template <typename T, typename Size = size_t, typename Alloc = Def_Allocator<T>, size_t static_capacity = STACK_STATIC_BYTES<T, Size, Alloc>>
    struct Stack : detail::Stack_Data<T, Size, static_capacity, Alloc>
    {
        using allocator_type  = Alloc;
        using slice_type      = Slice<T, Size>;
        using const_slice_type= Slice<const T, Size>;

        constexpr static bool has_static_storage = static_capacity != 0;
        constexpr static bool has_stateful_alloc = !std::is_empty_v<Alloc>;

        Stack() = default;
        Stack(Stack&& other) noexcept { swap(other); }
        Stack(Size size) { alloc_self(size); }

        Stack(const Stack& other)
        {
            alloc_self(other.size);
            this->size = other.size;

            for (Size i = 0; i < this->size; i++)
                new (this->data + i) T(other.data[i]);
        }

        ~Stack() noexcept
        {
            destroy_elems();

            if (this->data != nullptr)
                dealloc_self();
        }

        Stack& operator=(const Stack& vec)
        {
            if(this->size > vec.size)
            {
                destroy_elems();
                
                this->size = vec.size;
                for (Size i = 0; i < this->size; i++)
                    new (this->data + i) T(vec.data[i]);
            }
            else
            {
                Stack copy(vec);
                swap(copy);
            }

            return *this;
        }

        Stack& operator=(Stack&& vec)
        {
            swap(vec);
            return *this;
        }

        func operator<=>(const Stack&) const noexcept = default;
        constexpr bool operator==(const Stack&) const noexcept = default;

        void swap(Stack& other) noexcept
        {
            if(this->capacity <= static_capacity)
            {
                //Litle optim for large static arrays/little arrays and loop unrolling
                if constexpr (static_capacity * sizeof(T) > 64)
                {
                    let smaller_cap = std::min(this->capacity, other.capacity);
                    for(Size i = 0; i < smaller_cap; i++)
                        std::swap((*this)[i], other[i]);
                }
                else
                {
                    std::swap(this->static_data, other.static_data);
                }
            }
            else
                std::swap(this->data, other.data);

            std::swap(this->size, other.size);
            std::swap(this->capacity, other.capacity);

            if constexpr(has_stateful_alloc)
                std::swap(*(Alloc*)this, *(Alloc*)&other);
        }

        void alloc_self(Size capacity)
        {
            if(capacity <= static_capacity)
            {
                if constexpr (has_static_storage)
                    this->data = this->static_data;
                return;
            }

            this->data = this->allocate(sizeof(T) * this->capacity);
            this->capacity = capacity;
        }

        void dealloc_self()
        {
            if(this->capacity > static_capacity)
                this->deallocate(this->data, sizeof(T) * this->capacity);
        }

        bool realloc_self(Size capacity)
        {
            if(capacity <= static_capacity)
                return;

            mut new_data = this->allocate(sizeof(T) * this->capacity);

            for (Size i = 0; i < this->size; i++)
                new (new_data + i) T(std::move(this->data[i]));

            dealloc_self();
            this->data = new_data;
            this->capacity = capacity;
        }

        void destroy_elems()
        {
            for (Size i = 0; i < this->size; i++)
                this->data[i].~T();
        }

        #include "span_array_shared_text.h"
    };
//}
//
//
//namespace std 
//{
//    template <STACK_TEMPL>
//    proc swap(StackT& stack1, Stack<T, Size, Alloc, scap>& stack2)
//    {
//        return stack1.swap(stack2);
//    }
//}
//
//namespace jot
//{
    #define STACK_TEMPL class T, class Size, class Alloc, size_t scap
    #define StackT Stack<T, Size, Alloc, scap>

    template <STACK_TEMPL>
    proc reserve(StackT* stack, Size toFit)
    {
        if (stack->capacity >= toFit)
            return false;
        if (stack->capacity == 0)
            stack->realloc_self(toFit);
        else
        {
            Size reallocTo = stack->capacity;
            while(reallocTo < toFit)
                reallocTo *= 2;

            stack->realloc_self(reallocTo);
        }
        return true;
    }

    template <STACK_TEMPL>
    proc clear(StackT* stack)
    {
        stack->destroy_elems();
        stack.size = 0;
    }

    template <STACK_TEMPL>
    func empty(StackT const& stack) noexcept
    {
        return stack->size == 0;
    }

    template <STACK_TEMPL>
    func is_empty(StackT const& stack) noexcept
    {
        return stack->size == 0;
    }

    template <STACK_TEMPL>
    proc push(StackT* stack, T what) -> T*
    {
        reserve(stack, stack->size + 1);
        new (stack->data + stack->size) T(std::move(what));
        stack->size++;
        return stack->data + stack->size - 1;
    }

    template <STACK_TEMPL>
    proc back(StackT* stack) -> T&
    {
        assert(stack->size > 0);
        return stack->data[stack->size - 1];
    }

    template <STACK_TEMPL>
    proc back(StackT const& stack) -> T const&
    {
        assert(stack.size > 0);
        return stack.data[stack.size - 1];
    }

    template <STACK_TEMPL>
    proc front(StackT* stack) -> T&
    {
        assert(stack->size > 0);
        return stack->data[0];
    }

    template <STACK_TEMPL>
    proc front(StackT const& stack) -> T const&
    {
        assert(stack.size > 0);
        return stack.data[0];
    }

    template <STACK_TEMPL>
    proc pop(StackT* stack) -> T
    {
        assert(stack->size != 0);

        stack->size--;

        T ret = std::move(this->data[stack->size]);
        this->data[stack->size].~T();
        return ret;
    }

    template <STACK_TEMPL>
    proc resize(StackT* stack, Size to, T fillWith = T())
    {
        reserve(stack, to);
        for (Size i = stack->size; i < to; i++)
            new (stack->data + i) T(fillWith);

        for (Size i = to; i < stack->size; i++)
            stack->data[i].~T();
        stack->size = to;
    }

    template <STACK_TEMPL>
    proc remove(StackT* stack, Size at) -> T
    {
        assert(at < stack->size);

        T removed = std::move(stack->data[at]);

        for (Size i = at; i < stack->size; i++)
            stack->data[i] = std::move(stack->data[i + 1]);

        pop(stack);
        return removed;
    }

    template <STACK_TEMPL>
    proc insert(StackT* stack, Size at, T what) -> T*
    {
        assert(at < stack->size);
        reserve(stack, stack->size + 1);

        for (Size i = stack->size - 1; i > at; i--)
            stack->data[i + 1] = std::move(stack->data[i]);

        stack->data[at] = std::move(what);
        return stack->data + at;
    }

    template <STACK_TEMPL>
    proc unordered_remove(StackT* stack, Size at) -> T
    {
        std::swap(stack->data[at], back(stack));
        return pop(stack);
    }

    template <STACK_TEMPL>
    proc unordered_insert(StackT* stack, Size at, T what) -> T*
    {
        push(stack, what);
        std::swap(stack->data[at], back(stack));
        return stack->data + at;
    }

    #undef STACK_TEMPL
    #undef StackT
}


#include "undefs.h"