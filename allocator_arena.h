#pragma once

#include "allocator.h"
#include "stack.h"
#include "defines.h"

namespace jot 
{
    //2097152; //2MiB
    #ifndef ALLOCATOR_ARENA_DEF_SIZE
        #define ALLOCATOR_ARENA_DEF_SIZE 4096*4
    #endif

    struct Unbound_Arena_Allocator : Allocator
    {
        struct Block
        {
            Block* next = nullptr;
            isize size = -1;
            isize align = -1;
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

        static func total_data(Block* block) -> Slice<u8>
        {
            u8* address = cast(u8*) cast(void*) block;
            return Slice<u8>{address, block->size + cast(isize) sizeof(Block)};
        }

        u8* availible_from = nullptr;
        u8* availible_to = nullptr;
        u8* last_allocation = dummy_data;
        isize last_alloced_size = 0;

        Chain used_chain;
        Chain free_chain;

        Allocator* parent = allocator_globals::DEFAULT;
        isize chunk_size = ALLOCATOR_ARENA_DEF_SIZE;

        isize used_blocks = 0;
        isize free_blocks = 0;
        isize max_free_block_size = 0;
        isize max_used_block_size = 0;

        isize max_used_blocks = 0;
        isize total_bytes_alloced = 0; 
        isize total_bytes_used = 0;
        isize max_bytes_alloced = 0;

        u8 dummy_data[8] = {0};

        Unbound_Arena_Allocator(Allocator* parent = allocator_globals::DEFAULT, size_t chunk_size = ALLOCATOR_ARENA_DEF_SIZE) 
            : parent(parent), chunk_size(chunk_size)
        {
            assert(is_invariant());
        }

        ~Unbound_Arena_Allocator()
        {
            assert(is_invariant());

            isize dealloced_bytes = 0;

            dealloced_bytes += dealloc_chain(parent, used_chain);
            dealloced_bytes += dealloc_chain(parent, free_chain);

            assert(dealloced_bytes == total_bytes_used);
        }

        Slice<u8>
        available_slice() const 
        {
            return Slice<u8>(availible_from, ptrdiff(availible_to, availible_from));
        }

        Slice<u8>
        last_alloced_slice() const 
        {
            return Slice<u8>(last_allocation, last_alloced_size);
        }

        static Block*
        extract_node(Chain* from, Chain extracted)
        {
            assert(is_valid_chain(*from));
            assert(extracted.last != nullptr);
            assert(from->first != nullptr);

            //if is start of chain
            if(extracted.first == nullptr)
            {
                from->first = extracted.last->next;
            }
            else
            {
                //if is end of chain
                if(extracted.last == from->last)
                    from->last = extracted.first;

                extracted.first->next = extracted.last->next;
            }

            if(from->first == nullptr || from->last == nullptr)
            {
                from->first = nullptr;
                from->last = nullptr;
            }
            assert(is_valid_chain(*from));

            extracted.last->next = nullptr;
            return extracted.last;
        }

        

        static bool
        is_valid_chain(Chain chain)
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
        push_chain(Chain* chain, Chain pushed)
        {
            assert(is_valid_chain(pushed));
            assert(is_valid_chain(*chain));
            if(chain->last == nullptr)
            {
                assert(chain->first == nullptr);
                chain->first = pushed.first;
                chain->last = pushed.last;
            }
            else
            {
                chain->last->next = pushed.first;
                chain->last = pushed.last;
            }
        }

        static isize
        dealloc_chain(Allocator* allocator, Chain chain)
        {
            isize dealloced_bytes = 0;
            Block* current = chain.first;
            Block* prev = nullptr;
            while(current != nullptr && prev != chain.last)
            {
                prev = current;
                current = current->next;
                Slice<u8> total_block_data = total_data(prev);
                dealloced_bytes += total_block_data.size;
                mut state = allocator->deallocate(total_block_data, prev->align);
                assert(state == OK);
            }

            assert(prev == chain.last && "must be a valid chain!");
            return dealloced_bytes;
        }

        static Chain
        find_block_to_fit(Chain chain, isize size, isize align)
        {
            Block* current = chain.first;
            Block* prev = nullptr;
            while(current != nullptr)
            {
                Slice<u8> block_data = data(current);
                Slice<u8> aligned = align_forward(block_data, align);
                if(aligned.size >= size)
                    return {prev, current};
            }

            return {nullptr, nullptr};
        }

        struct Obtained_Block
        {
            Block* block;
            Allocator_State_Type state;
            bool was_alloced;
        };

        Obtained_Block
        extract_or_allocate_block(Chain* from, isize size, isize align) const noexcept
        {
            if(size > max_free_block_size)
            {
                #ifdef NDEBUG
                    mut found = find_block_to_fit(*from, size, align);
                    assert(found.last == nullptr);
                #endif
                return allocate_block(size, align);
            }

            mut found = find_block_to_fit(*from, size, align);
            if(found.last == nullptr)
            {
                return allocate_block(size, align);
            }

            Block* extracted = extract_node(from, found);
            return Obtained_Block{extracted, Allocator_State::OK, false};
        }

        Obtained_Block
        allocate_block(isize size, isize align) const noexcept
        {
            assert(is_invariant());

            isize effective_size = size + sizeof(Block);
            isize required_align = alignof(Block);
            if(align > alignof(Block))
                effective_size += align;

            isize required_size = max(effective_size, chunk_size);

            Allocator_Result result = parent->allocate(required_size, required_align);
            if(result.state == ERROR)
                return Obtained_Block{nullptr, result.state, true};

            Block* block = cast(Block*) cast(void*) result.items.data;
            *block = Block{nullptr, required_size - cast(isize) sizeof(Block), required_align};
            
            return Obtained_Block{block, Allocator_State::OK, true};

        }

