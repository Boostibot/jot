#pragma once

#include "memory.h"
#include "stack.h"
#include "defines.h"

namespace jot 
{
    #ifndef ALLOCATOR_UNBOUND_STACK_DEF_SIZE
        #define ALLOCATOR_UNBOUND_STACK_DEF_SIZE 4096
    #endif

    #ifndef ALLOCATOR_UNBOUND_STACK_DEF_GROW
        #define ALLOCATOR_UNBOUND_STACK_DEF_GROW 2
    #endif

    namespace detail
    {
        struct Block
        {
            Block* next = nullptr;
            Block* prev = nullptr;
            isize size = -1;
            u32 align = -1;
            b32 was_alloced = false;
        };

        struct Chain
        {
            Block* first = nullptr;
            Block* last = nullptr;
        };

        static func data(Block* block) -> Slice<u8>
        {
            u8* address = cast(u8*) cast(void*) block;
            if(block->size == 0)
                return Slice<u8>{};

            return Slice<u8>{address + sizeof(Block), block->size};
        }

        static func used_by_block(Block* block) -> Slice<u8>
        {
            u8* address = cast(u8*) cast(void*) block;
            return Slice<u8>{address, block->size + cast(isize) sizeof(Block)};
        }
        
        static bool
        is_valid_chain(Chain chain) noexcept
        {
            Block* current = chain.first;
            Block* prev = nullptr;

            while(current != nullptr && prev != chain.last)
            {
                prev = current;
                current = current->next;
            }

            return prev == chain.last;
        }
        
        static void
        link_chain(Block* before, Block* first_inserted, Block* last_inserted, Block* after) noexcept
        {
            assert(first_inserted != nullptr && last_inserted != nullptr && "must not be null");
            assert(first_inserted->prev == nullptr && last_inserted->next == nullptr && "must be isolated");

            first_inserted->prev = before;
            if(before != nullptr)
                before->next = first_inserted;

            last_inserted->next = after;
            if(after != nullptr)
                after->prev = last_inserted;
        }

        static void
        unlink_chain(Block* first_inserted, Block* last_inserted) noexcept
        {
            assert(first_inserted != nullptr && last_inserted != nullptr && "must not be null");

            Block* before = first_inserted->prev;
            Block* after = last_inserted->next;

            first_inserted->prev = nullptr;
            if(before != nullptr)
                before->next = after;

            last_inserted->next = nullptr;
            if(after != nullptr)
                after->prev = before;
        }
        
        static Block*
        extract_node(Chain* from, Block* what) noexcept
        {
            assert(is_valid_chain(*from));
            assert(what != nullptr);
            assert(from->first != nullptr && "cant extract from empty chain");

            //if is start of chain
            if(what->prev == nullptr)
                from->first = what->next;
            
            //if is end of chain
            if(what == from->last)
                from->last = what->prev;

            unlink_chain(what, what);
            
            if(from->first == nullptr || from->last == nullptr)
            {
                assert(from->first == nullptr && from->last == nullptr && "@TODO: remove this");
                from->first = nullptr;
                from->last = nullptr;
            }
            assert(is_valid_chain(*from));

            return what;
        }

        static void
        insert_node(Chain* to, Block* insert_after, Block* what) noexcept
        {
            assert(is_valid_chain(*to));
            assert(what != nullptr);
            assert(what->next == nullptr && what->prev == nullptr && "must be isolated");

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
                link_chain(nullptr, what, what, to->first->next);
                to->first = what;
            }
            //if is end of chain
            else if(insert_after == to->last)
            {
                link_chain(insert_after, what, what, nullptr);
                to->last = what;
            }
            else
            {
                link_chain(insert_after, what, what, insert_after->next);
            }

            assert(is_valid_chain(*to));
        }


