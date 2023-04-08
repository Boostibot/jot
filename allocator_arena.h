#pragma once

#include "memory.h"
#include "intrusive_list.h"
#include "utils.h"
#include "defines.h"

namespace jot 
{
    namespace detail
    {
        struct Arena_Block
        {
            static constexpr bool is_bidirectional = false;
            Arena_Block* next = nullptr;
            uint32_t size = 0;
            uint32_t was_alloced = false;
        };
        
        static constexpr isize ARENA_BLOCK_ALLOCED_BIT = cast(isize) 1 << (sizeof(isize) * CHAR_BIT - 1);
        static constexpr isize ARENA_BLOCK_ALIGN = 32;

        nodisc static
        Slice<u8> slice(Arena_Block* block)
        {
            u8* address = cast(u8*) cast(void*) block;
            return Slice<u8>{address + sizeof(Arena_Block), block->size}; //@TODO casts!
        }
        
        static
        isize deallocate_and_count_chain(Allocator* alloc, Chain<Arena_Block> chain)
        {
            isize passed_bytes = 0;
            Arena_Block* current = chain.first;
            Arena_Block* prev = nullptr;
            while(current != nullptr)
            {
                prev = current;
                current = current->next;

                Slice<u8> total_block_data; //@TODO: casts!
                total_block_data.data = cast(u8*) cast(void*) prev;
                total_block_data.size = prev->size + cast(isize) sizeof(Arena_Block);

                passed_bytes += total_block_data.size;

                if(prev->was_alloced)
                    alloc->deallocate(total_block_data, ARENA_BLOCK_ALIGN);
            }

            assert(prev == chain.last && "must be a valid chain!");
            return passed_bytes;
        }

        struct Arena_Block_Found
        {
            Arena_Block* before = nullptr;
            Arena_Block* found = nullptr;
        };

