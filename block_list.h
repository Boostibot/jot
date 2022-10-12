#pragma once
#include <memory>


#include "jot/utils.h"
#include "jot/slice.h"
#include "jot/array.h"
#include "jot/defines.h"

namespace jot 
{
    template <typename T, std::integral Size>
    struct Block 
    {
        Block* next = nullptr;
        Block* prev = nullptr;
        Size size = 0;
    };

    template <typename T, std::integral Size>
    proc data(Block<T, Size>* block)
    {
        byte* bytes = cast(byte*) cast(void*) block;
        return cast(T*) cast(void*) (bytes + sizeof(Block<T, Size>));
    }

    constexpr size_t BLOCK_ALIGN = 16;

    template <typename T, std::integral Size, typename Allocator>
    proc allocate_block(Size item_count, Allocator* alloc) -> Block<T, Size>*
    {
        using Block = Block<T, Size>;

        Block* block = cast(Block*) cast(void*) allocate<byte>(alloc, sizeof(Block) + sizeof(T) * item_count, DEF_ALIGNMENT<Block>);
        *block = Block{
            .size = item_count
        };
        return block;
    }

    template <typename T, std::integral Size = Def_Size>
    struct Block_List_View_
    {
        using Block = Block<T, Size>;

        Block* first = nullptr;
        Block* last = nullptr;
        Size item_size = 0;
        Size size = 0;
    };

    template <typename T, std::integral Size = Def_Size, allocator Alloc = std::allocator<T>>
    struct Block_List_ : Alloc
    {
        using Block = Block<T, Size>;

        Block* first = nullptr;
        Block* last = nullptr;
        Size item_size = 0;
        Size size = 0;

        template <stdr::forward_range Inserted>
        explicit Block_List_(Inserted&& inserted, Alloc alloc = Alloc())
            : Alloc(std::move(alloc)), first(), last(), item_size()
        {
            unsafe_init(std::forward<Inserted>(inserted));
        }


        explicit Block_List_(Size size, Alloc alloc = Alloc())
            requires std::is_default_constructible_v<T>
            : Alloc(std::move(alloc)), first(), last(), item_size()
        {
            unsafe_init(size);

            mut* block_data = data(this->first);
            if constexpr (std::is_fundamental_v<T>)
                memset(block_data, 0, this->item_size * sizeof(T));
            else
                for(Size i = 0; i < this->item_size; i++)
                    block_data[i] = T();
        }

        Block_List_() noexcept = default;
        Block_List_(Alloc allocator) noexcept 
            : Alloc(allocator), first(), last(), item_size(), size() {}
        Block_List_(Block_List_ && other) noexcept
        {
            swap(&other);
        }

        Block_List_& operator=(Block_List_&& other) noexcept
        {
            swap(&other);
            return *this;
        }


        ~Block_List_() noexcept
        {
            assert(is_invariant());

            Size passed_size = 0;
            Block* current = this->first;
            if(current == nullptr)
            {
                assert(current == this->last && "broken node chain");
                assert(passed_size == this->item_size);
                return;
            }

            while(true)
            {
                passed_size += current->size;
                T* items = data(current);
                for(Size i = 0; i < current->size; i++)
                    items[i].~T();


                
                Block* next = current->next;
                deallocate<byte>(allocator(), cast(byte*) cast(void*) current, current->size * sizeof(T), DEF_ALIGNMENT<Block>);
                if(next == nullptr)
                    break;

                current = next;
            }

            assert(passed_size == this->item_size && "must deinit all items");
            assert(current == last && "broken node chain");
        }

        proc allocator() noexcept -> Alloc* {return cast(Alloc*)this; }
        proc allocator() const noexcept -> Alloc const* {return cast(Alloc*)this; }

        template <stdr::forward_range Inserted>
        proc unsafe_init(Inserted&& inserted) -> void
        {
            const Size size = cast(Size) std::size(inserted);
            unsafe_init(size);
            Block* block = this->first;

            mut it = stdr::begin(inserted);
            mut end = stdr::end(inserted);
            mut data_ptr = data(block);

            for(; it != end; ++it, ++data_ptr)
            {
                if constexpr(std::is_rvalue_reference_v<decltype(inserted)>)
                    *data_ptr = std::move(*it);
                else
                    *data_ptr = *it;
            }       
        }

        proc unsafe_init(Size item_size) -> void
        {
            Block* block = allocate_block<T>(item_size, allocator());

            this->first = block;
            this->last = block;
            this->size = 1;
            this->item_size = block->size;
        }

