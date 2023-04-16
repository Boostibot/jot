#pragma once

//if defined checks the integrity (if the list is conected)
// on every mutating function
#if 0
#define INTRUSIVE_LIST_PEDANTIC
#endif

namespace jot
{
    //A collection of primitives for dealing with linked lists.
    //Because linked lists can take all shapes and sizes it is better
    // to implement only the bare minimum and let the user pack it in
    // a nice List class than trying to implement every single list in
    // existance (and failing).

    //Works on all nodes that have the following structure:
    // (1) `next` ptr
    // (2) static constexpr bool `is_bidirectional`
    // (3) `prev` ptr* 
    //     * prev ptr required only if is_bidirectional is set to true
    #if 0
    struct Example_Node
    {
        int my_data1 = 0;
        int my_data2 = 0;

        static constexpr bool is_bidirectional = true;
        Example_Node* next = nullptr;
        Example_Node* prev = nullptr;
    };
    #endif
    
    template<typename Node>
    struct Chain
    {
        Node* first = nullptr;
        Node* last = nullptr;
    };

    template<typename Node> constexpr
    bool is_isolated(Node node)
    {
        if constexpr(Node::is_bidirectional)
            return node.next == nullptr && node.prev == nullptr;
        else
            return node.next == nullptr;
    }

    template<typename Node> constexpr
    bool is_isolated(const Node first, const Node last)
    {
        if constexpr(Node::is_bidirectional)
            return last.next == nullptr && first.prev == nullptr;
        else
            return last.next == nullptr;
    }

    //Checks if last node is reachable from first.
    // null - null is also valid chain but any other combination
    // of null and not null is not
    template<typename Node> constexpr
    bool is_connected(const Node* first, const Node* last)
    {
        const Node* current = first;
        const Node* prev = nullptr;

        while(current != nullptr && prev != last)
        {
            prev = current;
            current = current->next;
        }

        return prev == last;
    }
    
    template<typename Node> constexpr
    bool _check_is_connected(const Node* first, const Node* last) noexcept
    {
        #ifdef INTRUSIVE_INDEX_LIST_PEDANTIC
        return is_connected(first, last, arr);
        #else
        (void) first; (void) last;
        return true;
        #endif
    }

    //Adds chain between (and including) first_inserted - last_inserted between before
    // and after after. Both before and after can be nullptr in that case they are treated 
    // as the start/end of the list and properly handled
    template<typename Node> constexpr
    void link_chain(Node* before, Node* first_inserted, Node* last_inserted, Node* after) noexcept
    {
        assert(first_inserted != nullptr && last_inserted != nullptr && "must not be null");
        assert(is_isolated(*first_inserted, *last_inserted) && "must be isolated");
        
        last_inserted->next = after;
        if(before != nullptr)
        {
            assert(before->next == after && "before and after must be adjecent!");
            before->next = first_inserted;
        }
            
        if constexpr(Node::is_bidirectional)
        {
            first_inserted->prev = before;
            if(after != nullptr)
            {
                assert(after->prev == before && "before and after must be adjecent!");
                after->prev = last_inserted;
            }
        }
    }

    //Removes a chain between (and including) first_inserted - last_inserted.
    // Both before and after can be nullptr in that case they are treated as start/end of
    // list and properly handled
    template<typename Node> constexpr
    void unlink_chain(Node* before, Node* first_inserted, Node* last_inserted, Node* after) noexcept
    {
        assert(first_inserted != nullptr && last_inserted != nullptr && "must not be null");

        last_inserted->next = nullptr;
        if(before != nullptr)
        {
            assert(before->next == first_inserted && "before and first_inserted must be adjecent!");
            before->next = after;
        }

        if constexpr(Node::is_bidirectional)
        {
            first_inserted->prev = nullptr;
            if(after != nullptr)
            {
                assert(after->prev == last_inserted && "last_inserted and after must be adjecent!");
                after->prev = before;
            }
        }
    }

    //Unlinks node after extract_after (which has to be what) properly unlinking it from the
    // chain and returning it. extract_after can be nullptr in which case it is treated as the start 
    // of the chain and what has to be the first node. what cannot be nullptr
    template<typename Node> constexpr
    Node* extract_node(Chain<Node>* from, Node* extract_after, Node* what) noexcept
    {
        assert(_check_is_connected(from->first, from->last));
        assert(what != nullptr && "cannot be nullptr");
        assert(from->first != nullptr && "cant extract from empty chain");

        //if is start of chain
        if(extract_after == nullptr)
            from->first = what->next;
        else
            assert(extract_after->next == what);
            
        //if is end of chain
        if(what == from->last)
            from->last = extract_after;

        unlink_chain(extract_after, what, what, what->next);
            
        if(from->first == nullptr || from->last == nullptr)
        {
            from->first = nullptr;
            from->last = nullptr;
        }
        
        assert(_check_is_connected(from->first, from->last));
        return what;
    }

    //Links what node after insert_after properly linking it to the rest of the chain.
    // if the insert after is nullptr it is inserted as the first node to the chain.
    // what cannot be nullptr and must be isolated node (not part of any chain)
    template<typename Node> constexpr
    void insert_node(Chain<Node>* to, Node* insert_after, Node* what) noexcept
    {
        assert(_check_is_connected(to->first, to->last));
        assert(what != nullptr && "cannot be nullptr");
        assert(is_isolated(*what) && "must be isolated");

        if(to->first == nullptr)
        {
            assert(insert_after == nullptr);
            to->first = what;
            to->last = what;
            return;
        }

        //if is start of chain
        if(insert_after == nullptr)
        {
            link_chain<Node>(nullptr, what, what, to->first);
            to->first = what;
        }
        //if is end of chain
        else if(insert_after == to->last)
        {
            link_chain<Node>(insert_after, what, what, nullptr);
            to->last = what;
        }
        else
        {
            link_chain(insert_after, what, what, insert_after->next);
        }
        
        assert(_check_is_connected(to->first, to->last));
    }
}
