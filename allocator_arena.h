#pragma once

#include "allocator.h"
#include "stack.h"
#include "defines.h"

namespace jot 
{
    struct Unbound_Arena_Resource : Allocator_Resource
    {
        using Block = Stack<u8>;
        
        Slice<u8> active_block = {dummy_storage + 8, 0}; 
        tsize active_block_used = 0;
        u8* last_allocation = nullptr;
        u8* last_unaligned = nullptr;
        Stack<Block, 8> blocks;

        tsize used_blocks = 0;
        Allocator_Resource* parent = DEFAULT_RESOURCE;
        tsize chunk_size = 2097152; //2MiB
        //so that we dont ever return nullptr even when we dont have any memory
        // and big enough so that when someone (me) references out of it it wont change this struct
        u8 dummy_storage[16] = {0}; 

        tsize ideal_total_used = 0; //the real size of all allocations combined (not including alignemnt and block rounding)
        tsize current_total_used = 0; //the real size of all allocations combined 
        tsize max_used = 0;
        tsize max_ideal_used = 0;
        tsize max_used_blocks = 0;
        tsize max_single_alloc = 0; //including alignment

        Unbound_Arena_Resource() = default;
        Unbound_Arena_Resource(size_t chunk_size, Allocator_Resource* parent = DEFAULT_RESOURCE) 
            : blocks(Poly_Allocator{parent}), parent(parent), chunk_size(chunk_size)
        {}

        Slice<u8>
        available_slice() const 
        {
            return slice(active_block, active_block_used);
        }

        virtual Alloc_Result 
        do_allocate(Alloc_Info info) noexcept
        {
            assert(blocks.size >= used_blocks);
            assert(is_power_of_two(info.align));

            Slice<u8> available = available_slice();
            Slice<u8> aligned = align_forward(available, info.align);

            //unhappy path guard
            if(aligned.size < info.byte_size)
                return add_block_and_allocate(info);

            Slice<u8> alloced = trim(aligned, info.byte_size);
            last_allocation = alloced.data;
            last_unaligned = available.data;

            tsize total_alloced_bytes = alloced.data + alloced.size - available.data;
            active_block_used += total_alloced_bytes;
            
            #ifndef SKIP_ALLOCATOR_STATS
            ideal_total_used += info.byte_size;
            current_total_used += total_alloced_bytes;
            max_used = max(max_used, current_total_used);
            max_ideal_used = max(max_ideal_used, current_total_used);
            max_single_alloc = max(max_single_alloc, total_alloced_bytes);
            #endif

            return Alloc_Result{Alloc_State::OK, alloced};
        }

        Alloc_Result
        add_block_and_allocate(Alloc_Info info) noexcept
        {
            assert(is_invarinat());
            current_total_used += available_slice().size; //add the rounded off size
            tsize required_chunk_size = max(info.byte_size, chunk_size);
            Block created(Poly_Allocator{parent});

            auto big_alloc_res = resize_for_overwrite(&created, required_chunk_size);
            if(big_alloc_res == Error())
                return Alloc_Result{big_alloc_res};

            auto insert_res = unordered_insert(&blocks, used_blocks, move(&created));
            if(insert_res == Error())
                return Alloc_Result{insert_res};

            active_block = slice(&blocks[used_blocks]);
            used_blocks ++;
            active_block_used = 0;
            last_allocation = nullptr;
            last_unaligned = nullptr;

            max_used_blocks = max(used_blocks, max_used_blocks);
            assert(is_invarinat());

            return do_allocate(info);
        }

        bool 
        was_last_alloced_slice(Slice<u8> old_slice) const noexcept
        {
            if(old_slice.data != last_allocation)
                return false;

            Slice<u8> available = available_slice();
            if(old_slice.data + old_slice.size != available.data)
                return false;
            
            assert(are_aliasing<u8>(active_block, old_slice));
            return true;
        }