        nodisc static
        Arena_Block_Found find_block_to_fit(Chain<Arena_Block> chain, Arena_Block* before, isize size, isize align)
        {
            Arena_Block* prev = before;
            Arena_Block* current = chain.first;
            Arena_Block_Found found;
            while(current != nullptr)
            {
                Slice<u8> block_data = slice(current);
                Slice<u8> aligned = align_forward(block_data, align);
                if(aligned.size >= size)
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
    }
    
    static
    isize default_arena_grow(isize current)
    {
        if(current == 0)
            return memory_constants::PAGE;

        isize new_size = current * 2;
        return min(new_size, memory_constants::GIBI_BYTE*4);
    }

    //Allocate lineary from block. If the block is exhausted request more memory from its parent alocator and add it to block list.
    // Can be easily reset without freeying any acquired memeory. Releases all emmory in destructor
    struct Arena_Allocator : Allocator
    {
        using Arena_Block = detail::Arena_Block;
        using Chain = Chain<Arena_Block>;
        using Grow_Fn = isize(*)(isize);

        u8* available_from = nullptr;
        u8* available_to = nullptr;
        u8* last_allocation = nullptr;

        Chain blocks = {};
        Arena_Block* current_block = nullptr; 

        Allocator* parent = nullptr;
        Grow_Fn chunk_grow = nullptr;

        isize chunk_size = 0; 
        isize used_blocks = 0; 
        isize max_used_blocks = 0;
        isize bytes_alloced_ = 0; 
        isize bytes_used_ = 0;
        isize max_bytes_alloced_ = 0;
        isize max_bytes_used_ = 0;

        Arena_Allocator(
            Allocator* parent = memory_globals::default_allocator(), 
            isize chunk_size = memory_constants::PAGE,
            Grow_Fn chunk_grow = default_arena_grow) 
            : parent(parent), chunk_grow(chunk_grow), chunk_size(chunk_size)
        {
            reset_last_allocation();
            assert(is_invariant());
        }

        nodisc virtual
        Allocation_State allocate(Slice<u8>* output, isize size, isize align) noexcept override
        {
            assert(is_power_of_two(align));
            u8* aligned = align_forward(available_from, align);
            u8* used_to = aligned + size;

            if(used_to > available_to)
            {
                Allocation_State state = obtain_block_and_update(size, align);
                if(state != Allocation_State::OK)
                {
                    *output = Slice<u8>{};
                    return state;
                }

                return allocate(output, size, align);
            }

            *output = Slice<u8>{aligned, size};
            available_from = used_to;
            last_allocation = aligned;

            update_bytes_alloced(size);

            return Allocation_State::OK;
        }

        nodisc virtual
        Allocation_State deallocate(Slice<u8> allocated, isize align) noexcept override 
        {
            assert(is_power_of_two(align));
            if(allocated.data != last_allocation)
                return Allocation_State::OK;

            available_from = allocated.data;
            reset_last_allocation();
            
            bytes_alloced_ -= allocated.size;
            assert(bytes_alloced_ >= 0);

            return Allocation_State::OK;
        } 

        nodisc virtual
        Allocation_State resize(Slice<u8>* output, Slice<u8> allocated, isize new_size, isize align) noexcept override
        {
            assert(is_power_of_two(align));
            u8* used_to = available_from + new_size;
            if(allocated.data != last_allocation || used_to > available_to)
            {
                *output = Slice<u8>{};
                return Allocation_State::NOT_RESIZABLE;
            }

            available_from = used_to;

            update_bytes_alloced(new_size - allocated.size);
            output->data = allocated.data;
            output->size = new_size;
            return Allocation_State::OK;
        }

        nodisc virtual 
        isize bytes_allocated() const noexcept override
        {
            return bytes_alloced_;
        }

        nodisc virtual 
        isize bytes_used() const noexcept override 
        {
            return bytes_used_;    
        }

        nodisc virtual 
        isize max_bytes_allocated() const noexcept override
        {
            return max_bytes_alloced_;
        }

        nodisc virtual 
        isize max_bytes_used() const noexcept override 
        {
            return max_bytes_used_;    
        }
         
        virtual
        ~Arena_Allocator() override
        {
            assert(is_invariant());

            isize passed_bytes = deallocate_and_count_chain(parent, blocks);
            assert(passed_bytes == bytes_used_);
        }

        void add_external_block(Slice<u8> block_data)
        {
            if(block_data.size < sizeof(Arena_Block))
                return;

            Arena_Block* block = cast(Arena_Block*) cast(void*) block_data.data;
            *block = Arena_Block{};
            block->size = block_data.size - cast(isize) sizeof(Arena_Block); //@TODO casts!

            Chain free = free_chain();
            detail::Arena_Block_Found found_res = detail::find_block_to_fit(free, current_block, block->size, 1);

            insert_node(&blocks, found_res.before, block);
        }

        Chain used_chain() const noexcept
        {
            return Chain{blocks.first, current_block};
        }
        
        Chain free_chain() const noexcept
        {
            if(current_block == nullptr)
                return Chain{nullptr, nullptr};

            assert(current_block != nullptr);
            return Chain{current_block->next, blocks.last};
        }

        void reset() 
        {
            current_block = blocks.first;

            Slice<u8> block;
            if(current_block != nullptr)
                block = slice(current_block);

            available_from = block.data;
            available_to = block.data + block.size;

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
            last_allocation = cast(u8*) cast(void*) this;
        }

        struct Obtained_Arena_Block
        {
            Arena_Block* block;
            Allocation_State state;
        };
        
        //@TODO do not return it but make it get added to the front of the free_blocks list
        Obtained_Arena_Block allocate_block(isize size, isize align) noexcept
        {
            assert(is_invariant());
            using namespace detail;

            isize effective_size = size + cast(isize) sizeof(Arena_Block);
            if(align > ARENA_BLOCK_ALIGN)
                effective_size += align;

            isize required_size = max(effective_size, chunk_size);

            Slice<u8> obtained;
            Allocation_State state = parent->allocate(&obtained, required_size, ARENA_BLOCK_ALIGN);
            if(state != Allocation_State::OK)
                return Obtained_Arena_Block{nullptr, state};

            Arena_Block* block = cast(Arena_Block*) cast(void*) obtained.data;
            *block = Arena_Block{};
            block->was_alloced = true;
            block->size = required_size - cast(isize) sizeof(Arena_Block); //@TODO: casts!

            used_blocks ++;
            bytes_used_ += required_size;
            max_bytes_used_ = max(max_bytes_used_, bytes_used_);
            max_used_blocks = max(max_used_blocks, used_blocks);
            chunk_size = chunk_grow(chunk_size);

            return Obtained_Arena_Block{block, Allocation_State::OK};
        }

        Allocation_State obtain_block_and_update(isize size, isize align) noexcept
        {
            using namespace detail;
            assert(is_invariant());
            Arena_Block* obtained = nullptr;

            Arena_Block_Found found_res = find_block_to_fit(free_chain(), current_block, size, align);
            if(found_res.found == nullptr)
            {
                Obtained_Arena_Block obtained_ = allocate_block(size, align);
                if(obtained_.state != Allocation_State::OK)
                    return obtained_.state;

                obtained = obtained_.block;
            }
            else
            {
                obtained = extract_node(&blocks, found_res.before, found_res.found);
            }

            assert(obtained != nullptr);
            Slice<u8> block_data = slice(obtained);
            available_from = block_data.data;
            available_to = block_data.data + block_data.size;

            insert_node(&blocks, current_block, obtained);
            reset_last_allocation();

            current_block = obtained;

            assert(is_invariant());
            return Allocation_State::OK;
        }

        nodisc
        bool is_invariant() const noexcept
        {
            bool available_inv1 = available_from <= available_to;
            bool available_inv2 = (available_from == nullptr) == (available_to == nullptr);

            bool last_alloc_inv1 = last_allocation != nullptr;

            bool blocks_inv1 = is_connected(blocks.first, blocks.last);
            
            bool blocks_inv2 = (blocks.first == nullptr) == (used_blocks == 0) && used_blocks >= 0;
            isize count = 0;
            Arena_Block* current = blocks.first;
            while(current != nullptr)
            {
                count ++;
                current = current->next;
            }

            assert(count == used_blocks);

            bool parent_inv = parent != nullptr;
            bool block_size_inv = chunk_size > sizeof(Arena_Block);

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

        void 
        update_bytes_alloced(isize delta)
        {
            bytes_alloced_ += delta;
            max_bytes_alloced_ = max(max_bytes_alloced_, bytes_alloced_);
            assert(bytes_alloced_ >= 0);
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

#include "undefs.h"