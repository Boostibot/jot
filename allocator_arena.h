#pragma once

#include "memory.h"
#include "intrusive_list.h"

namespace jot 
{
    //Allocate lineary from block. If the block is exhausted request more memory from its parent alocator and add it to block list.
    // Can be easily reset without freeying any acquired memeory. Releases all emmory in destructor
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
        isize used_blocks = 0; 
        isize max_used_blocks = 0;
        isize bytes_alloced_ = 0; 
        isize bytes_used_ = 0;
        isize max_bytes_alloced_ = 0;
        isize max_bytes_used_ = 0;
        
        static constexpr isize ARENA_BLOCK_ALLOCED_BIT = (isize) 1 << (sizeof(isize) * CHAR_BIT - 1);
        static constexpr isize ARENA_BLOCK_ALIGN = 32;


        Arena_Allocator(
            Allocator* parent = memory_globals::default_allocator(), 
            isize chunk_size = memory_constants::PAGE,
            Grow_Fn chunk_grow = default_arena_grow) 
            : parent(parent), chunk_grow(chunk_grow), chunk_size(chunk_size)
        {
            reset_last_allocation();
            assert(is_invariant());
        }
        
        NODISCARD virtual
        void* allocate(isize size, isize align, Line_Info callee) noexcept override
        {
            assert(is_power_of_two(align));
            uint8_t* aligned = (uint8_t*) align_forward(available_from, align);
            uint8_t* used_to = aligned + size;

            if(used_to > available_to)
            {
                bool state = obtain_block_and_update(size, align);
                if(!state)
                    return nullptr;

                return allocate(size, align, callee);
            }

            available_from = used_to;
            last_allocation = aligned;

            bytes_alloced_ += size;
            max_bytes_alloced_ = max(max_bytes_alloced_, bytes_alloced_);

            return aligned;
        } 
        
        virtual 
        bool deallocate(void* allocated, isize old_size, isize align, Line_Info) noexcept override
        {
            assert(is_power_of_two(align));
            if(allocated != last_allocation)
                return true;

            available_from = (uint8_t*) allocated;
            reset_last_allocation();
            
            bytes_alloced_ -= old_size;
            assert(bytes_alloced_ >= 0);

            return true;
        }

        NODISCARD virtual
        bool resize(void* allocated, isize new_size, isize old_size, isize align, Line_Info) noexcept override
        {
            assert(is_power_of_two(align));
            uint8_t* used_to = available_from + new_size;
            if(allocated != last_allocation || used_to > available_to)
                return false;

            available_from = used_to;
            bytes_alloced_ += new_size - old_size;
            assert(bytes_alloced_ >= 0);

            return true;
        }

        virtual
        Stats get_stats() const noexcept override
        {
            Stats stats = {};
            stats.name = "Arena_Allocator";
            stats.supports_resize = true;
            stats.bytes_allocated = bytes_alloced_;
            stats.max_bytes_allocated = max_bytes_alloced_;
            stats.bytes_used = bytes_used_;
            stats.max_bytes_used = max_bytes_used_;
            return stats;
        }

        virtual
        ~Arena_Allocator() override
        {
            assert(is_invariant());

            isize passed_bytes = deallocate_and_count_chain(parent, blocks);
            assert(passed_bytes == bytes_used_);
        }

        void add_external_block(Slice<uint8_t> block_data)
        {
            if(block_data.size < sizeof(Block))
                return;

            Block* block = (Block*) (void*) block_data.data;
            *block = Block{};
            block->size = block_data.size - (isize) sizeof(Block);

            Chain<Block> free = free_chain();
            Arena_Block_Found found_res = find_block_to_fit(free, current_block, block->size, 1);

            insert_node(&blocks, found_res.before, block);
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

            bytes_alloced_ = 0;
            reset_last_allocation();
        }

        void release_extra_memory() 
        {
            isize released = deallocate_and_count_chain(parent, free_chain());
            blocks.last = current_block;
            bytes_used_ -= released;
        }

        void reset_last_allocation() noexcept 
        {
            last_allocation = (uint8_t*) (void*) this;
        }

