#pragma once

#pragma once

#include "slice.h"
#include "types.h"
#include "defines.h"

namespace jot
{
    //example of how block can look like:
    //All blocks are required to have:
    // (1) next ptr
    // (2) is_single_linked constexpr bool
    // (3) prev ptr - only if is_bidirectional is set to true
    #if 0
    struct Example_Node
    {
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

    //Checks if last node is reachable from first
    // null - null is also valid chain but any other combination
    // of null and not null is not
    template<typename Node> nodisc
    bool is_valid_chain(const Node* first, const Node* last)
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

    template<typename Node> nodisc static
    bool is_isolated(Node node)
    {
        if constexpr(Node::is_bidirectional)
            return node.next == nullptr && node.prev == nullptr;
        else
            return node.next == nullptr;
    }

    template<typename Node> nodisc static
    bool is_isolated(const Node first, const Node last)
    {
        if constexpr(Node::is_bidirectional)
            return last.next == nullptr && first.prev == nullptr;
        else
            return last.next == nullptr;
    }
        
    template<typename Node> 
    void link_chain(Node* before, Node* first_inserted, Node* last_inserted, Node* after)
    {
        assert(first_inserted != nullptr && last_inserted != nullptr && "must not be null");
        assert(is_isolated(*first_inserted, *last_inserted) && "must be isolated");
        
        last_inserted->next = after;
        if(before != nullptr)
            before->next = first_inserted;
            
        if constexpr(Node::is_bidirectional)
        {
            first_inserted->prev = before;
            if(after != nullptr)
                after->prev = last_inserted;
        }
    }

    template<typename Node> 
    void unlink_chain(Node* before, Node* first_inserted, Node* last_inserted, Node* after)
    {
        assert(first_inserted != nullptr && last_inserted != nullptr && "must not be null");

        last_inserted->next = nullptr;
        if(before != nullptr)
            before->next = after;

        if constexpr(Node::is_bidirectional)
        {
            first_inserted->prev = nullptr;
            if(after != nullptr)
                after->prev = before;
        }
    }
    template<typename Node> 
    Node* extract_node(Chain<Node>* from, Node* extact_after, Node* what) 
    {
        assert(is_valid_chain(from->first, from->last));
        assert(what != nullptr);
        assert(from->first != nullptr && "cant extract from empty chain");

        //if is start of chain
        if(extact_after == nullptr)
            from->first = what->next;
        else
            assert(extact_after->next == what);
            
        //if is end of chain
        if(what == from->last)
            from->last = extact_after;

        unlink_chain(extact_after, what, what, what->next);
            
        if(from->first == nullptr || from->last == nullptr)
        {
            from->first = nullptr;
            from->last = nullptr;
        }
        
        assert(is_valid_chain(from->first, from->last));
        return what;
    }

    template<typename Node>
    void insert_node(Chain<Node>* to, Node* insert_after, Node* what)
    {
        assert(is_valid_chain(to->first, to->last));
        assert(what != nullptr && is_isolated(*what) && "must be isolated");

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
            link_chain<Node>(nullptr, what, what, to->first->next);
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
        
        assert(is_valid_chain(to->first, to->last));
    }
}

#include "undefs.h"