        static isize
        deallocated_and_count_chain(Allocator* alloc, Chain chain)
        {
            isize passed_bytes = 0;
            Block* current = chain.last;
            Block* next = nullptr;

            //we dealloc backwards because if the parent allocator is some form 
            // of stack allocator it allows us to reclaim more memory
            //If the parent allocator is ring allocator it coealesces the reclaiming 
            // into a single loop
            while(current != nullptr && next != chain.first)
            {
                next = current;
                current = current->prev;

                Slice<u8> total_block_data = used_by_block(next);
                passed_bytes += total_block_data.size;

                if(next->was_alloced)
                    alloc->deallocate(total_block_data, next->align);
            }

            assert(next == chain.first && "must be a valid chain!");

            return passed_bytes;
        }

        static Block*
        find_block_to_fit(Chain chain, isize size, isize align) noexcept
        {
            Block* current = chain.first;
            while(current != nullptr)
            {
                Slice<u8> block_data = data(current);
                Slice<u8> aligned = align_forward(block_data, align);
                if(aligned.size >= size)
                    return current;

                current = current->next;
            }

            return nullptr;
        }
    }

    //Allocate lineary from block. If the block is exhausted request more memory from its parent alocator and add it to block list.
    // Can be easily reset without freeying any acquired memeory. Releases all emmory in destructor
    struct Unbound_Stack_Allocator : Allocator
    {
        using Chain = detail::Chain;
        using Block = detail::Block;

        u8* available_from = nullptr;
        u8* available_to = nullptr;
        Slice<u8> last_allocation = {};

        Chain blocks = {};
        Block* current_block = nullptr;

        Allocator* parent = nullptr;
        isize chunk_size = 0;
        isize chunk_grow = 0;

        isize used_blocks = 0;
        isize max_used_blocks = 0;

        isize bytes_alloced_ = 0; 
        isize bytes_used_ = 0;
        isize max_bytes_alloced_ = 0;
        isize max_bytes_used_ = 0;

        u8 dummy_data[8] = {0};

        Unbound_Stack_Allocator(
            Allocator* parent = memory_globals::default_allocator(), 
            size_t chunk_size = memory_constants::PAGE,
            size_t chunk_grow = 2) 
            : parent(parent), chunk_size(chunk_size), chunk_grow(chunk_grow)
        {
            reset_last_allocation();
            assert(is_invariant());
        }

        ~Unbound_Stack_Allocator()
        {
            assert(is_invariant());

            isize passed_bytes = deallocated_and_count_chain(parent, blocks);
            
            assert(passed_bytes == bytes_used_);
        }

        Chain
        used_chain() const noexcept
        {
            return Chain{blocks.first, current_block};
        }

        Chain
        free_chain() const noexcept
        {
            if(current_block == nullptr)
                return Chain{nullptr, nullptr};

            assert(current_block != nullptr);
            return Chain{current_block->next, blocks.last};
        }

        virtual Allocation_Result 
        allocate(isize size, isize align) noexcept override
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
            last_allocation = alloced;

            update_bytes_alloced(size);

            return Allocation_Result{Allocator_State::OK, alloced};
        }

        virtual Allocator_State_Type 
        deallocate(Slice<u8> allocated, isize align) noexcept override 
        {
            if(allocated != last_allocation)
                return Allocator_State::OK;

            reset_last_allocation();

            #ifdef DO_ALLOCATOR_STATS
            bytes_alloced_ -= allocated.size;
            assert(bytes_alloced_ >= 0);
            #endif

            return Allocator_State::OK;
        } 

        virtual Allocation_Result 
        resize(Slice<u8> allocated, isize align, isize new_size) noexcept override
        {
            u8* used_to = available_from + new_size;
            if(allocated != last_allocation || used_to > available_to)
                return Allocation_Result{Allocator_State::NOT_RESIZABLE};

            available_from = used_to;
            last_allocation.size = new_size;

            update_bytes_alloced(new_size - allocated.size);
            return Allocation_Result{Allocator_State::OK, {allocated.data, new_size}};
        }

        virtual Nullable<Allocator*> 
        parent_allocator() const noexcept override
        {
            return {parent};
        }

        virtual isize 
        bytes_allocated() const noexcept override
        {
            return bytes_alloced_;
        }

        virtual isize 
        bytes_used() const noexcept override 
        {
            return bytes_used_;    
        }

        virtual isize 
        max_bytes_allocated() const noexcept override
        {
            return max_bytes_alloced_;
        }

        virtual isize 
        max_bytes_used() const noexcept override 
        {
            return max_bytes_used_;    
        }

        void 
        reset() 
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

        void 
        release_extra_memory() 
        {
            isize released = deallocated_and_count_chain(parent, free_chain());
            blocks.last = current_block;
            bytes_used_ -= released;
        }

        virtual proc custom_action(
            Allocator_Action::Type action_type, 
            Nullable<Allocator*> other_alloc, 
            isize new_size, u8 new_align, 
            Slice<u8> allocated, u8 old_align, 
            Nullable<void*> custom_data) noexcept -> Allocation_Result
        {
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


        void 
        reset_last_allocation() noexcept 
        {
            last_allocation = Slice<u8>{dummy_data, 0};
        }

        struct Obtained_Block
        {
            Block* block;
            Allocator_State_Type state;
            bool was_just_alloced;
        };

        Obtained_Block
        extract_or_allocate_block(isize size, isize align) noexcept
        {
            Block* found = find_block_to_fit(free_chain(), size, align);
            if(found == nullptr)
                return allocate_block(size, align);

            Block* extracted = extract_node(&blocks, found);
            return Obtained_Block{extracted, Allocator_State::OK, false};
        }

        Obtained_Block
        allocate_block(isize size, isize align) noexcept
        {
            assert(is_invariant());

            isize effective_size = size + sizeof(Block);
            isize required_align = alignof(Block);
            if(align > alignof(Block))
                effective_size += align;

            isize required_size = max(effective_size, chunk_size);

            Allocation_Result result = parent->allocate(required_size, required_align);
            if(result.state == ERROR)
                return Obtained_Block{nullptr, result.state, true};

            Block* block = cast(Block*) cast(void*) result.items.data;
            *block = Block{nullptr, nullptr, required_size - cast(isize) sizeof(Block), cast(u32) required_align, true};

            bytes_used_ += required_size;
            max_bytes_used_ = max(max_bytes_used_, bytes_used_);
            return Obtained_Block{block, Allocator_State::OK, true};

        }

        Allocator_State_Type
        obtain_block_and_update(isize size, isize align) noexcept
        {
            assert(is_invariant());
            Obtained_Block obtained = extract_or_allocate_block(size, align);
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
                used_blocks ++;
                max_used_blocks = max(max_used_blocks, used_blocks);
                chunk_size *= chunk_grow;
            }

            current_block = obtained.block;

            assert(is_invariant());
            return obtained.state;
        }


        bool 
        is_invariant() const noexcept
        {
            bool available_inv1 = available_from <= available_to;
            bool available_inv2 = (available_from == nullptr) == (available_to == nullptr);

            bool last_alloc_inv1 = (last_allocation.data == dummy_data) == (last_allocation.size == 0);
            bool last_alloc_inv2 = last_allocation.data != nullptr;

            bool blocks_inv1 = is_valid_chain(blocks);
            bool blocks_inv2 = (blocks.first == nullptr) == (used_blocks == 0) && used_blocks >= 0;

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
                && last_alloc_inv1 && last_alloc_inv2
                && blocks_inv1 && blocks_inv2
                && parent_inv 
                && block_size_inv
                && stat_inv1 
                && stat_inv2;

            return total_inv;
        }

        void update_bytes_alloced(isize delta)
        {
            #ifdef DO_ALLOCATOR_STATS
            bytes_alloced_ += delta;
            max_bytes_alloced_ = max(max_bytes_alloced_, bytes_alloced_);
            assert(bytes_alloced_ >= 0);
            #endif
        }
    };
}

#include "undefs.h"