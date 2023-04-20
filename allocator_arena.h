#pragma once

#include "memory.h"
#include "intrusive_list.h"

namespace jot 
{
    //Allocate lineary from block. If the block is exhausted request more memory from its parent alocator and add it to block list.
    // Can be easily reset without freeying any acquired memeory. Releases all memory in destructor
    struct Arena_Allocator : Allocator
    {
        struct Block
        {
            static constexpr bool is_bidirectional = false;
            Block* next = nullptr;
            uint32_t size = 0;
            uint32_t was_alloced = false;
        };

        using Grow_Fn = isize(*)(isize);

        uint8_t* available_from = nullptr;
        uint8_t* available_to = nullptr;
        uint8_t* last_allocation = nullptr;

        Chain<Block> blocks = {};
        Block* current_block = nullptr; 

        Allocator* parent = nullptr;
        Grow_Fn chunk_grow = nullptr;

        isize chunk_size = 0; 
        isize bytes_alloced = 0; 
        isize bytes_used = 0;
        isize max_bytes_alloced = 0;
        isize max_bytes_used = 0;
        
        isize used_blocks = 0; 
        isize max_used_blocks = 0;
        
        static constexpr isize ARENA_BLOCK_ALIGN = 16;

        Arena_Allocator(
            Allocator* parent = memory_globals::default_allocator(), 
            isize chunk_size = memory_constants::PAGE,
            Grow_Fn chunk_grow = default_arena_grow) 
            : parent(parent), chunk_grow(chunk_grow), chunk_size(chunk_size)
        {
            assert(is_invariant());
        }
        
        NODISCARD virtual
        void* allocate(isize size, isize align, Line_Info callee) noexcept override
        {
            assert(is_power_of_two(align));
            uint8_t* aligned = (uint8_t*) align_forward(available_from, align);

            if(aligned + size > available_to)
            {
                bool state = find_or_add_block(size, align);
                if(!state)
                    return nullptr;

                return allocate(size, align, callee);
            }

            available_from = aligned + size;
            last_allocation = aligned;

            bytes_alloced += size;
            max_bytes_alloced = max(max_bytes_alloced, bytes_alloced);

            return aligned;
        } 
        
        virtual 
        bool deallocate(void* allocated, isize old_size, isize align, Line_Info) noexcept override
        {
            assert(is_power_of_two(align));
            bytes_alloced -= old_size;

            uint8_t* ptr = (uint8_t*) allocated;
            if(ptr != last_allocation || ptr + old_size != available_from)
                return true;

            available_from = (uint8_t*) allocated;
            assert(bytes_alloced >= 0);

            return true;
        }

        NODISCARD virtual
        bool resize(void* allocated, isize old_size, isize new_size, isize align, Line_Info) noexcept override
        {
            assert(is_power_of_two(align));
            uint8_t* used_to = available_from + new_size;
            if(allocated != last_allocation || used_to > available_to)
                return false;

            available_from = used_to;
            bytes_alloced += new_size - old_size;
            assert(bytes_alloced >= 0);

            return true;
        }

        virtual
        Stats get_stats() const noexcept override
        {
            Stats stats = {};
            stats.name = "Arena_Allocator";
            stats.supports_resize = true;
            stats.parent = parent;

            stats.bytes_allocated = bytes_alloced;
            stats.max_bytes_allocated = max_bytes_alloced;
            stats.bytes_used = bytes_used;
            stats.max_bytes_used = max_bytes_used;
            return stats;
        }

        virtual
        ~Arena_Allocator() override
        {
            assert(is_invariant());
            isize passed_bytes = 0;
            

            Block* current = blocks.first;
            Block* prev = nullptr;
            while(current != nullptr)
            {
                prev = current;
                current = current->next;

                isize total_block_size = prev->size + (isize) sizeof(Block);
                passed_bytes += total_block_size;

                if(prev->was_alloced)
                    parent->deallocate(prev, total_block_size, ARENA_BLOCK_ALIGN, GET_LINE_INFO());
            }

            assert(prev == blocks.last && "must be a valid chain!");
            assert(passed_bytes >= bytes_used);
        }

        void add_external_block(void* buffer, isize buffer_size)
        {
            if(buffer_size < sizeof(Block))
                return;

            Block* block = (Block*) (void*) buffer;
            *block = Block{};
            block->was_alloced = false;
            block->size = (uint32_t) buffer_size - sizeof(Block);

            Chain<Block> free = free_chain();
            Block* before = nullptr;
            for(Block* curr = free.first; curr; before = curr, curr = curr->next)
            {
                if(curr->size >= buffer_size)
                    break;
            }

            insert_node(&blocks, before, block);
        }

        Chain<Block> used_chain() const noexcept
        {
            return Chain<Block>{blocks.first, current_block};
        }
        