        void swap(Block_List_* other) noexcept 
        {
            std::swap(this->first, other->first);
            std::swap(this->last, other->last);
            std::swap(this->item_size, other->item_size);
        }

        operator Block_List_View_<T, Size>() const noexcept {
            return Block_List_View_<T, Size>{
                .first = first,
                .last = last,
                .item_size = item_size,
                .size = size,
            };
        }

        func is_invariant() const
        {
            bool all_active = first != nullptr && last != nullptr && first->prev == nullptr && last->next == nullptr;
            bool all_inactive = first == nullptr && last == nullptr;

            return (all_active || all_inactive && "all fields must either be set or unset");
        }

        using value_type      = T;
        using size_type       = Size;
        using difference_type = ptrdiff_t;
        using pointer         = T*;
        using const_pointer   = const T*;
        using reference       = T&;
        using const_reference = const T&;

        template <typename SubT>
        struct Iter
        {
            using iterator_category = std::bidirectional_iterator_tag;
            using Block           = typename Block_List_::Block;
            using value_type      = Block;
            using difference_type = typename Block_List_::difference_type;
            using pointer         = Block*;
            using reference       = value_type&;

            Block* block = nullptr; 

            proc operator++() noexcept -> Iter&
            {
                assert(block != nullptr);
                block = block->next;
                return *this;
            }
            proc operator++(int) noexcept -> Iter
            {
                mut copy = *this;
                assert(block != nullptr);
                block = block->next;
                return copy;
            }
            proc operator--() noexcept -> Iter&
            {
                assert(block != nullptr);
                block = block->prev;
                return *this;
            }
            proc operator--(int) noexcept -> Iter
            {
                mut copy = *this;
                assert(block != nullptr);
                block = block->prev;
                return copy;
            }

            func operator*() const -> reference
            {
                assert(block != nullptr);
                return *block;
            }
            func operator->() const -> pointer       
            { 
                assert(block != nullptr);
                return *block;
            }

            friend proc swap(Iter& left, Iter& right) -> void
            {
                std::swap(left.block, right.block);
            }

            constexpr bool operator==(Iter const&) const = default;
            constexpr bool operator!=(Iter const&) const = default;
        };

        using iterator       = Iter<T>;
        using const_iterator = Iter<const T>;

        using reverse_iterator       = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        func begin() noexcept -> iterator{
            return iterator{.block = this->first};
        }

        func begin() const noexcept -> const_iterator{
            return const_iterator{.block = this->first};
        }

        func end() noexcept -> iterator{
            return iterator{.block = nullptr};
        }

        func end() const noexcept -> const_iterator{
            return const_iterator{.block = nullptr};
        }

        static_assert(std::input_iterator<iterator>, "failed input iterator");
        static_assert(std::output_iterator<iterator, Block>, "failed output iterator");
        static_assert(std::forward_iterator<iterator>, "failed forward iterator");
        static_assert(std::input_iterator<iterator>, "failed input iterator");
        static_assert(std::bidirectional_iterator<iterator>, "failed bidirectional iterator");
    };


    #define BLOCK_LIST_TEMPL typename T, std::integral Size, typename Alloc
    #define BLOCK_TEMPL typename T, std::integral Size
    #define Block_T Block<T, Size>
    #define Block_List_T Block_List_<T, Size, Alloc>

    enum class Iter_Direction 
    {
        FORWARD,
        BACKWARD
    };

    namespace detail 
    {
        //is used to be able to specialize for both const and not const
        template <typename Block_, std::integral Size_>
        func block_at(Block_* from, Size_ block_offset, Iter_Direction direction) -> Block_*
        {
            mut* current = from;
            while(true)
            {
                assert(current != nullptr && "Block index must be in range");
                if(block_offset == 0)
                    break;

                if (direction == Iter_Direction::FORWARD)
                    current = current->next;
                else
                    current = current->prev;

                block_offset --;
            }

            return current;
        }

        template <typename T, std::integral Size>
        func slice_range(Block<T, Size>* from, Size block_count, Iter_Direction direction) -> Block_List_View_<T, Size>
        {
            mut* current = from;
            Size passed_size = 0;
            Size i = 0;

            if(block_count == 0)
                return Block_List_View_<T, Size>{};

            assert(current != nullptr && "Block index must be in range");
            while(true)
            {
                i++;
                passed_size += current->size;
                if(i == block_count)
                    break;

                if (direction == Iter_Direction::FORWARD)
                    current = current->next;
                else
                    current = current->prev;

                assert(current != nullptr && "Block index must be in range");
            }

            return Block_List_View_<T, Size>{
                .first = from,
                .last = current,
                .item_size = passed_size,
                .size = block_count,
            };
        }

