#pragma once
#include "jot/utils.h"
#include "jot/slice.h"
#include "jot/defines.h"

namespace jot 
{
    //Is an (almost) universal set of concepts from which to build linked list type containers.
    //Nodes called blocks are comprised of Header and Items (type determined by header). These blocks
    // can be: 
    // 1: All statically sized (say one item each) 
    // 2: Have variable ammount of items 
    // 3: Be all uniformly sized (but not statically - meaning the size can be set at runtime construction)
    // 4: Other
    // In addition headers can have next or prev pointers or both determining the linked-ness of the list

    //This lets us build normal single linked lists, doubly linked lists, blocks for arena allocators, c++ style dequeue and others

    //@TOOD: rebuild fillers, add push pop in default direction, add push for size and default init, readd block_at etc.


    //=== Header concepts ===
    struct List_Block_Tag {};

    namespace detail
    {
        template <typename Block_Sizer, typename Header>
        concept block_size_type_concept = requires(Block_Sizer block_size, Header header)
        {
            requires(std::is_move_constructible_v<Block_Sizer>);
            requires(std::is_default_constructible_v<Block_Sizer>);
            { block_size.block_size(header) } -> std::convertible_to<size_t>;
            { block_size.default_block_size() } -> std::convertible_to<size_t>;
        };
    }

    template <typename Header>
    concept block_header_base = std::is_default_constructible_v<Header> && requires(Header header)
    {
        requires(Tagged<Header, List_Block_Tag>);
        typename Header::value_type;
        typename Header::size_type;
        typename Header::block_sizer;

        requires(integral<typename Header::size_type>);
        requires(non_void<typename Header::value_type>);
        requires(detail::block_size_type_concept<typename Header::block_sizer, Header>);
    };

    template <typename Header>
    concept forward_block_header = block_header_base<Header> && requires(Header header)
    {
        { header.next } -> std::convertible_to<Header*>; 
    };

    template <typename Header>
    concept backward_block_header = block_header_base<Header> && requires(Header header)
    {
        { header.prev } -> std::convertible_to<Header*>; 
    };

    template <typename Header>
    concept block_header = forward_block_header<Header> || backward_block_header<Header>;

    template <typename Header>
    concept bidi_block_header = forward_block_header<Header> && backward_block_header<Header>;

    template <typename Header>
    concept sized_block_header = block_header<Header> && requires(Header header)
    {
        header.size = 0;
        { header.size } -> std::convertible_to<size_t>; 
    };

    template <typename Header>
    concept static_sized_block_header = block_header<Header> && requires(Header header)
    {
        { Header::size } -> std::convertible_to<size_t>; 
    };

    template <typename Header, size_t size>
    concept static_sized_block_header_to = block_header<Header> && requires(Header header)
    {
        { Header::size } -> std::convertible_to<size_t>; 
        requires(cast(size_t) Header::size == size);
    };

    //=== Base ops ===
    enum class Iter_Direction 
    {
        FORWARD,
        BACKWARD
    };

    template <block_header Header>
    func is_valid_direction(Iter_Direction direction)
    {
        return (direction == Iter_Direction::FORWARD && forward_block_header<Header>) ||
            (direction == Iter_Direction::BACKWARD && backward_block_header<Header>);
    }

    template <block_header Header>
    Iter_Direction DEF_DIRECTION = forward_block_header<Header> ? Iter_Direction::FORWARD : Iter_Direction::BACKWARD;

    template <block_header Header>
    func advance(Header* from, Iter_Direction direction)
    {
        assert(is_valid_direction<Header>(direction));
        if(direction == Iter_Direction::FORWARD)
        {
            if constexpr(forward_block_header<Header>)
                return from->next;
            else
                return nullptr;
        }
        else
        {
            if constexpr(backward_block_header<Header>)
                return from->next;
            else
                return nullptr;
        }
    }

    template <block_header Header>
    func advance(Header* from)
    {
        if constexpr(forward_block_header<Header>)
            return from->next;
        else
            return from->prev;
    }

    template <block_header Header>
    proc data(Header* header)
    {
        byte* bytes = cast(byte*) cast(void*) header;
        return cast(typename Header::value_type*) cast(void*) (bytes + sizeof(Header));
    }

    template <block_header Header, allocator Alloc>
    static pure proc allocate_block(Alloc* alloc, typename Header::size_type item_count) -> Header*
    {
        Header* header = cast(Header*) cast(void*) allocate<byte>(alloc, item_count * sizeof(typename Header::value_type) + sizeof(Header), DEF_ALIGNMENT<Header>);
        *header = Header{};
        if constexpr(sized_block_header<Header>)
            header->size = item_count;

        return header;
    }

    template <block_header Header, allocator Alloc>
    static pure proc allocate_block(Alloc* alloc, typename Header::block_sizer const& sizer) -> Header*
    {
        return allocate_block(alloc, sizer.default_block_size());
    }

