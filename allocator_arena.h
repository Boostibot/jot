#pragma once

#include "memory.h"
#include "types.h"
#include "intrusive_list.h"
#include "defines.h"

namespace jot 
{
    #ifndef ALLOCATOR_UNBOUND_STACK_DEF_SIZE
        #define ALLOCATOR_UNBOUND_STACK_DEF_SIZE 4096
    #endif

    #ifndef ALLOCATOR_UNBOUND_STACK_DEF_GROW
        #define ALLOCATOR_UNBOUND_STACK_DEF_GROW 2
    #endif
    
    //we need only a single ptr. we need only simple size

    namespace detail
    {
        struct Arena_Block
        {
            static constexpr bool is_bidirectional = false;
            Arena_Block* next = nullptr;
            isize size = -1;
        };
        
        static constexpr isize ARENA_BLOCK_ALLOCED_BIT = cast(isize) 1 << (sizeof(isize) * CHAR_BIT - 1);
        static constexpr isize ARENA_BLOCK_ALIGN = 32;

        static
        void set_size(Arena_Block* block, isize size)
        {
            isize new_size = block->size & ARENA_BLOCK_ALLOCED_BIT | size & ~ARENA_BLOCK_ALLOCED_BIT;
            block->size = new_size;
        }
        
        nodisc static
        isize get_size(Arena_Block block)
        {
            return block.size & ~ARENA_BLOCK_ALLOCED_BIT;
        }
        
        nodisc static
        bool was_alloced(Arena_Block block)
        {
            return block.size & ARENA_BLOCK_ALLOCED_BIT;
        }

        nodisc static
        void set_alloced(Arena_Block* block, bool was_alloced = true)
        {
            isize cleared = get_size(*block);
            if(was_alloced)
                cleared |= ARENA_BLOCK_ALLOCED_BIT;

            block->size = cleared;
        }

        nodisc static
        Arena_Block* place_block(Slice<u8> items, bool was_alloced)
        {
            assert(items.size > sizeof(Arena_Block) && "must be big enough");
            Arena_Block* block = cast(Arena_Block*) cast(void*) items.data;
            *block = Arena_Block{};
            detail::set_alloced(block, was_alloced);
            detail::set_size(block, items.size - cast(isize) sizeof(Arena_Block));

            return block;
        }

        nodisc static
        Slice<u8> data(Arena_Block* block)
        {
            u8* address = cast(u8*) cast(void*) block;
            isize size = get_size(*block);
            if(block->size == 0)
                return Slice<u8>{};

            return Slice<u8>{address + sizeof(Arena_Block), size};
        }

        nodisc static
        Slice<u8> used_by_block(Arena_Block* block)
        {
            u8* address = cast(u8*) cast(void*) block;
            isize size = get_size(*block);
            return Slice<u8>{address, size + cast(isize) sizeof(Arena_Block)};
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

                Slice<u8> total_block_data = used_by_block(prev);
                passed_bytes += total_block_data.size;

                if(was_alloced(*prev))
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
                Slice<u8> block_data = data(current);
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

        isize default_arena_grow(isize current)
        {
            if(current == 0)
                return memory_constants::PAGE;

            isize new_size = current * 2;
            return min(new_size, memory_constants::GIBI_BYTE*4);
        }
    }

    #define ARENA_TRACK_BLOCKS

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
        
        #ifdef ARENA_TRACK_BLOCKS
        isize used_blocks = 0;
        isize max_used_blocks = 0;
        #endif

        isize bytes_alloced_ = 0; 
        isize bytes_used_ = 0;
        isize max_bytes_alloced_ = 0;
        isize max_bytes_used_ = 0;