        Allocator_State_Type
        obtain_block_and_update(isize size, isize align) noexcept
        {
            assert(is_invariant());
            Obtained_Block obtained = extract_or_allocate_block(&free_chain, size, align);
            if(obtained.state == ERROR)
                return obtained.state;

            assert(obtained.block != nullptr);
            Slice<u8> block_data = data(obtained.block);
            Slice<u8> total_block_data = total_data(obtained.block);

            push_chain(&used_chain, Chain{obtained.block, obtained.block});
            used_blocks ++;

            availible_from = block_data.data;
            availible_to = block_data.data + block_data.size;
            
            last_allocation = dummy_data;
            last_alloced_size = 0;

            max_used_block_size = max(max_used_block_size, total_block_data.size);

            if(obtained.was_alloced)
            {
                max_free_block_size = min(max_free_block_size, total_block_data.size);
                total_bytes_used += total_block_data.size;
            }
            else
            {
                free_blocks --;
            }

            max_used_blocks = max(max_used_blocks, used_blocks);
            assert(is_invariant());

            return obtained.state;
        }

        bool 
        is_invariant() const noexcept
        {
            bool available_inv1 = availible_from <= availible_to;
            bool available_inv2 = (availible_from != nullptr) == (availible_to != nullptr);
            bool last_alloc_inv = (last_allocation == dummy_data) == (last_alloced_size == 0) && last_allocation != nullptr;
            bool used_list_inv = (used_chain.first == nullptr) == (used_blocks == 0);
            bool free_list_inv = (free_chain.first == nullptr) == (free_blocks == 0);

            bool parent_inv = parent != nullptr;

            bool positivity_inv = used_blocks >= 0 
                && total_bytes_used >= 0 
                && total_bytes_alloced >= 0 
                && max_bytes_alloced >= 0;

            bool block_size_inv = chunk_size > sizeof(Block);

            bool conectivity_inv1 = is_valid_chain(used_chain);
            bool conectivity_inv2 = is_valid_chain(free_chain);
            bool stat_inv = total_bytes_used >= total_bytes_alloced;

            bool total_inv = 
                available_inv1 && available_inv2 && last_alloc_inv && 
                used_list_inv && free_list_inv && parent_inv && positivity_inv && 
                block_size_inv && conectivity_inv1 && conectivity_inv2 && stat_inv;

            return total_inv;
        }

        virtual Allocator_Result 
        allocate(isize size, isize align) noexcept 
        {
            assert(is_power_of_two(align));
            u8* aligned = align_forward(availible_from, align);
            u8* used_to = aligned + size;

            if(used_to > availible_to)
            {
                Allocator_State_Type state = obtain_block_and_update(size, align);
                if(state == ERROR)
                    return Allocator_Result{state};
                
                return allocate(size, align);
            }

            Slice<u8> alloced = Slice{aligned, size};
            availible_from = used_to;
            last_allocation = aligned;
            last_alloced_size = size;

            #ifndef SKIP_ALLOCATOR_STATS
            total_bytes_alloced += size;
            max_bytes_alloced = max(max_bytes_alloced, total_bytes_alloced);
            #endif

            return Allocator_Result{Allocator_State::OK, alloced};
        }

        virtual Allocator_State_Type 
        deallocate(Slice<u8> allocated, isize align) noexcept 
        {
            if(allocated != last_alloced_slice())
                return Allocator_State::OK;

            last_allocation = dummy_data;
            last_alloced_size = 0;

            #ifndef SKIP_ALLOCATOR_STATS
            total_bytes_alloced -= allocated.size;
            #endif

            assert(total_bytes_alloced >= 0);

            return Allocator_State::OK;
        } 

        virtual Allocator_Result 
        resize(Slice<u8> allocated, isize new_size) noexcept
        {
            if(new_size < allocated.size)
                return {Allocator_State::OK, {allocated.data, new_size}};

            u8* used_to = availible_from + new_size;
            if(allocated != last_alloced_slice() || used_to > availible_to)
                return {Allocator_State::NOT_RESIZABLE};

            availible_from = used_to;
            last_alloced_size = new_size;
            return {Allocator_State::OK, {allocated.data, new_size}};
        }

        virtual Nullable<Allocator*> 
        parent_allocator() const noexcept
        {
            return {parent};
        }

        virtual isize 
        bytes_allocated() const noexcept
        {
            return total_bytes_alloced;
        }

        virtual isize 
        bytes_used() const noexcept 
        {
            return total_bytes_used;    
        }

        void
        reset() noexcept
        {
            assert(is_invariant());

            availible_from = nullptr;
            availible_to = nullptr;
            last_allocation = dummy_data;
            last_alloced_size = 0;

            push_chain(&free_chain, used_chain);

            max_free_block_size = max(max_free_block_size, max_used_block_size);
            max_used_block_size = 0;
            free_blocks = free_blocks + used_blocks;
            used_blocks = 0;
            total_bytes_alloced = 0;

            used_chain.first = nullptr;
            used_chain.last = nullptr;

            assert(is_invariant());
        }

        virtual Allocator_Result 
        custom_action(
            Allocator_Action::Type action_type, 
            Nullable<Allocator*> other_alloc, 
            isize new_size, u8 new_align, 
            Slice<u8> allocated, u8 old_align, 
            Nullable<void*> custom_data) noexcept
        {
            if(action_type == Allocator_Action::RESET)
            {
                reset();
                return Allocator_Result{Allocator_State::OK};
            }

            return Allocator_Result{Allocator_State::UNSUPPORTED_ACTION};
        }
    };
}

#include "undefs.h"