    template <block_header Header, allocator Alloc>
    static proc deallocate_block(Alloc* alloc, Header* header, typename Header::size_type item_count) -> void
    {
        deallocate<byte>(alloc, cast(byte*) cast(void*) header, item_count * sizeof(typename Header::value_type) + sizeof(Header), DEF_ALIGNMENT<Header>);
    }

    template <block_header Header, allocator Alloc>
    static proc deallocate_block(Alloc* alloc, Header* header, typename Header::block_sizer const& sizer) -> void
    {
        return deallocate_block(alloc, header, sizer.block_size(*header));
    }

    //=== List_View ===
    template <block_header Header>
    struct List_Iterator_
    {
        using iterator_category = std::conditional_t<
            bidi_block_header<Header>,
            std::bidirectional_iterator_tag,
            std::forward_iterator_tag
        >;

        using value_type      = Header;
        using difference_type = ptrdiff_t;
        using pointer         = value_type*;
        using reference       = value_type&;

        Header* header = nullptr; 

        proc operator++() noexcept -> List_Iterator_&
        {
            assert(header != nullptr);
            header = advance(header);
            return *this;
        }
        proc operator++(int) noexcept -> List_Iterator_
        {
            mut copy = *this;
            assert(header != nullptr);
            header = advance(header);
            return copy;
        }
        proc operator--() noexcept -> List_Iterator_&
            requires bidi_block_header<Header>
        {
            assert(header != nullptr);
            header = header->prev;
            return *this;
        }
        proc operator--(int) noexcept -> List_Iterator_
            requires bidi_block_header<Header>
        {
            mut copy = *this;
            assert(header != nullptr);
            header = header->prev;
            return copy;
        }

        func operator*() const -> reference
        {
            assert(header != nullptr);
            return *header;
        }
        func operator->() const -> pointer       
        { 
            assert(header != nullptr);
            return *header;
        }

        friend proc swap(List_Iterator_& left, List_Iterator_& right) -> void
        {
            std::swap(left.header, right.header);
        }

        constexpr bool operator==(List_Iterator_ const&) const = default;
        constexpr bool operator!=(List_Iterator_ const&) const = default;
    };

    template <block_header Header>
    struct List_View_Base : Header::block_sizer
    {
        using value_type      = typename Header::value_type;
        using size_type       = typename Header::size_type;
        using difference_type = ptrdiff_t;
        using pointer         = value_type*;
        using const_pointer   = const value_type*;
        using reference       = value_type&;
        using const_reference = const value_type&;

        using iterator       = List_Iterator_<Header>;
        using const_iterator = List_Iterator_<const Header>;

        using reverse_iterator       = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        static func begin(Header* first, Header* last) noexcept -> iterator
        {
            if constexpr(forward_block_header<Header>)
                return iterator{first};
            else
                return iterator{last};
        }

        static func cbegin(Header* first, Header* last) noexcept -> const_iterator
        {
            if constexpr(forward_block_header<Header>)
                return const_iterator{last};
            else
                return const_iterator{first};
        }

        static func end(Header* first, Header* last) noexcept -> iterator             
        { 
            if constexpr(forward_block_header<Header>)
                return iterator{last->next};
            else
                return iterator{first->prev};

        }
        static func cend(Header* first, Header* last) noexcept -> const_iterator 
        { 
            if constexpr(forward_block_header<Header>)
                return const_iterator{last->next};
            else
                return const_iterator{first->prev};
        }
    };

    template <block_header Header, bool has_item_size = true>
    struct List_View_ : List_View_Base<Header>
    {
        using Base = List_View_Base<Header>;

        Header* first = nullptr;
        Header* last = nullptr;
        Header::size_type size = 0;
        Header::size_type item_size = 0;

        func begin() noexcept -> auto       { return Base::begin(this->first, this->last); }
        func begin() const noexcept -> auto { return Base::cbegin(this->first, this->last); }
        func end() noexcept -> auto         { return Base::end(this->first, this->last); }
        func end() const noexcept -> auto   { return Base::cend(this->first, this->last); }
    };

    template <block_header Header>
    struct List_View_<Header, false> : List_View_Base<Header>
    {
        using Base = List_View_Base<Header>;

        Header* first = nullptr;
        Header* last = nullptr;
        Header::size_type size = 0;

        func begin() noexcept -> auto       { return Base::begin(this->first, this->last); }
        func begin() const noexcept -> auto { return Base::cbegin(this->first, this->last); }
        func end() noexcept -> auto         { return Base::end(this->first, this->last); }
        func end() const noexcept -> auto   { return Base::cend(this->first, this->last); }
    };
    
    namespace detail
    {
        template <block_header Header, allocator Alloc>
        func dealloc_list(List_View_<Header>* list, Alloc* alloc) -> void;
    }

    template <
        block_header Header_, 
        allocator Alloc = std::allocator<typename Header_::value_type>>
    struct Intrusive_List : Alloc, List_View_<Header_>
    {
        using value_type = typename Header_::value_type;
        using Block_Sizer = typename Header_::block_sizer;
        using size_type = typename Header_::size_type;

        using Header = Header_;
        using Size = size_type;
        using Value = value_type;
        using View = List_View_<Header_>;

