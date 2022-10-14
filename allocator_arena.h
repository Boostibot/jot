#pragma once

#include "allocator.h"
#include "allocator_resource.h"
#include "block_list.h"
#include "defines.h"

namespace jot 
{

    struct Arena_Resource;

    runtime_proc allocate(Arena_Resource* resource, size_t size, size_t align) -> void*;
    runtime_proc deallocate(Arena_Resource* resource, void* ptr, size_t old_size, size_t align) -> void;
    runtime_proc resize(Arena_Resource* resource, void* ptr, size_t old_size, size_t new_size) -> bool;
    runtime_proc deallocate_all(Arena_Resource* resource) -> void;
    runtime_proc action(
        Arena_Resource* resource, 
        Allocator_Actions::Action action_type, 
        void* old_ptr, 
        size_t old_size, size_t new_size, 
        size_t old_align, size_t new_align, 
        void* custom_data = nullptr) -> Allocator_Actions::Result<void>;

    //func align_size(size_t current_pos, )

    struct Arena_Resource : Allocator_Resource
    {
        using Block_List = Block_List_<byte, size_t, Allocator>;
        using Block = Block<byte, size_t>;

        Allocator_Resource* upstream = new_delete_resource();
        Block_List blocks = Block_List{upstream};
        Block_List free_blocks = Block_List{upstream};
        size_t filled_to = 0;
        size_t chunk_size = 2097152; //2MiB
        byte* last_allocation = nullptr;

        Arena_Resource() = default;
        Arena_Resource(size_t chunk_size) 
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


    func align_forward(uintptr_t ptr_num, size_t align_to) -> uintptr_t
    {
        return div_round_up(ptr_num, align_to) * align_to;
    }

    runtime_func align_forward(void* ptr, size_t align_to) -> void*
    {
        return cast(void*)(align_forward(cast(uintptr_t) ptr, align_to));
    }

    runtime_proc allocate(Arena_Resource* resource, size_t byte_size, size_t align) -> void* 
    {
        assert(is_power_of_two(align));

        using Alloc = Arena_Resource;
        using Block_List = Alloc::Block_List;
        using Block = Alloc::Block;

        byte* block_from = nullptr;
        byte* block_to = nullptr;
        byte* filled_to_ptr = nullptr;
        byte* allocated_from = nullptr;
        byte* allocated_to = block_to + 1;

        let calc_all_ptrs = [&](Block* last_block) 
        {
            block_from = data(last_block);
            block_to = block_from + last_block->size;
            filled_to_ptr = block_from + resource->filled_to;
            allocated_from = cast(byte*) align_forward(filled_to_ptr, align);
            allocated_to = allocated_from + byte_size;
        };

        if(!is_empty(resource->blocks))
            calc_all_ptrs(resource->blocks.last);

        //will trigger even if all variables unitialized
        if(allocated_to > block_to)
        {
            size_t chunk_size = resource->chunk_size;
            size_t chunk_count = div_round_up(byte_size + align, chunk_size);
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
                Block_List popped = pop_block(&resource->free_blocks, found);
                push(&resource->blocks, move(popped));
            }

            calc_all_ptrs(resource->blocks.last);
        }

        assert(filled_to_ptr >= block_from);
        assert(allocated_from >= filled_to_ptr);
        assert(block_to >= allocated_to);

        resource->last_allocation = allocated_from;
        resource->filled_to = allocated_to - block_from;

        assert(resource->filled_to > 0);
        return resource->last_allocation;
    }

    runtime_proc deallocate(Arena_Resource* resource, void* ptr, size_t old_size, size_t align) -> void
    {
        assert(is_power_of_two(align));
        cast(void) resize(resource, ptr, old_size, 0);
    }

    runtime_proc resize(Arena_Resource* resource, void* ptr, size_t old_size, size_t new_size) -> bool 
    {
        byte* byte_ptr = cast(byte*) ptr;
        if(resource->last_allocation != byte_ptr)
            return false;

        mut* last_block = resource->blocks.last;
        mut* block_data = data(resource->blocks.last);

        let prev_offset = cast(size_t) (byte_ptr - block_data);

        if(prev_offset + new_size < last_block->size)
        {
            resource->filled_to = prev_offset + new_size;
            return true;
        }
        return false;
    }

    runtime_proc deallocate_all(Arena_Resource* resource) -> void
    {
        push(&resource->free_blocks, move(resource->blocks));
        resource->filled_to = 0;
        resource->last_allocation = nullptr;
    }

    runtime_proc action(
        Arena_Resource* resource, 
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
            case RESIZE: {
                bool ok = resize(resource, old_ptr, old_size, new_size);
                return Result{true, ok ? old_ptr : nullptr};
            }
        }
        return Result{false, nullptr};
    }

    struct Flat_Arena_Resource : Allocator_Resource
    {
        void* data;
        size_t size;
        size_t filled_to = 0;
        void* last_alloc = nullptr;

        using Result = Allocator_Actions::Result<void>;

        Flat_Arena_Resource() = delete; 
        Flat_Arena_Resource(void* data, size_t size)
            : data(data), size(size) {}

        runtime_proc do_allocate(size_t bytes, size_t alignment) -> void* override
        {
            size_t space = this->size - this->filled_to;
            void* ptr = cast(byte*) this->data + this->size;

            //this is such a horribly designed function...
            if(std::align(alignment, bytes, ptr, space) == nullptr)
                throw std::bad_alloc();

            this->last_alloc = ptr;
            return ptr;
        }
        runtime_proc do_deallocate(void* old_ptr, size_t bytes, size_t alignment) -> void override
        {
            cast(void) resize(old_ptr, bytes, 0);
        }

        runtime_proc resize(void* ptr, size_t old_size, size_t new_size) -> bool
        {
            if(ptr != this->last_alloc)
                return false;

            size_t start_index = cast(uintptr_t)this->data - cast(uintptr_t)ptr;
            if(start_index + new_size > this->size)
                return false;
            
            this->size = start_index + new_size;
            return true;
        }

        runtime_proc do_action(
            Allocator_Actions::Action action_type, 
            void* old_ptr, 
            size_t old_size, size_t new_size, 
            size_t old_align, size_t new_align, 
            void* custom_data = nullptr) -> Result override
        {
            using namespace Allocator_Actions;
            switch(action_type)
            {
                case DEALLOC_ALL: {
                    filled_to = 0;
                    last_alloc = nullptr;
                }
                case RESIZE: {
                    bool ok = resize(old_ptr, old_size, new_size);
                    return Result{true, ok ? old_ptr : nullptr};
                }
            }
            return Result{false, nullptr};
        }

        runtime_proc do_upstream_resource() const noexcept -> Allocator_Resource* override
        {
            return nullptr;
        }

        runtime_proc do_is_equal(std::pmr::memory_resource const& other) const noexcept -> bool override
        {
            //@TODO: use dynamic cast and compare data ptrs
            return cast(std::pmr::memory_resource*)(this) == &other;
        }
    };
}

#include "undefs.h"