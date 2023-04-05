#pragma once

#include "slice.h"
#define nodisc [[nodiscard]]

//if defined checks the integrity (if the list is conected)
// on every mutating function
#if 0
#define INTRUSIVE_INDEX_LIST_PEDANTIC
#endif

namespace jot
{
    //A collection of primitives for dealing with linked lists.
    //Because linked lists can take all shapes and sizes it is better
    // to implement only the bare minimum and let the user pack it in
    // a nice List class than trying to implement every single list in
    // existance (and failing).

    //Works on all nodes that have the following structure:
    // (1) `next` uint32_t index
    // (2) static constexpr bool `is_bidirectional`
    // (3) `prev` uint32_t index* 
    //     * prev index required only if is_bidirectional is set to true
    #if 0
    struct Example_Index_Node
    {
        int my_data1 = 0;
        int my_data2 = 0;

        static constexpr bool is_bidirectional = true;
        uint32_t next = -1;
        uint32_t prev = -1;
    };
    #endif

    //@NOTE: 
    //We use directly uint32_t because its rather difficult to find a proper null value
    //  for all unsigned and signed integer types to work as a standin for nullptr. 
    //
    //We could use something like whenever the index is bigger than the array the index is considered
    //  to be null but I am afraid that would result in bugs showing themselfs too late 
    //  (ie if a bug occured the list would work correctly - but we really want know there was a bug 
    //   so we can fix it)
    //
    //If one is worried that uint32_t is too small of integer type for general use consider the following:
    //  max value of uint32_t is more than 4'000'000. If the node struct contains anything more than just the next
    //  index (ie a single char for example) it will get padded to 8 bytes. This means that the maximum ammount of
    //  nodes is worth 8*4 million = 32GB of space from this data structure alone. Nobody is going to store that much
    //  data in a linked list and if they are they should write their won fuctions.

    static constexpr uint32_t NULL_LIST_INDEX = (uint32_t) -1;

    struct Index_Chain
    {
        uint32_t first = NULL_LIST_INDEX;
        uint32_t last = NULL_LIST_INDEX;
    };
        
    template<typename Node> nodisc constexpr
    bool is_isolated(uint32_t node, Slice<Node> arr) noexcept
    {
        const Node* bucket = &arr[node];
        return bucket->prev == NULL_LIST_INDEX && bucket->next == NULL_LIST_INDEX;
    }
        
    template<typename Node> nodisc constexpr
    bool is_isolated(uint32_t first, uint32_t last, Slice<Node> arr) noexcept
    {
        return arr[first].prev == NULL_LIST_INDEX && arr[last].next == NULL_LIST_INDEX;
    }
    
    template<typename Node> nodisc constexpr
    bool is_connected(uint32_t first, uint32_t last, Slice<Node> arr) noexcept
    {
        uint32_t current = first;
        uint32_t prev = NULL_LIST_INDEX;

        while(current != NULL_LIST_INDEX && prev != last)
        {
            prev = current;
            current = arr[current].next;
        }

        return prev == last;
    }
    
    template<typename Node> nodisc constexpr
    bool _check_is_connected(uint32_t first, uint32_t last, Slice<Node> arr) noexcept
    {
        #ifdef INTRUSIVE_INDEX_LIST_PEDANTIC
        return is_connected(first, last, arr);
        #else
        (void) first;
        (void) last;
        (void) arr;
        return true;
        #endif
    }
    
    template<typename Node> constexpr
    void link_chain(uint32_t before, uint32_t first_inserted, uint32_t last_inserted, uint32_t after, Slice<Node> arr) noexcept
    {
        assert(first_inserted != NULL_LIST_INDEX && last_inserted != NULL_LIST_INDEX && "must not be null");
        
        arr[last_inserted].next = after;
        if(before != NULL_LIST_INDEX)
        {
            assert(arr[before].next == after && "before and after must be adjecent!");
            arr[before].next = first_inserted;
        }
            
        arr[first_inserted].prev = before;
        if(after != NULL_LIST_INDEX)
        {
            assert(arr[after].prev == before && "before and after must be adjecent!");
            arr[after].prev = last_inserted;
        }
    }
        
    template<typename Node> constexpr
    void unlink_chain(uint32_t before, uint32_t first_inserted, uint32_t last_inserted, uint32_t after, Slice<Node> arr) noexcept
    {
        assert(first_inserted != NULL_LIST_INDEX && last_inserted != NULL_LIST_INDEX && "must not be null");

        arr[last_inserted].next = NULL_LIST_INDEX;
        if(before != NULL_LIST_INDEX)
        {
            assert(arr[before].next == first_inserted && "before and first_inserted must be adjecent!");
            arr[before].next = after;
        }

        arr[first_inserted].prev = NULL_LIST_INDEX;
        if(after != NULL_LIST_INDEX)
        {
            assert(arr[after].prev == last_inserted && "last_inserted and after must be adjecent!");
            arr[after].prev = before;
        }
    }
    
    template<typename Node> constexpr
    uint32_t extract_node(Index_Chain* from, uint32_t extract_after, uint32_t what, Slice<Node> arr) noexcept
    {
        assert(what != NULL_LIST_INDEX && "cannot be nullptr");
        assert(from->first != NULL_LIST_INDEX && "cant extract from empty chain");
        assert(_check_is_connected(from->first, from->last, arr));

        //if is start of chain
        if(extract_after == NULL_LIST_INDEX)
            from->first = arr[what].next;
        else
            assert(arr[extract_after].next == what);
            
        //if is end of chain
        if(what == from->last)
            from->last = extract_after;

        unlink_chain(extract_after, what, what, arr[what].next, arr);
            
        if(from->first == NULL_LIST_INDEX || from->last == NULL_LIST_INDEX)
        {
            from->first = NULL_LIST_INDEX;
            from->last = NULL_LIST_INDEX;
        }
        
        assert(is_isolated(what, arr));
        assert(_check_is_connected(from->first, from->last, arr));
            
        return what;
    }
    
    template<typename Node> constexpr
    void insert_node(Index_Chain* to, uint32_t insert_after, uint32_t what, Slice<Node> arr) noexcept
    {
        assert(what != NULL_LIST_INDEX && "cannot be nullptr");
        assert(is_isolated(what, arr) && "must be isolated");
        assert(_check_is_connected(to->first, to->last, arr));

        if(to->first == NULL_LIST_INDEX)
        {
            assert(insert_after == NULL_LIST_INDEX);
            to->first = what;
            to->last = what;
            return;
        }

        //if is start of chain
        if(insert_after == NULL_LIST_INDEX)
        {
            link_chain(NULL_LIST_INDEX, what, what, to->first, arr);
            to->first = what;
        }
        //if is end of chain
        else if(insert_after == to->last)
        {
            link_chain(insert_after, what, what, NULL_LIST_INDEX, arr);
            to->last = what;
        }
        else
        {
            link_chain(insert_after, what, what, arr[insert_after].next, arr);
        }
            
        assert(_check_is_connected(to->first, to->last, arr));
    }

}

#undef nodisc