        Intrusive_List() noexcept = default;
        Intrusive_List(Alloc allocator, View view = View()) noexcept 
            : Alloc(move(allocator)), View(view) {}
        Intrusive_List(Intrusive_List && other) noexcept = default;

        ~Intrusive_List() noexcept
        {
            detail::dealloc_list(view(), alloc());
        }

        Intrusive_List& operator=(Intrusive_List&& other) noexcept
        {
            swap(this, &other);
            return *this;
        }

        proc alloc() noexcept -> Alloc*                             {return cast(Alloc*)this; }
        proc alloc() const noexcept -> const Alloc *                {return cast(const Alloc*)this; }

        proc view() noexcept -> View*                               {return cast(View*)this; }
        proc view() const noexcept -> const View *                  {return cast(const View*)this; }

        proc block_sizer() noexcept -> Block_Sizer*                 {return cast(Block_Sizer*) view(); }
        proc block_sizer() const noexcept -> const Block_Sizer *    {return cast(const Block_Sizer*) view(); }

        static void swap(Intrusive_List* self, Intrusive_List* other) noexcept 
        {
            std::swap(*self->alloc(), *other->alloc());
            std::swap(*self->view(), *other->view());
        }

        static_assert(std::input_iterator<Intrusive_List::iterator>);
        static_assert(std::output_iterator<Intrusive_List::iterator, Header>);
        static_assert(std::forward_iterator<Intrusive_List::iterator>);
        static_assert(std::input_iterator<Intrusive_List::iterator>);
        static_assert(bidi_block_header<Header> == std::bidirectional_iterator<Intrusive_List::iterator>);
    };


    #define LIST_TEMPL block_header Header, allocator Alloc
    #define FORWARD_LIST_TEMPL forward_block_header Header, allocator Alloc
    #define BACKWARD_LIST_TEMPL backward_block_header Header, allocator Alloc
    #define SIZED_LIST_TEMPL sized_block_header Header, allocator Alloc
    #define List_T Intrusive_List<Header, Alloc>

    namespace detail 
    {
        template <block_header Header, integral Size>
        func block_at(Header* from, Size block_offset, Iter_Direction direction = DEF_DIRECTION<Header>) -> Header*
        {
            assert(is_valid_direction<Header>(direction));

            mut* current = from;
            while(true)
            {
                assert(current != nullptr && "Header index must be in range");
                if(block_offset == 0)
                    break;

                current = advance(current, direction);
                block_offset --;
            }

            return current;
        }

        template <block_header Header, integral Size>
        func slice_range(Header* from, Size block_count, Iter_Direction direction = DEF_DIRECTION<Header>) -> List_View_<Header>
        {
            assert(is_valid_direction<Header>(direction));

            mut* current = from;
            Size passed_size = 0;
            Size i = 0;

            if(block_count == 0)
                return List_View_<Header>{};

            assert(current != nullptr && "Header index must be in range");
            while(true)
            {
                i++;

                if constexpr(sized_block_header<Header>)
                    passed_size += current->size;
                if(i == block_count)
                    break;

                current = advance(current, direction);
                assert(current != nullptr && "Header index must be in range");
            }

            mut view = List_View_<Header>{
                .first = from,
                .last = current,
                .size = block_count,
            };

            if constexpr(sized_block_header<Header>)
                view.item_size = passed_size;

            return view;
        }

        template <typename Header, integral Size>
        struct At_Result 
        {
            Header* header;
            Size index;
        };

        template <block_header Header, integral Size>
        func block_and_item_at(Header* from, Size item_index, Iter_Direction direction = DEF_DIRECTION<Header>) -> At_Result<Header, Size>
        {
            assert(is_valid_direction<Header>(direction));

            i64 signed_index = item_index;
            mut* current = from;
            while(true)
            {
                assert(current != nullptr && "Header index must be in range");
                item_index -= current->size;
                if(signed_index < 0)
                {
                    signed_index += current->size;
                    return At_Result<Header, Size>{
                        .header = from,
                        .index = cast(Size)(signed_index),
                    };
                }

                current = advance(current, direction);
            }
        }

        template <typename Header, integral Size>
        func item_at(Header* from, Size item_index, Iter_Direction direction = DEF_DIRECTION<Header>) -> Header*
        {
            mut found = block_and_item_at(from, item_index, direction);
            return data(found.header) + found.index;
        }

        template <block_header Header>
        func link(Header* before, Header* first_inserted, Header* last_inserted, Header* after) -> void
        {
            if constexpr(forward_block_header<Header>)
            {
                if(before != nullptr)
                    before->next = first_inserted;

                last_inserted->next = after;
            }

            if constexpr(backward_block_header<Header>)
            {
                if(after != nullptr)
                    after->prev = last_inserted;

                first_inserted->prev = before;
            }
        }

        template <block_header Header>
        func unlink(Header* before, Header* first_removed, Header* last_removed, Header* after) -> void
        {
            if constexpr(forward_block_header<Header>)
            {
                if(before != nullptr)
                    before->next = after;

                last_removed->next = nullptr;
            }

            if constexpr(backward_block_header<Header>)
            {
                if(after != nullptr)
                    after->prev = before;

                first_removed->prev = nullptr;
            }
        }
    }