        template <typename Block, std::integral Size>
        struct At_Result 
        {
            Block* block;
            Size index;
        };

        template <typename Block_, std::integral Size_>
        func block_and_item_at(Block_* from, Size_ item_index, Iter_Direction direction) -> At_Result<Block_, Size_>
        {
            i64 signed_index = item_index;
            mut* current = from;
            while(true)
            {
                assert(current != nullptr && "Block index must be in range");
                item_index -= current->size;
                if(signed_index < 0)
                {
                    signed_index += current->size;
                    return At_Result<Block_, Size_>{
                        .block = from,
                        .index = cast(Size_)(signed_index),
                    };
                }

                if (direction == Iter_Direction::FORWARD)
                    current = current->next;
                else
                    current = current->prev;
            }
        }

        template <typename Block_, std::integral Size_>
        func item_at(Block_* from, Size_ item_index) -> Block_*
        {
            mut found = block_and_item_at(from, item_index);
            return data(found.block) + found.index;
        }

        template <typename Block_>
        func link(Block_* before, Block_* first_inserted, Block_* last_inserted, Block_* after) -> void
        {
            if(before != nullptr)
                before->next = first_inserted;

            if(after != nullptr)
                after->prev = last_inserted;
            first_inserted->prev = before;
            last_inserted->next = after;
        }

        template <typename Block_>
        func unlink(Block_* before, Block_* first_removed, Block_* last_removed, Block_* after) -> void
        {
            if(before != nullptr)
                before->next = after;

            if(after != nullptr)
                after->prev = before;
            last_removed->next = nullptr;
            first_removed->prev = nullptr;
        }
    }

    template <BLOCK_TEMPL, std::integral Size_>
        requires std::convertible_to<Size_, Size>
    func block_at(Block_T* from, Size_ block_index, Iter_Direction dir = Iter_Direction::FORWARD) -> Block_T*
    {
        return detail::block_at(from, block_index);
    }

    template <BLOCK_TEMPL, std::integral Size_>
        requires std::convertible_to<Size_, Size>
    func block_at(Block_T const& from, Size_ block_index, Iter_Direction dir = Iter_Direction::FORWARD) -> Block_T
    {
        return *detail::block_at(&from, block_index);
    }

    template <BLOCK_TEMPL, std::integral Size_>
        requires std::convertible_to<Size_, Size>
    func item_at(Block_T* from, Size_ item_index, Iter_Direction dir = Iter_Direction::FORWARD) -> T*
    {
        return detail::item_at(from, item_index);
    }

    template <BLOCK_TEMPL, std::integral Size_>
        requires std::convertible_to<Size_, Size>
    func item_at(Block_T const& from, Size_ item_index, Iter_Direction dir = Iter_Direction::FORWARD) -> T
    {
        return *detail::item_at(&from, item_index);
    }

    template <BLOCK_LIST_TEMPL, std::integral Size_>
        requires std::convertible_to<Size_, Size>
    func block_at(Block_List_T* list, Size_ block_index, Iter_Direction dir = Iter_Direction::FORWARD) -> Block_T*
    {
        if(dir == Iter_Direction::FORWARD)
            return block_at(list->first, block_index, dir);
        else
            return block_at(list->last, block_index, dir);
    }

    template <BLOCK_LIST_TEMPL, std::integral Size_>
        requires std::convertible_to<Size_, Size>
    func block_at(Block_List_T const& list, Size_ block_index, Iter_Direction dir = Iter_Direction::FORWARD) -> Block_T
    {
        if(dir == Iter_Direction::FORWARD)
            return block_at(*list.first, block_index, dir);
        else
            return block_at(*list.last, block_index, dir);
    }

    template <BLOCK_LIST_TEMPL, std::integral Size_>
        requires std::convertible_to<Size_, Size>
    func item_at(Block_List_T* list, Size_ item_index, Iter_Direction dir = Iter_Direction::FORWARD) -> T*
    {
        if(dir == Iter_Direction::FORWARD)
            return item_at(list->first, item_index, dir);
        else
            return item_at(list->last, item_index, dir);
    }

    template <BLOCK_LIST_TEMPL, std::integral Size_>
        requires std::convertible_to<Size_, Size>
    func item_at(Block_List_T const& list, Size_ item_index, Iter_Direction dir = Iter_Direction::FORWARD) -> T
    {
        if(dir == Iter_Direction::FORWARD)
            return item_at(*list.first, item_index, dir);
        else
            return item_at(*list.last, item_index, dir);
    }

