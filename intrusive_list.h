#pragma once

//if defined checks the integrity (if the list is conected)
// on every mutating function
#if 0
#define INTRUSIVE_LIST_PEDANTIC
#define INTRUSIVE_LIST_SINGLE
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
    // (3) `prev` ptr  (required only if INTRUSIVE_LIST_DOUBLE)
    #if 0
    struct Example_Node
    {
        int my_data1 = 0;
        int my_data2 = 0;

        Example_Node* next = nullptr;
        Example_Node* prev = nullptr;
    };
    #endif

    //the single linked version is postfixed with sl (Single Linked)
    // and the double linked with dl (Double Linked)
    #if defined(INTRUSIVE_LIST_SINGLE)
        #define is_isolated_xx          is_isolated_sl
        #define is_connected_xx         is_connected_sl
        #define _check_is_connected_xx  _check_is_connected_dl
        
        #define link_chain_xx   link_chain_sl
        #define unlink_chain_xx unlink_chain_sl
        #define extract_node_xx extract_node_sl
        #define insert_node_xx  insert_node_sl
    
    #elif defined(INTRUSIVE_LIST_DOUBLE)
        #define is_isolated_xx          is_isolated_dl
        #define is_connected_xx         is_connected_dl
        #define _check_is_connected_xx  _check_is_connected_dl
        
        #define link_chain_xx   link_chain_dl
        #define unlink_chain_xx unlink_chain_dl
        #define extract_node_xx extract_node_dl
        #define insert_node_xx  insert_node_dl
    #else
        #error "Define either INTRUSIVE_LIST_SINGLE or INTRUSIVE_LIST_DOUBLE before including intrusive_list.h"
    #endif

    template<typename Node>
    bool is_isolated_xx(Node const& node)
    {
        #ifndef INTRUSIVE_LIST_SINGLE
            return node.next == nullptr && node.prev == nullptr;
        #else
            return node.next == nullptr;
        #endif
    }

    template<typename Node>
    bool is_isolated_xx(Node const& first, Node const& last)
    {
        
        #ifndef INTRUSIVE_LIST_SINGLE
            return last.next == nullptr && first.prev == nullptr;
        #else
            (void) first;
            return last.next == nullptr;
        #endif
    }

    //Checks if last node is reachable from first.
    // null - null is also valid chain but any other combination
    // of null and not null is not
    template<typename Node>
    bool is_connected_xx(const Node* first, const Node* last)
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
    
    template<typename Node>
    bool _check_is_connected_xx(const Node* first, const Node* last) noexcept
    {
        #ifdef INTRUSIVE_INDEX_LIST_PEDANTIC
        return is_connected_xx(first, last, arr);
        #else
        (void) first; (void) last;
        return true;
        #endif
    }

    //Adds chain between (and including) first_inserted - last_inserted between before
    // and after after. Both before and after can be nullptr in that case they are treated 
    // as the start/end of the list and properly handled
    template<typename Node>
    void link_chain_xx(Node* before, Node* first_inserted, Node* last_inserted, Node* after) noexcept
    {
        assert(first_inserted != nullptr && last_inserted != nullptr && "must not be null");
        assert(is_isolated_xx(*first_inserted, *last_inserted) && "must be isolated");
        
        last_inserted->next = after;
        if(before != nullptr)
        {
            assert(before->next == after && "before and after must be adjecent!");
            before->next = first_inserted;
        }
            
        #ifndef INTRUSIVE_LIST_SINGLE
            first_inserted->prev = before;
            if(after != nullptr)
            {
                assert(after->prev == before && "before and after must be adjecent!");
                after->prev = last_inserted;
            }
        #endif
    }
    //Removes a chain between (and including) first_inserted - last_inserted.
    // Both before and after can be nullptr in that case they are treated as start/end of
    // list and properly handled
    template<typename Node>
    void unlink_chain_xx(Node* before, Node* first_inserted, Node* last_inserted, Node* after) noexcept
    {
        assert(first_inserted != nullptr && last_inserted != nullptr && "must not be null");

        last_inserted->next = nullptr;
        if(before != nullptr)
        {
            assert(before->next == first_inserted && "before and first_inserted must be adjecent!");
            before->next = after;
        }
        
        #ifndef INTRUSIVE_LIST_SINGLE
            first_inserted->prev = nullptr;
            if(after != nullptr)
            {
                assert(after->prev == last_inserted && "last_inserted and after must be adjecent!");
                after->prev = before;
            }
        #endif
    }

    //Unlinks node after extract_after (which has to be what) properly unlinking it from the
    // chain and returning it. extract_after can be nullptr in which case it is treated as the start 
    // of the chain and what has to be the first node. what cannot be nullptr
    template<typename Node>
    Node* extract_node_xx(Node** first, Node** last, Node* extract_after, Node* what) noexcept
    {
        assert(_check_is_connected_xx(*first, *last));
        assert(what != nullptr && "cannot be nullptr");
        assert(*first != nullptr && "cant extract from empty chain");

        //if is start of chain
        if(extract_after == nullptr)
            *first = what->next;
        else
            assert(extract_after->next == what);
            
        //if is end of chain
        if(what == *last)
            *last = extract_after;

        unlink_chain_xx(extract_after, what, what, what->next);
            
        if(*first == nullptr || *last == nullptr)
        {
            *first = nullptr;
            *last = nullptr;
        }
        
        assert(_check_is_connected_xx(*first, *last));
        return what;
    }

    //Links what node after insert_after properly linking it to the rest of the chain.
    // if the insert after is nullptr it is inserted as the first node to the chain.
    // what cannot be nullptr and must be isolated node (not part of any chain)
    template<typename Node>
    void insert_node_xx(Node** first, Node** last, Node* insert_after, Node* what) noexcept
    {
        assert(_check_is_connected_xx(*first, *last));
        assert(what != nullptr && "cannot be nullptr");
        assert(is_isolated_xx(*what) && "must be isolated");

        if(*first == nullptr)
        {
            assert(insert_after == nullptr);
            *first = what;
            *last = what;
            return;
        }

        //if is start of chain
        if(insert_after == nullptr)
        {
            link_chain_xx<Node>(nullptr, what, what, *first);
            *first = what;
        }
        //if is end of chain
        else if(insert_after == *last)
        {
            link_chain_xx<Node>(insert_after, what, what, nullptr);
            *last = what;
        }
        else
        {
            link_chain_xx<Node>(insert_after, what, what, insert_after->next);
        }
        
        assert(_check_is_connected_xx(*first, *last));
    }
}


#undef is_isolated_xx         
#undef is_connected_xx        
#undef _check_is_connected_xx 
#undef link_chain_xx  
#undef unlink_chain_xx
#undef extract_node_xx
#undef insert_node_xx 

#ifdef INTRUSIVE_LIST_DOUBLE
    #undef INTRUSIVE_LIST_DOUBLE
#endif

#ifdef INTRUSIVE_LIST_SINGLE
    #undef INTRUSIVE_LIST_SINGLE
#endif