    /*template <BLOCK_TEMPL, integral Size_>
        requires std::convertible_to<Size_, Size>
    func block_at(Block_T* from, Size_ block_index, Iter_Direction dir = Iter_Direction::FORWARD) -> Block_T*
    {
        return detail::block_at(from, block_index);
    }

    template <BLOCK_TEMPL, integral Size_>
        requires std::convertible_to<Size_, Size>
    func block_at(Block_T const& from, Size_ block_index, Iter_Direction dir = Iter_Direction::FORWARD) -> Block_T
    {
        return *detail::block_at(&from, block_index);
    }

    template <BLOCK_TEMPL, integral Size_>
        requires std::convertible_to<Size_, Size>
    func item_at(Block_T* from, Size_ item_index, Iter_Direction dir = Iter_Direction::FORWARD) -> T*
    {
        return detail::item_at(from, item_index);
    }

    template <BLOCK_TEMPL, integral Size_>
        requires std::convertible_to<Size_, Size>
    func item_at(Block_T const& from, Size_ item_index, Iter_Direction dir = Iter_Direction::FORWARD) -> T
    {
        return *detail::item_at(&from, item_index);
    }

    template <LIST_TEMPL, integral Size_>
        requires std::convertible_to<Size_, Size>
    func block_at(List_T* list, Size_ block_index, Iter_Direction dir = Iter_Direction::FORWARD) -> Block_T*
    {
        if(dir == Iter_Direction::FORWARD)
            return block_at(list->first, block_index, dir);
        else
            return block_at(list->last, block_index, dir);
    }

    template <LIST_TEMPL, integral Size_>
        requires std::convertible_to<Size_, Size>
    func block_at(List_T const& list, Size_ block_index, Iter_Direction dir = Iter_Direction::FORWARD) -> Block_T
    {
        if(dir == Iter_Direction::FORWARD)
            return block_at(*list.first, block_index, dir);
        else
            return block_at(*list.last, block_index, dir);
    }

    template <LIST_TEMPL, integral Size_>
        requires std::convertible_to<Size_, Size>
    func item_at(List_T* list, Size_ item_index, Iter_Direction dir = Iter_Direction::FORWARD) -> T*
    {
        if(dir == Iter_Direction::FORWARD)
            return item_at(list->first, item_index, dir);
        else
            return item_at(list->last, item_index, dir);
    }

    template <LIST_TEMPL, integral Size_>
        requires std::convertible_to<Size_, Size>
    func item_at(List_T const& list, Size_ item_index, Iter_Direction dir = Iter_Direction::FORWARD) -> T
    {
        if(dir == Iter_Direction::FORWARD)
            return item_at(*list.first, item_index, dir);
        else
            return item_at(*list.last, item_index, dir);
    }*/


    template <LIST_TEMPL>
    func empty(List_T const& list) -> bool  
    { 
        return list.first == nullptr; 
    }

    template <LIST_TEMPL>
    func is_empty(List_T const& list) -> bool 
    {
        return list.first == nullptr; 
    }

    template <block_header Header>
    func is_separated(Header const& from, Header const& to) -> bool  
    { 
        bool separated = true;
        if constexpr(forward_block_header<Header>)
            separated = separated && to.next == nullptr;
        if constexpr(backward_block_header<Header>)
            separated = separated && from.prev == nullptr;

        return separated; 
    }

    template <block_header Header>
    static func is_separated(List_View_<Header> const& list) noexcept -> bool
    {
        return is_separated(*list.first, *list.last);
    }

    static constexpr bool EXPENSIVE_TESTING = true;
    template <block_header Header>
    static func is_invariant(List_View_<Header> const& list, bool expensive = false) noexcept -> bool
    {
        using Size = typename Header::size_type;

        bool all_active = list.first != nullptr && list.last != nullptr;
        bool all_inactive = list.first == nullptr && list.last == nullptr;

        bool item_size_match = true;
        bool size_match = true;
        bool ptrs_match = true;

        if(expensive || EXPENSIVE_TESTING)
        {
            Size calced_item_size = 0;
            Size calced_size = 0;
            const Header* first = list.begin().header;
            const Header* last = nullptr;

            for(mut it = list.begin(); it != list.end(); ++it)
            {
                if constexpr(sized_block_header<Header>)
                    calced_item_size += it.header->size;
                calced_size++;
                last = it.header;
            }

            if constexpr(sized_block_header<Header>)
                item_size_match = list.item_size == calced_item_size && "item size must match";

            size_match = list.size == calced_size && "size must match";

            if constexpr(forward_block_header<Header>)
                ptrs_match = list.first == first && list.last == last;
            else //else iterates backwards => is in reverse
                ptrs_match = list.first == last && list.last == first;

            ptrs_match = ptrs_match && "iteration must pass through all blocks (chain has to be valid)";
        }

        return item_size_match && size_match && ptrs_match && (all_active || all_inactive && "all fields must either be set or unset");
    }