        bool 
        is_invarinat() const noexcept
        {
            bool last_alloc_inv = cast(uintptr_t) last_unaligned <= cast(uintptr_t) last_allocation;
            bool nullptr_inv = true;
            if(last_unaligned == nullptr || last_allocation == nullptr)
                nullptr_inv = last_unaligned == last_allocation;

            bool chunk_size_inv = chunk_size > 0;
            bool active_block_inv = active_block.data != nullptr && active_block_used <= active_block.size;
            bool used_blocks_inv = used_blocks <= blocks.size;
            bool parent_inv = parent != nullptr;
            bool stats_inv = ideal_total_used >= 0 
                && current_total_used >= 0
                && max_used >= 0
                && max_ideal_used >= 0
                && max_used_blocks >= 0
                && max_single_alloc >= 0;

            bool total_inv = last_alloc_inv && nullptr_inv && chunk_size_inv && active_block_inv && used_blocks_inv && parent_inv && stats_inv;
            return total_inv;
        }

        virtual bool
        do_deallocate(Slice<u8> old_slice, Alloc_Info old_info) noexcept
        {
            assert(old_slice.size == old_info.byte_size && "data must be consistent");

            if(was_last_alloced_slice(old_slice) == false)
                return true;

            //maybe just use the last_allocation instead of last_unaligned cause
            // chances are you are going to align it back anyway
            tsize total_dealloced_bytes = cast(ptrdiff_t) last_unaligned - cast(ptrdiff_t) last_allocation;
            active_block_used -= total_dealloced_bytes;
            last_allocation = nullptr;
            last_unaligned = nullptr;
            
            #ifndef SKIP_ALLOCATOR_STATS
            ideal_total_used -= old_slice.size;
            current_total_used -= total_dealloced_bytes;
            #endif

            assert(ideal_total_used >= 0);
            assert(current_total_used >= 0);
            assert(active_block_used >= 0);

            return true;
        }

        virtual bool 
        do_is_alloc_equal(Allocator_Resource const& other) const noexcept {
            return &other == this;
        }; 

        virtual Allocator_Resource* 
        do_parent_resource() const noexcept { 
            return parent; 
        }

        void
        deallocate_all() noexcept
        {
            active_block = {dummy_storage + 8, 0}; 
            active_block_used = 0;
            last_allocation = nullptr;
            last_unaligned = nullptr;
            used_blocks = 0;
        }
        
        Alloc_Result
        resize_allocation(Slice<u8> prev, Alloc_Info new_, Alloc_Info old_) noexcept
        {
            if(new_.byte_size < old_.byte_size)
                return {Alloc_State::OK, {prev.data, new_.byte_size}};
                
            if(was_last_alloced_slice(prev) == false)
                return {Alloc_State::ERROR};

            Slice<u8> available = available_slice();
            tsize new_filled_to = cast(ptrdiff_t) last_allocation - cast(ptrdiff_t) active_block.data + new_.byte_size;
            if(new_.align != old_.align || new_.byte_size < 0 || new_filled_to > active_block.size)
                return {Alloc_State::ERROR};

            active_block_used = new_filled_to;
            return {Alloc_State::OK, {prev.data, new_.byte_size}};
        }

        virtual Alloc_Result 
        do_action(
            Alloc_Action action_type, 
            Option<Allocator_Resource*> other_alloc, 
            Slice<u8> prev, 
            Alloc_Info new_, 
            Alloc_Info old_, 
            Option<void*> custom_data) noexcept
        {
            using namespace Alloc_Actions;
            if(action_type == RESIZE)
                return resize_allocation(prev, new_, old_);

            if(action_type == DEALLOCATE_ALL)
            {
                deallocate_all();
                return Alloc_Result{Alloc_State::OK};
            }

            return Alloc_Result{Alloc_State::UNSUPPORTED_ACTION};
        }
    };
}

#include "undefs.h"