        Chain<Block> free_chain() const noexcept
        {
            if(current_block == nullptr)
                return Chain<Block>{nullptr, nullptr};

            assert(current_block != nullptr);
            return Chain<Block>{current_block->next, blocks.last};
        }

        void reset() 
        {
            current_block = blocks.first;
            if(current_block != nullptr)
            {
                available_from = data(current_block);
                available_to = available_from + current_block->size;
            }
            else
            {
                available_from = nullptr;
                available_to = nullptr;
            }

            bytes_alloced = 0;
        }

        bool find_or_add_block(isize size, isize align) noexcept
        {
            assert(is_invariant());
            Block* obtained = nullptr;

            //Tries to find an open block that would fit size and align
            Chain<Block> free = free_chain();
            Block* before = nullptr;
            Block* found = nullptr;

            for(Block* curr = free.first; curr; before = curr, curr = curr->next)
            {
                uint8_t* block_data = data(curr);
                void* aligned = align_forward(block_data, align);
                isize aligned_size = ptrdiff(aligned, block_data + curr->size);
                if(aligned_size >= size)
                {
                    found = curr;
                    break;
                }
            }
            
            //If found extract it
            if(found != nullptr)
                obtained = extract_node(&blocks, before, found);
            //Else allocate it
            else
            {
                if(parent == nullptr)
                    return false;

                isize effective_size = size + (isize) sizeof(Block);
                if(align > ARENA_BLOCK_ALIGN)
                    effective_size += align;

                isize required_size = max(effective_size, chunk_size);
                obtained = (Block*) parent->allocate(required_size, ARENA_BLOCK_ALIGN, GET_LINE_INFO());
                if(obtained == nullptr)
                    return false;

                *obtained = Block{};
                obtained->was_alloced = true;
                obtained->size = (uint32_t) required_size - sizeof(Block);

                used_blocks ++;
                bytes_used += required_size;
                max_bytes_used = max(max_bytes_used, bytes_used);
                max_used_blocks = max(max_used_blocks, used_blocks);
                chunk_size = chunk_grow(chunk_size);
                assert(obtained != nullptr);
            }
            
            assert(obtained != nullptr);

            //Add it to the end and set statistics
            insert_node(&blocks, current_block, obtained);
            available_from = data(obtained);
            available_to = available_from + obtained->size;
            current_block = obtained;

            assert(is_invariant());
            return true;
        }

        bool is_invariant() const noexcept
        {
            bool available_inv1 = available_from <= available_to;
            bool available_inv2 = (available_from == nullptr) == (available_to == nullptr);

            isize count = 0;
            Block* last = nullptr;
            for(Block* current = blocks.first; current; current = current->next)
            {
                last = current;
                count ++;
            }
            
            bool blocks_inv1 = last == blocks.last && count == used_blocks;
            bool blocks_inv2 = (blocks.first == nullptr) == (used_blocks == 0) && used_blocks >= 0;

            bool block_size_inv = chunk_size > sizeof(Block);

            bool stat_inv = bytes_used >= 0 && max_bytes_used >= 0;

            bool total_inv = available_inv1 && available_inv2 
                && blocks_inv1 && blocks_inv2
                && block_size_inv
                && stat_inv;

            return total_inv;
        }

        static isize default_arena_grow(isize current)
        {
            if(current == 0)
                return memory_constants::PAGE;

            isize new_size = current * 2;
            if(new_size > memory_constants::GIBI_BYTE)
                new_size = memory_constants::GIBI_BYTE;
            
            return new_size;
        }
        
        static uint8_t* data(Block* block)
        {
            uint8_t* address = (uint8_t*) (void*) block;
            return address + sizeof(Block);
        }

        static isize max(isize a, isize b)
        {
            return a > b ? a : b;
        }
    };

    struct Unbound_Stack_Allocator;
    struct Unbound_Tracking_Stack_Allocator;
    //this structure should get used in the following way:
    //we quickly set it up: should be 0 init
    //we add first block of static data
    //we allocate
    //we destroy
    //we dont do any block shuffeling at all since its 
    //we cannot really guarantee that the blocks will be in order if we allow this function to be present durring runtime
    //we dont have to guarantee that they are in order. We simply scan until we find then move the pointer there in order or not.
    //we grow from last size using the static function
    //we need to store: 
    //  1 - vtable ptr    
    //  2 - blocks first + last
    //  1 - current block (also block start really so thats fine)
    //  1 - block to
    //  1 - last allocation
    //  1 - last allocation to
    //  1 - parent ptr
    //  ------------------
    //  8 & zero init!
    // 
    //  we can potentially do some template magic to use this with tracking as well
    //  Unbound_Stack_Allocator & Unbound_Tracking_Stack_Allocator
    //  
    //  provide a proc for ordering based on size which will probably be the most complex proc
    //  
    // this should be exactly the same speed as simp stack allocator but able to use static buffer and grow
}