    template <typename Filler, typename For_Type>
    concept filler_function = requires(Filler filler)
    {
        { filler(0) } -> std::convertible_to<For_Type>;
    };

    template <typename Filler, typename For_Type>
    concept filler_function_2D = requires(Filler filler)
    {
        { filler(0, 5) } -> std::convertible_to<For_Type>;
    };

    namespace detail
    {
        template <block_header Header, allocator Alloc>
        func dealloc_list(List_View_<Header>* list, Alloc* alloc) -> void
        {
            using Size = typename Header::size_type;
            using Value = typename Header::value_type;

            assert(is_invariant(*list, true));
            assert(is_separated(*list));

            for(mut it = list->begin(); it != list->end(); )
            {
                let last = it;
                ++it;

                Header* current = last.header;
                Value* items = data(current);
                Size size = list->block_size(*current);
                for(Size i = 0; i < size; i++)
                    items[i].~Value();

                deallocate_block<Header, Alloc>(alloc, current, size);
            }
        }

        //@TODO: Transform fillers into functions taking begin end and filling it -> that way we can have uninit storage for free
        template <LIST_TEMPL, typename Fn>
            requires filler_function<Fn, typename List_T::Value>
        pure proc make_block(Alloc* alloc, typename Header::size_type size, Fn filler) -> Header*
        {
            using Size = typename List_T::Size;

            Header* header = allocate_block<Header, Alloc>(alloc, size);

            mut* data = jot::data(header);
            for(Size i = 0; i < size; i++)
                data[i] = filler(i);

            return header;
        }

        template <LIST_TEMPL, typename Fn>
            requires filler_function<Fn, typename List_T::Value>
        pure proc make_block(Alloc* alloc, typename Header::block_sizer const& sizer, Fn filler) -> Header*
        {
            return make_block(alloc, sizer.default_block_size(), filler);
        }

        template <FORWARD_LIST_TEMPL>
        proc from_block(Alloc alloc, typename Header::block_sizer const& sizer, Header* header) -> List_T
        {
            List_T made{move(alloc), List_View_<Header>{sizer, header, header, 1}};
            if constexpr(sized_block_header<Header>)
                made.item_size = header->size;

            return made;
        }

        template <LIST_TEMPL, typename Fn1, typename Fn2>
            requires filler_function<Fn1, typename List_T::Size> && filler_function_2D<Fn2, typename List_T::Value>
        func make_blocks(Alloc alloc, typename Header::size_type block_count, Fn1 sizes, Fn2 filler) -> List_T
        {
            using Size = typename List_T::Size;
            using Sizer = typename Header::block_sizer;

            if(block_count == 0)
                return List_T{move(alloc)};

            let make_filler_1D = [&](Size block_index) {
                return [&](Size item_index){ 
                    return filler(item_index); 
                };
            };

            Size first_item_size = sizes(0);
            Header* first = detail::make_block<Header>(&alloc, first_item_size, make_filler_1D(0));
            Header* last = first;
            Size item_size = first_item_size;

            for(Size i = 1; i < block_count; i++)
            {
                Size current_item_size = sizes(i);
                item_size += current_item_size;
                Header* current = detail::make_block<Header>(&alloc, current_item_size, make_filler_1D(i));

                if constexpr(bidi_block_header<Header>)
                {
                    last->next = current;
                    current->prev = last;
                }
                else if constexpr(forward_block_header<Header>)
                    last->next = current;
                else if constexpr(backward_block_header<Header>)
                    last->prev = current;
            }

            List_View_<Header> view;
            if constexpr(forward_block_header<Header>)
                view = List_View_<Header>{Sizer(), first, last, 1};
            else if constexpr(backward_block_header<Header>)
                view = List_View_<Header>{Sizer(), last, first, 1};

            if constexpr(sized_block_header<Header>)
                view.item_size = item_size;

            List_T made{move(alloc), view};
        }
    }

    template <LIST_TEMPL, typename Fn>
        requires filler_function<Fn, typename List_T::Value>
    func make_block(Alloc alloc, typename Header::block_sizer const& sizer, Fn filler) -> List_T
    {
        Header* header = detail::make_block<Header>(&alloc, sizer, filler);
        return detail::from_block(move(alloc), sizer, header);
    }

    template <SIZED_LIST_TEMPL, typename Fn>
        requires filler_function<Fn, typename List_T::Value>
    func make_block(Alloc alloc, typename Header::size_type item_count, Fn filler) -> List_T
    {
        Header* header = detail::make_block<Header>(&alloc, item_count, filler);
        return detail::from_block(move(alloc), typename Header::block_sizer(), header);
    }

    template <SIZED_LIST_TEMPL, typename Fn1, typename Fn2>
        requires filler_function<Fn1, typename List_T::Size> && filler_function_2D<Fn2, typename List_T::Value>
    func make_blocks(Alloc alloc, typename Header::size_type block_count, Fn1 sizes, Fn2 filler) -> List_T
    {
        return detail::make_blocks(move(alloc), block_count, sizes, filler);
    }

