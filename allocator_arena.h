#pragma once

#include "allocator.h"
#include "allocator_resource.h"
#include "block_list.h"
#include "defines.h"

namespace jot 
{

    struct Unbound_Arena_Resource;

    runtime_proc allocate(Unbound_Arena_Resource* resource, size_t size, size_t align) -> void*;
    runtime_proc deallocate(Unbound_Arena_Resource* resource, void* ptr, size_t old_size, size_t align) -> void;
    runtime_proc grow(Unbound_Arena_Resource* resource, void* ptr, size_t old_size, size_t new_size) -> bool;
    runtime_proc shrink(Unbound_Arena_Resource* resource, void* ptr, size_t old_size, size_t new_size) -> bool;
    runtime_proc deallocate_all(Unbound_Arena_Resource* resource) -> void;
    runtime_proc action(
        Unbound_Arena_Resource* resource, 
        Allocator_Actions::Action action_type, 
        void* old_ptr, 
        size_t old_size, size_t new_size, 
        size_t old_align, size_t new_align, 
        void* custom_data = nullptr) -> Allocator_Actions::Result<void>;

    struct Unbound_Arena_Resource : Allocator_Resource
    {
        using size_t = size_t;
        using Block_List = Block_List_<byte, size_t, Allocator>;
        using Block = Block<byte, size_t>;

        Allocator_Resource* upstream = new_delete_resource();
        Block_List blocks = Block_List{upstream};
        Block_List free_blocks = Block_List{upstream};
        size_t filled_to = 0;
        size_t chunk_size = 2097152; //2MiB
        byte* last_allocation = nullptr;

        Unbound_Arena_Resource() = default;
        Unbound_Arena_Resource(size_t chunk_size) 
        {
            this->chunk_size = chunk_size;
        }

        runtime_proc do_allocate(size_t bytes, size_t alignment) -> void* override
        {
            return jot::allocate(this, bytes, alignment);
        }
        runtime_proc do_deallocate(void* old_ptr, size_t bytes, size_t alignment) -> void override
        {
            return jot::deallocate(this, old_ptr, bytes, alignment);
        }
        runtime_proc do_action(
            Allocator_Actions::Action action_type, 
            void* old_ptr, 
            size_t old_size, size_t new_size, 
            size_t old_align, size_t new_align, 
            void* custom_data = nullptr) -> Result override
        {
            return action(this, action_type, old_ptr, old_size, new_size, old_align, new_align, custom_data);
        }

        runtime_proc do_upstream_resource() const noexcept -> Allocator_Resource* override
        {
            return upstream;
        }

        runtime_proc do_is_equal(std::pmr::memory_resource const& other) const noexcept -> bool override
        {
            return cast(std::pmr::memory_resource*)(this) == &other;
        }
    };

    runtime_proc allocate(Unbound_Arena_Resource* resource, size_t byte_size, size_t align) -> void* 
    {
        assert(align > 0);

        using Alloc = Unbound_Arena_Resource;
        using Block_List = Alloc::Block_List;
        using Block = Alloc::Block;

        size_t filled_to = resource->filled_to;
        size_t from = div_round_up(filled_to, align) * align;
        size_t to = from + byte_size;
        size_t total_size = to - filled_to;

        mut* last_block = resource->blocks.last;

        if(is_empty(resource->blocks) || to > last_block->size)
        {
            assert(total_size > 1);
            size_t chunk_size = resource->chunk_size;
            size_t chunk_count = (total_size + chunk_size - 1) / chunk_size;
            size_t total_alloced = chunk_count * chunk_size;

            Block* found = nullptr;
            for(mut& block : resource->free_blocks)
            {
                if(block.size >= total_alloced)
                {
                    found = &block;
                    break;
                }
            }

            if(found == nullptr)
                push(&resource->blocks, Block_List{total_alloced, resource->upstream}); 
            else
            {
                //Block_List popped{resource->do_upstream_resource()};
                //pop_block(&resource->free_blocks, popped, found);
                //push(&resource->blocks, move(popped));
            }

            last_block = resource->blocks.last;
            resource->filled_to = 0;
        }

        resource->last_allocation = data(last_block) + from;
        resource->filled_to += total_size;
        return resource->last_allocation;
    }

    runtime_proc deallocate(Unbound_Arena_Resource* resource, void* ptr, size_t old_size, size_t align) -> void
    {
        //attempts to free up 
        cast(void) shrink(resource, ptr, old_size, 0);
    }

    runtime_proc grow(Unbound_Arena_Resource* resource, void* ptr, size_t old_size, size_t new_size) -> bool 
    {
        byte* byte_ptr = cast(byte*) ptr;
        if(resource->last_allocation != byte_ptr)
            return false;

        mut* last_block = resource->blocks.last;
        mut* block_data = data(resource->blocks.last);

        let prev_offset = cast(size_t) (byte_ptr - block_data);
        let prev_size = resource->filled_to - prev_offset;

        assert(new_size >= prev_size);

        if(prev_offset + new_size < last_block->size)
        {
            resource->filled_to = prev_offset + new_size;
            return true;
        }
        return false;
    }

    runtime_proc shrink(Unbound_Arena_Resource* resource, void* ptr, size_t old_size, size_t new_size) -> bool 
    {
        byte* byte_ptr = cast(byte*) ptr;
        if(resource->last_allocation != byte_ptr)
            return false;

        mut* block_data = data(resource->blocks.last);

        let prev_offset = cast(size_t) (byte_ptr - block_data);
        let prev_size = resource->filled_to - prev_offset;

        assert(new_size <= prev_size);

        resource->filled_to = prev_offset + new_size;
        return true;
    }

    runtime_proc deallocate_all(Unbound_Arena_Resource* resource) -> void
    {
        push(&resource->free_blocks, move(resource->blocks));
        resource->filled_to = 0;
        resource->last_allocation = nullptr;
    }

    runtime_proc action(
        Unbound_Arena_Resource* resource, 
        Allocator_Actions::Action action_type, 
        void* old_ptr, 
        size_t old_size, size_t new_size, 
        size_t old_align, size_t new_align, 
        void* custom_data) -> Allocator_Actions::Result<void>
    {
        using namespace Allocator_Actions;
        using Result = Allocator_Actions::Result<void>;
        switch(action_type)
        {
            case DEALLOC_ALL: {
                deallocate_all(resource); 
                return Result{true, nullptr};
            }
            case GROW: {
                bool ok = grow(resource, old_ptr, old_size, new_size);
                return Result{true, ok ? old_ptr : nullptr};
            }
            case SHRINK: {
                bool ok = shrink(resource, old_ptr, old_size, new_size);
                return Result{true, ok ? old_ptr : nullptr};
            }
        }
        return Result{false, nullptr};
    }
}

#include "undefs.h"