    template <BLOCK_LIST_TEMPL>
    func is_invariant(Block_List_T const& block_list) -> bool
    {
        return block_list.is_invariant();
    }

    template <BLOCK_LIST_TEMPL>
    proc push_back(Block_List_T* block_list, Block_List_T&& inserted) -> Block_List_View_<T, Size>
    {
        assert(is_invariant(*block_list));
        
        Block_List_View_<T, Size> ret = inserted;
        detail::link<Block_T>(block_list->last, inserted.first, inserted.last, nullptr);

        if(block_list->first == nullptr)
            block_list->first = inserted.first;

        block_list->last = inserted.last;
        block_list->size += inserted.size;
        block_list->item_size += inserted.item_size;

        inserted.first = nullptr;
        inserted.last = nullptr;
        inserted.size = 0;
        inserted.item_size = 0;

        assert(is_invariant(*block_list));
        return ret;
    }

    //@TODO: this too complex seek to find a shorter alternative


    template <BLOCK_LIST_TEMPL, stdr::forward_range Inserted>
    proc push_back(Block_List_T* block_list, Inserted&& inserted) -> Block_List_View_<T, Size>
    {
        return push_back(block_list, Block_List_T{std::forward<Inserted>(inserted)});
    }

    template <BLOCK_LIST_TEMPL, typename T_>
        requires std::convertible_to<T_, T>
    proc push_back(Block_List_T* block_list, T_ inserted) -> Block_List_View_<T, Size>
    {
        return push_back(block_list, Block_List_T{Array_<T, 1>{inserted}});
    }

    template <BLOCK_LIST_TEMPL>
    proc push_back(Block_List_T* block_list, T&& inserted) -> Block_List_View_<T, Size>
    {
        return push_back(block_list, Block_List_T{Slice_<T>{&inserted, 1}});
    }


    template <BLOCK_LIST_TEMPL>
    proc push_front(Block_List_T* block_list, Block_List_T&& inserted) -> Block_List_View_<T, Size>
    {
        assert(is_invariant(*block_list));

        Block_List_View_<T, Size> ret = inserted;
        detail::link<Block_T>(nullptr, inserted.first, inserted.last, block_list->first);

        if(block_list->last == nullptr)
            block_list->last = inserted.last;

        block_list->first = inserted.first;
        block_list->size += inserted.size;
        block_list->item_size += inserted.item_size;

        inserted.last = nullptr;
        inserted.first = nullptr;
        inserted.size = 0;
        inserted.item_size = 0;

        assert(is_invariant(*block_list));
        return ret;
    }

    template <BLOCK_LIST_TEMPL, stdr::forward_range Inserted>
    proc push_front(Block_List_T* block_list, Inserted&& inserted) -> Block_List_View_<T, Size>
    {
        return push_front(block_list, Block_List_T{std::forward<Inserted>(inserted)});
    }

    template <BLOCK_LIST_TEMPL, typename T_>
        requires std::convertible_to<T_, T>
    proc push_front(Block_List_T* block_list, T_ inserted) -> Block_List_View_<T, Size>
    {
        return push_front(block_list, Block_List_T{Array_<T, 1>{inserted}});
    }

    template <BLOCK_LIST_TEMPL>
    proc push_front(Block_List_T* block_list, T&& inserted) -> Block_List_View_<T, Size>
    {
        return push_front(block_list, Block_List_T{Slice_<T>{&inserted, 1}});
    }

    template <BLOCK_LIST_TEMPL>
    func unsafe_to_block_list(Block_List_View_<T, Size> const& view, Alloc const& alloc) -> Block_List_T
    {
        Block_List_T made{alloc};
        made.first = view.first;
        made.last = view.last;
        made.size = view.size;
        made.item_size = view.item_size;
        return made;
    }

    template <BLOCK_LIST_TEMPL, std::integral Size_ = int>
        requires std::convertible_to<Size_, Size>
    proc pop_back(Block_List_T* block_list, Size_ count = 1) -> Block_List_T
    {
        assert(is_invariant(*block_list));

        let slice = detail::slice_range(block_list->last, cast(Size) count, Iter_Direction::BACKWARD);
        mut popped = unsafe_to_block_list(slice, *block_list->allocator());

        block_list->last = slice.first->prev;
        block_list->size -= popped.size;
        block_list->item_size -= popped.item_size;

        detail::unlink<Block_T>(block_list->last, popped.first, popped.last, nullptr);
        if(block_list->last == nullptr)
            block_list->first = nullptr;

        assert(is_invariant(*block_list));
        assert(is_invariant(popped));

        return popped;
    }