    template <SIZED_LIST_TEMPL, typename Fn>
        requires filler_function<Fn, typename List_T::Value>
    func make_blocks(Alloc alloc, typename Header::size_type block_count, typename Header::size_type block_size, Fn filler) -> List_T
    {
        using Size = typename List_T::Size;
        let size_func = [=](Size) -> Size 
            { return block_size; };
        let filler_func = [=](Size, Size index) -> typename List_T::Value 
            { return filler(index); };

        return detail::make_blocks(move(alloc), block_count, size_func, filler_func);
    }

    template <LIST_TEMPL, typename Fn>
        requires filler_function<Fn, typename List_T::Value>
    func make_blocks(Alloc alloc, typename Header::size_type block_count, typename Header::block_sizer const& sizer, Fn filler) -> List_T
    {
        using Size = typename List_T::Size;
        Size const_size = sizer.default_block_size();
        let size_func = [=](Size) -> Size 
            { return const_size; };
        let filler_func = [=](Size, Size index) -> typename List_T::Value 
            { return filler(index); };

        return detail::make_blocks(move(alloc), block_count, size_func, filler_func);
    }

    template <FORWARD_LIST_TEMPL>
    proc push_back(List_T* list, Header* header) -> List_View_<Header>
    {
        assert(is_invariant(*list));
        assert(is_separated(*header));

        if(list->last == nullptr)
            list->first = header;
        else
            detail::link<Header>(list->last, header, header, nullptr);

        list->size += 1;
        list->last = header;

        mut view = List_View_<Header>{*list->block_sizer(),header, header, 1};
        if constexpr(sized_block_header<Header>)
        {
            list->item_size += header->size;
            view.item_size += header->size;
        }

        assert(is_invariant(*list));
        return view;
    }

    template <BACKWARD_LIST_TEMPL>
    proc push_front(List_T* list, Header* header) -> List_View_<Header>
    {
        assert(is_invariant(*list));
        assert(is_separated(*header));

        if(list->last == nullptr)
            list->last = header;
        else
            detail::link<Header>(nullptr, header, header, list->first);

        list->size += 1;
        list->last = header;

        mut view = List_View_<Header>{*list->block_sizer(),header, header, 1};
        if constexpr(sized_block_header<Header>)
        {
            list->item_size += header->size;
            view.item_size += header->size;
        }

        assert(is_invariant(*list));
        return view;
    }

    template <FORWARD_LIST_TEMPL>
    proc push_back(List_T* list, List_T&& inserted) -> List_View_<Header>
    {
        assert(is_invariant(*list));
        
        List_View_<Header> ret = inserted;
        detail::link<Header>(list->last, inserted.first, inserted.last, nullptr);

        if(list->first == nullptr)
            list->first = inserted.first;

        list->last = inserted.last;
        list->size += inserted.size;

        inserted.first = nullptr;
        inserted.last = nullptr;
        inserted.size = 0;

        if constexpr(sized_block_header<Header>)
        {
            list->item_size += inserted.item_size;
            inserted.item_size = 0;
        }

        assert(is_invariant(*list));
        return ret;
    }

    template <typename Range, typename T>
    concept forward_range_of = stdr::forward_range<Range> && std::convertible_to<stdr::range_value_t<Range>, T>;

    template <FORWARD_LIST_TEMPL, typename Inserted>
        requires sized_block_header<Header> && forward_range_of<Inserted, typename Header::value_type>
    proc push_back(List_T* list, Inserted&& inserted) -> List_View_<Header>
    {
        mut it = stdr::begin(inserted);
        Header* header = detail::make_block<Header, Alloc>(list->alloc(), stdr::size(inserted), [&](typename Header::size_type){
            return *(it ++);
        });
        return push_back(list, header);
    }

    template <FORWARD_LIST_TEMPL>
        requires sized_block_header<Header> || static_sized_block_header_to<Header, 1>
    proc push_back(List_T* list, typename Header::value_type inserted) -> List_View_<Header>
    {
        Header* header = detail::make_block<Header, Alloc>(list->alloc(), 1, [&](typename Header::size_type){
            return std::move(inserted);
        });
        return push_back(list, header);
    }


    template <BACKWARD_LIST_TEMPL>
    proc push_front(List_T* list, List_T&& inserted) -> List_View_<Header>
    {
        assert(is_invariant(*list));

        List_View_<Header> ret = inserted;
        detail::link<Header>(nullptr, inserted.first, inserted.last, list->first);

        if(list->last == nullptr)
            list->last = inserted.last;

        list->first = inserted.first;
        list->size += inserted.size;

        inserted.last = nullptr;
        inserted.first = nullptr;
        inserted.size = 0;

        if constexpr(sized_block_header<Header>)
        {
            list->item_size += inserted.item_size;
            inserted.item_size = 0;
        }

        assert(is_invariant(*list));
        return ret;
    }