        //@TODO do not return it but make it get added to the front of the free_blocks list
        Block* allocate_block(isize size, isize align) noexcept
        {
            assert(is_invariant());

            isize effective_size = size + (isize) sizeof(Block);
            if(align > ARENA_BLOCK_ALIGN)
                effective_size += align;

            isize required_size = max(effective_size, chunk_size);
            Block* block = (Block*) parent->allocate(required_size, ARENA_BLOCK_ALIGN, GET_LINE_INFO());
            if(block == nullptr)
                return nullptr;

            *block = Block{};
            block->was_alloced = true;
            block->size = required_size - (isize) sizeof(Block); //@TODO: s!

            used_blocks ++;
            bytes_used_ += required_size;
            max_bytes_used_ = max(max_bytes_used_, bytes_used_);
            max_used_blocks = max(max_used_blocks, used_blocks);
            chunk_size = chunk_grow(chunk_size);

            return block;
        }

        bool obtain_block_and_update(isize size, isize align) noexcept
        {
            assert(is_invariant());
            Block* obtained = nullptr;

            Arena_Block_Found found_res = find_block_to_fit(free_chain(), current_block, size, align);
            if(found_res.found == nullptr)
                obtained = allocate_block(size, align);
            else
                obtained = extract_node(&blocks, found_res.before, found_res.found);
            
            if(obtained == nullptr)
                return false;

            available_from = data(obtained);
            available_to = available_from + obtained->size;

            insert_node(&blocks, current_block, obtained);
            reset_last_allocation();

            current_block = obtained;

            assert(is_invariant());
            return true;
        }

        bool is_invariant() const noexcept
        {
            bool available_inv1 = available_from <= available_to;
            bool available_inv2 = (available_from == nullptr) == (available_to == nullptr);

            bool last_alloc_inv1 = last_allocation != nullptr;

            bool blocks_inv1 = is_connected(blocks.first, blocks.last);
            
            bool blocks_inv2 = (blocks.first == nullptr) == (used_blocks == 0) && used_blocks >= 0;
            isize count = 0;
            Block* current = blocks.first;
            while(current != nullptr)
            {
                count ++;
                current = current->next;
            }

            blocks_inv2 = blocks_inv2 && count == used_blocks;
            assert(count == used_blocks);

            bool parent_inv = parent != nullptr;
            bool block_size_inv = chunk_size > sizeof(Block);

            bool stat_inv1 = bytes_used_ >= 0 
                && bytes_alloced_ >= 0 
                && max_bytes_used_ >= 0
                && max_bytes_alloced_ >= 0;

            bool stat_inv2 = bytes_used_ >= bytes_alloced_
                && max_bytes_used_ >= bytes_used_
                && max_bytes_alloced_ >= bytes_alloced_;

            bool total_inv = available_inv1 && available_inv2 
                && last_alloc_inv1
                && blocks_inv1 && blocks_inv2
                && parent_inv 
                && block_size_inv
                && stat_inv1 
                && stat_inv2;

            return total_inv;
        }

        static
        isize default_arena_grow(isize current)
        {
            if(current == 0)
                return memory_constants::PAGE;

            isize new_size = current * 2;
            return min(new_size, memory_constants::GIBI_BYTE);
        }
        
        static
        uint8_t* data(Block* block)
        {
            uint8_t* address = (uint8_t*) (void*) block;
            return address + sizeof(Block);
        }
        
        static
        isize deallocate_and_count_chain(Allocator* alloc, Chain<Block> chain)
        {
            isize passed_bytes = 0;
            Block* current = chain.first;
            Block* prev = nullptr;
            while(current != nullptr)
            {
                prev = current;
                current = current->next;

                isize total_block_size = prev->size + (isize) sizeof(Block);
                passed_bytes += total_block_size;

                if(prev->was_alloced)
                    alloc->deallocate(prev, total_block_size, ARENA_BLOCK_ALIGN, GET_LINE_INFO());
            }

            assert(prev == chain.last && "must be a valid chain!");
            return passed_bytes;
        }

        //@TODO: inline
        struct Arena_Block_Found
        {
            Block* before = nullptr;
            Block* found = nullptr;
        };

        static
        Arena_Block_Found find_block_to_fit(Chain<Block> chain, Block* before, isize size, isize align)
        {
            Block* prev = before;
            Block* current = chain.first;
            Arena_Block_Found found;
            while(current != nullptr)
            {
                uint8_t* block_data = data(current);
                void* aligned = align_forward(block_data, align);
                isize aligned_size = ptrdiff(aligned, block_data + current->size);
                if(aligned_size >= size)
                {
                    found.before = prev;
                    found.found = current;
                    return found;
                }

                prev = current;
                current = current->next;
            }
            
            return found;
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