    template <BLOCK_LIST_TEMPL, std::integral Size_ = int>
        requires std::convertible_to<Size_, Size>
    proc pop_front(Block_List_T* block_list, Size_ count = 1) -> Block_List_T
    {
        assert(is_invariant(*block_list));
        assert(block_list->first != nullptr && "cannot pop empty list");

        let slice = detail::slice_range(block_list->first, cast(Size) count, Iter_Direction::FORWARD);
        mut popped = unsafe_to_block_list(slice, *block_list->allocator());

        block_list->first = block_list->first->next;
        block_list->size -= popped.size;
        block_list->item_size -= popped.item_size;

        detail::unlink<Block_T>(nullptr, popped.first, popped.last, block_list->first);
        if(block_list->first == nullptr)
            block_list->last = nullptr;

        assert(is_invariant(*block_list));
        assert(is_invariant(popped));

        return popped;
    }

    //this 4 replication is insane...
    namespace {};

    template <BLOCK_LIST_TEMPL>
    proc push(Block_List_T* block_list, Block_List_T&& inserted) -> Block_List_View_<T, Size>
    {
        return push_back(block_list, std::forward<Block_List_T>(inserted));
    }

    template <BLOCK_LIST_TEMPL, stdr::forward_range Inserted>
    proc push(Block_List_T* block_list, Inserted&& inserted) -> Block_List_View_<T, Size>
    {
        return push_back(block_list, std::forward<Inserted>(inserted));
    }
    
    template <BLOCK_LIST_TEMPL, typename T_>
        requires std::convertible_to<T_, T>
    proc push(Block_List_T* block_list, T_ inserted) -> Block_List_View_<T, Size>
    {
        return push_back(block_list, std::forward<T_>(inserted));
    }

    template <BLOCK_LIST_TEMPL>
    proc push(Block_List_T* block_list, T&& inserted) -> Block_List_View_<T, Size>
    {
        return push_back(block_list, std::forward<T>(inserted));
    }


    template <BLOCK_LIST_TEMPL>
    func unsafe_to_block_list(Block_T* block, Alloc const& alloc) -> Block_List_T
    {
        detail::unlink(block->prev, block, block, block->next);
        Block_List_T made{alloc};
        made.first = block;
        made.last = block;
        made.size = 1;
        made.item_size = block->size;

        return made;
    }

    template <BLOCK_LIST_TEMPL>
    proc pop_block(Block_List_T* block_list, Block_T* at) -> Block_List_T
    {
        if(at == block_list->first)
            return pop_front(block_list);
        if(at == block_list->last)
            return pop_back(block_list);
        
        return unsafe_to_block_list(at, *block_list->allocator());
    }

    template <BLOCK_LIST_TEMPL, std::integral Size_ = int>
        requires std::convertible_to<Size_, Size>
    proc pop(Block_List_T* block_list, Size_ count = 1) -> void
    {
        return pop_back(block_list, count);
    }

    template <BLOCK_LIST_TEMPL>
    func is_empty(Block_List_T const& list) -> bool 
    {
        return list.size == 0;
    }

    template <BLOCK_LIST_TEMPL>
    func empty(Block_List_T const& list) -> bool 
    {
        return list.size == 0;
    }

    #undef BLOCK_LIST_TEMPL
    #undef BLOCK_TEMPL 
    #undef Block_T 
    #undef Block_List_T

    void tester_block_list()
    {
        //Block_List_<byte> buffer;
        //push_block(allocate_block());

        //push(&buffer, [&]{return "hello";});
    }
}

namespace std
{
    template <typename T, std::integral Size, typename Alloc>
    func size(jot::Block_List_<T, Size, Alloc> const& list) noexcept {return list.size;}

    template <typename T, std::integral Size, typename Alloc>
    func begin(jot::Block_List_<T, Size, Alloc>& list) noexcept {return list.begin();}
    template <typename T, std::integral Size, typename Alloc>
    func begin(jot::Block_List_<T, Size, Alloc> const& list) noexcept {return list.begin();}

    template <typename T, std::integral Size, typename Alloc>
    func end(jot::Block_List_<T, Size, Alloc>& list) noexcept {return list.end();}
    template <typename T, std::integral Size, typename Alloc>
    func end(jot::Block_List_<T, Size, Alloc> const& list) noexcept {return list.end();}

    template <typename T, std::integral Size, typename Alloc>
    func cbegin(jot::Block_List_<T, Size, Alloc> const& list) noexcept {return list.begin();}
    template <typename T, std::integral Size, typename Alloc>
    func cend(jot::Block_List_<T, Size, Alloc> const& list) noexcept {return list.end();}
}

#include "jot/defer.h"