    template <BACKWARD_LIST_TEMPL, typename Inserted>
        requires sized_block_header<Header> && forward_range_of<Inserted, typename Header::value_type>
    proc push_front(List_T* list, Inserted&& inserted) -> List_View_<Header>
    {
        mut it = stdr::begin(inserted);
        Header* header = detail::make_block<Header, Alloc>(list->alloc(), stdr::size(inserted), [&](typename Header::size_type){
            return *(it ++);
            });
        return push_front(list, header);
    }

    template <BACKWARD_LIST_TEMPL>
        requires sized_block_header<Header> || static_sized_block_header_to<Header, 1>
    proc push_front(List_T* list, typename Header::value_type inserted) -> List_View_<Header>
    {
        Header* header = detail::make_block<Header, Alloc>(list->alloc(), 1, [&](typename Header::size_type){
            return std::move(inserted);
            });
        return push_front(list, header);
    }

    template <BACKWARD_LIST_TEMPL>
    proc pop_back(List_T* list, typename Header::size_type count = 1) -> List_T
    {
        assert(is_invariant(*list));
        assert(list->first != nullptr && "cannot pop empty list");

        let slice = detail::slice_range(list->last, count, Iter_Direction::BACKWARD);
        mut popped = List_T{*list->alloc(), slice};

        list->last = slice.first->prev;
        list->size -= popped.size;
        list->item_size -= popped.item_size;

        detail::unlink<Header>(list->last, popped.first, popped.last, nullptr);
        if(list->last == nullptr)
            list->first = nullptr;

        assert(is_invariant(*list));
        assert(is_invariant(popped));

        return popped;
    }

    template <FORWARD_LIST_TEMPL>
    proc pop_front(List_T* list, typename Header::size_type count = 1) -> List_T
    {
        assert(is_invariant(*list));
        assert(list->first != nullptr && "cannot pop empty list");

        let slice = detail::slice_range(list->first, count, Iter_Direction::FORWARD);
        mut popped = List_T{*list->alloc(), slice};

        list->first = list->first->next;
        list->size -= popped.size;
        list->item_size -= popped.item_size;

        detail::unlink<Header>(nullptr, popped.first, popped.last, list->first);
        if(list->first == nullptr)
            list->last = nullptr;

        assert(is_invariant(*list));
        assert(is_invariant(popped));

        return popped;
    }


    template <LIST_TEMPL>
    proc find_block_before(List_T const& list, Header const* header) -> Header*
    {
        assert(is_invariant(list, true));
        if constexpr(bidi_block_header<Header>)
            return header->prev;
        else
        {   
            Header* current = begin(list).header;
            while(current != nullptr)
            {
                Header* next = advance(current);
                if(next == header)
                    return current;

                current = next;
            }

            return nullptr;
        }    
    }

    template <LIST_TEMPL>
        requires (std::is_copy_constructible_v<Alloc>)
    proc pop_block(List_T* list, Header* at) -> List_T
    {
        assert(is_invariant(*list));
        assert(list->size > 0);

        if(at == list->first)
            return pop_front(list);
        if(at == list->last)
            return pop_back(list);

        Header* before = find_block_before(*list, at);

        assert(before != nullptr && "header must be within the list");
        if constexpr(forward_block_header<Header>)
            detail::unlink(before, at, at, at->next);
        else
            detail::unlink(at->prev, at, at, before);

        assert(is_invariant(*list));
        return detail::from_block(*list->alloc(), *list->block_sizer(), at);
    }

    template <LIST_TEMPL>
    func alloc(List_T const& list) -> Alloc { return *list.alloc(); }

    template <LIST_TEMPL>
    func alloc(List_T* list) -> Alloc* { return list->alloc(); }

    template <LIST_TEMPL>
    func view(List_T const& list) -> List_View_<Header> { return *list.view(); }

    template <LIST_TEMPL>
    func view(List_T* list) -> List_View_<Header>* { return list->view(); }

    template <LIST_TEMPL>
    func block_sizer(List_T const& list) -> Header::block_sizer { return *list.block_sizer(); }

    template <LIST_TEMPL>
    func block_sizer(List_T* list) -> Header::block_sizer* { return list->block_sizer(); }

    #undef LIST_TEMPL 
    #undef FORWARD_LIST_TEMPL 
    #undef BACKWARD_LIST_TEMPL 
    #undef SIZED_LIST_TEMPL 
    #undef List_T

    //Most common blocks
    template<non_void T, size_t size_ = 1, integral Size = Def_Size> 
    struct Forward_Block
    {
        using value_type = T;
        using tag_type = List_Block_Tag;
        using size_type = Size;

        static constexpr size_type size = cast(size_type) size_;
        Forward_Block* next = nullptr;

        struct block_sizer 
        {
            func block_size(Forward_Block const&) -> size_type { return size; }
            func default_block_size() -> size_type { return size; }
        };
    };

    template<non_void T, size_t size_ = 1, integral Size = Def_Size> 
    struct Backward_Block
    {
        using value_type = T;
        using tag_type = List_Block_Tag;
        using size_type = Size;

        static constexpr size_type size = cast(size_type) size_;
        Backward_Block* prev = nullptr;