        Arena_Allocator(
            Allocator* parent = memory_globals::default_allocator(), 
            isize chunk_size = memory_constants::PAGE,
            Grow_Fn chunk_grow = detail::default_arena_grow) 
            : parent(parent), chunk_grow(chunk_grow), chunk_size(chunk_size)
        {
            reset_last_allocation();
            assert(is_invariant());
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

            Arena_Block* block = detail::place_block(block_data, false);
            Chain free = free_chain();
            detail::Arena_Block_Found found_res = detail::find_block_to_fit(free, current_block, get_size(*block), 1);

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

        nodisc virtual
        Allocation_Result allocate(isize size, isize align) noexcept override
        {
            assert(is_power_of_two(align));
            u8* aligned = align_forward(available_from, align);
            u8* used_to = aligned + size;

            if(used_to > available_to)
            {
                Allocator_State_Type state = obtain_block_and_update(size, align);
                if(state == ERROR)
                    return Allocation_Result{state};

                return allocate(size, align);
            }

            Slice<u8> alloced = Slice{aligned, size};
            available_from = used_to;
            last_allocation = aligned;

            update_bytes_alloced(size);

            return Allocation_Result{Allocator_State::OK, alloced};
        }

        nodisc virtual
        Allocator_State_Type deallocate(Slice<u8> allocated, isize align) noexcept override 
        {
            assert(is_power_of_two(align));
            if(allocated.data != last_allocation)
                return Allocator_State::OK;

            available_from = allocated.data;
            reset_last_allocation();
            
            bytes_alloced_ -= allocated.size;
            assert(bytes_alloced_ >= 0);

            return Allocator_State::OK;
        } 

        nodisc virtual
        Allocation_Result resize(Slice<u8> allocated, isize align, isize new_size) noexcept override
        {
            assert(is_power_of_two(align));
            u8* used_to = available_from + new_size;
            if(allocated.data != last_allocation || used_to > available_to)
                return Allocation_Result{Allocator_State::NOT_RESIZABLE};

            available_from = used_to;

            update_bytes_alloced(new_size - allocated.size);
            return Allocation_Result{Allocator_State::OK, {allocated.data, new_size}};
        }

        nodisc virtual
        Nullable<Allocator*> parent_allocator() const noexcept override
        {
            return {parent};
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
         
        void reset() 
        {
            current_block = blocks.first;

            Slice<u8> block;
            if(current_block != nullptr)
                block = data(current_block);

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

        nodisc virtual 
        Allocation_Result custom_action(
            Allocator_Action::Type action_type, 
            Nullable<Allocator*> other_alloc, 
            isize new_size, isize new_align, 
            Slice<u8> allocated, isize old_align, 
            Nullable<void*> custom_data) noexcept override
        {
            cast(void) custom_data;
            cast(void) allocated;
            cast(void) other_alloc;
            assert(is_power_of_two(new_align));
            assert(is_power_of_two(old_align));

            if(action_type == Allocator_Action::RESET)
            {
                reset();
                return Allocation_Result{Allocator_State::OK};
            }

            if(action_type == Allocator_Action::RELEASE_EXTRA_MEMORY)
            {
                release_extra_memory();
                return Allocation_Result{Allocator_State::OK};
            }

            assert_arg(new_size >= 0);
            return Allocation_Result{Allocator_State::UNSUPPORTED_ACTION};
        }


        void reset_last_allocation() noexcept 
        {
            last_allocation = cast(u8*) cast(void*) this;
        }

        struct Obtained_Arena_Block
        {
            Arena_Block* block;
            Allocator_State_Type state;
            bool was_just_alloced;
        };
        
        Obtained_Arena_Block extract_or_allocate_block(isize size, isize align) noexcept
        {
            Chain free = free_chain();
            detail::Arena_Block_Found found_res = find_block_to_fit(free, current_block, size, align);
            if(found_res.found == nullptr)
                return allocate_block(size, align);

            Arena_Block* extracted = extract_node(&blocks, found_res.before, found_res.found);
            return Obtained_Arena_Block{extracted, Allocator_State::OK, false};
        }
        
        Obtained_Arena_Block allocate_block(isize size, isize align) noexcept
        {
            assert(is_invariant());
            using namespace detail;

            isize effective_size = size + cast(isize) sizeof(Arena_Block);
            if(align > ARENA_BLOCK_ALIGN)
                effective_size += align;

            isize required_size = max(effective_size, chunk_size);

            Allocation_Result result = parent->allocate(required_size, ARENA_BLOCK_ALIGN);
            if(result.state == ERROR)
                return Obtained_Arena_Block{nullptr, result.state, true};

            Arena_Block* block = cast(Arena_Block*) cast(void*) result.items.data;
            *block = Arena_Block{nullptr};
            detail::set_alloced(block, true);
            detail::set_size(block, required_size - cast(isize) sizeof(Arena_Block));

            bytes_used_ += required_size;
            max_bytes_used_ = max(max_bytes_used_, bytes_used_);
            return Obtained_Arena_Block{block, Allocator_State::OK, true};

        }

        Allocator_State_Type obtain_block_and_update(isize size, isize align) noexcept
        {
            assert(is_invariant());
            Obtained_Arena_Block obtained = extract_or_allocate_block(size, align);
            if(obtained.state == ERROR)
                return obtained.state;

            assert(obtained.block != nullptr);
            Slice<u8> block_data = data(obtained.block);

            insert_node(&blocks, current_block, obtained.block);

            available_from = block_data.data;
            available_to = block_data.data + block_data.size;

            reset_last_allocation();

            if(obtained.was_just_alloced)
            {
                
                #ifdef ARENA_TRACK_BLOCKS
                used_blocks ++;
                max_used_blocks = max(max_used_blocks, used_blocks);
                #endif
                chunk_size = chunk_grow(chunk_size);
            }

            current_block = obtained.block;

            assert(is_invariant());
            return obtained.state;
        }

        nodisc
        bool is_invariant() const noexcept
        {
            bool available_inv1 = available_from <= available_to;
            bool available_inv2 = (available_from == nullptr) == (available_to == nullptr);

            bool last_alloc_inv1 = last_allocation != nullptr;

            bool blocks_inv1 = is_valid_chain(blocks.first, blocks.last);
            
            #ifdef ARENA_TRACK_BLOCKS
            bool blocks_inv2 = (blocks.first == nullptr) == (used_blocks == 0) && used_blocks >= 0;
            isize count = 0;
            Arena_Block* current = blocks.first;
            while(current != nullptr)
            {
                count ++;
                current = current->next;
            }

            assert(count == used_blocks);
            #else
            bool blocks_inv2 = true;
            #endif

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