        struct block_sizer 
        {
            func block_size(Backward_Block const&) -> size_type { return size; }
            func default_block_size() -> size_type { return size; }
        };
    };

    template<non_void T, size_t size_ = 1, integral Size = Def_Size> 
    struct Bidi_Block
    {
        using value_type = T;
        using tag_type = List_Block_Tag;
        using size_type = Size;

        static constexpr size_type size = cast(size_type) size_;
        Bidi_Block* prev = nullptr;
        Bidi_Block* next = nullptr;

        struct block_sizer 
        {
            func block_size(Bidi_Block const&) -> size_type { return size; }
            func default_block_size() -> size_type { return size; }
        };
    };

    template<non_void T, integral Size = Def_Size> 
    struct Bidi_Block_Sized
    {
        using value_type = T;
        using tag_type = List_Block_Tag;
        using size_type = Size;

        size_type size = 0;
        Bidi_Block_Sized* prev = nullptr;
        Bidi_Block_Sized* next = nullptr;

        struct block_sizer 
        {
            func block_size(Bidi_Block_Sized const& header) -> size_type { return header.size; }
            func default_block_size() -> size_type { return size; }
        };
    };

    template<non_void T, integral Size = Def_Size> 
    struct Bidi_Block_Uniform_Sized
    {
        using value_type = T;
        using tag_type = List_Block_Tag;
        using size_type = Size;

        Bidi_Block_Uniform_Sized* prev = nullptr;
        Bidi_Block_Uniform_Sized* next = nullptr;

        struct block_sizer 
        {
            size_type size = 0;
            func block_size(Bidi_Block_Uniform_Sized const&) -> size_type { return size; }
            func default_block_size() -> size_type { return size; }
        };
    };


    template<non_void T, size_t size_ = 1, integral Size = Def_Size, allocator Alloc = std::allocator<T>> 
    using Forward_List_ = Intrusive_List<Forward_Block<T, size_, Size>, Alloc>;

    template<non_void T, size_t size_ = 1, integral Size = Def_Size, allocator Alloc = std::allocator<T>> 
    using Backward_List_ = Intrusive_List<Backward_Block<T, size_, Size>, Alloc>;

    template<non_void T, size_t size_ = 1, integral Size = Def_Size, allocator Alloc = std::allocator<T>>
    using Bidi_List_ = Intrusive_List<Bidi_Block<T, size_, Size>, Alloc>;

    template<typename T, integral Size = Def_Size, allocator Alloc = std::allocator<T>> 
    using Block_List_ = Intrusive_List<Bidi_Block_Sized<T, Size>, Alloc>;

    template<typename T, integral Size = Def_Size, allocator Alloc = std::allocator<T>> 
    using Uniform_Block_List_ = Intrusive_List<Bidi_Block_Uniform_Sized<T, Size>, Alloc>;

    namespace detail::test
    {
        void tester_block_list2()
        {
            Forward_List_<int> list1;
            Backward_List_<double> list2;
            Uniform_Block_List_<int> list3;
        }

        struct Not_Block
        {
            using value_type = void;
        };

        static_assert(block_header_base<Not_Block> == false);
        static_assert(block_header_base<int> == false);
        static_assert(block_header_base<void> == false);

        struct Block0
        {
            using value_type = int;
            using size_type = size_t;
            using tag_type = List_Block_Tag;

            static constexpr size_type size = 1;

            struct block_sizer 
            {
                func block_size(Block0 const&) -> size_type { return 1; }
                func default_block_size() -> size_type      { return 1; }
            };
        };

        static_assert(block_header_base<Block0>);
        static_assert(forward_block_header<Block0> == false);
        static_assert(backward_block_header<Block0> == false);
        static_assert(sized_block_header<Block0> == false);

        using F_Block = Forward_Block<int>;

        static_assert(block_header_base<F_Block>);
        static_assert(forward_block_header<F_Block>);
        static_assert(backward_block_header<F_Block> == false);
        static_assert(block_header<F_Block>);
        static_assert(bidi_block_header<F_Block> == false);
        static_assert(sized_block_header<F_Block> == false);

        using B_Block = Backward_Block<int>;

        static_assert(block_header_base<B_Block>);
        static_assert(forward_block_header<B_Block> == false);
        static_assert(backward_block_header<B_Block>);
        static_assert(block_header<B_Block>);
        static_assert(bidi_block_header<B_Block> == false);
        static_assert(sized_block_header<B_Block> == false);
        static_assert(static_sized_block_header<B_Block>);
        static_assert(static_sized_block_header_to<B_Block, 1>);
        static_assert(static_sized_block_header_to<B_Block, 7> == false);

        using Bidi_Block = ::jot::Bidi_Block_Sized<int>;

        static_assert(block_header_base<Bidi_Block>);
        static_assert(forward_block_header<Bidi_Block>);
        static_assert(backward_block_header<Bidi_Block>);
        static_assert(block_header<Bidi_Block>);
        static_assert(bidi_block_header<Bidi_Block>);
        static_assert(sized_block_header<Bidi_Block>);
    }

}

#include "jot/defer.h"