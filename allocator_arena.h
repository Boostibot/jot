#pragma once

#include "allocator.h"
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

    struct Unbound_Stack_Allocator : Scratch_Allocator
    {
        static constexpr isize MAGIC_NUMBER = 0x0ABCDEF0;
        #if defined(DO_SNAPSHOT_VALIDTY_CHECKS) || defined(DO_ALLOCATOR_STATS)
            #define ALLOCATOR_UNBOUND_STACK_DO_STATS
        #endif

        struct Block
        {
            Block* next = nullptr;
            isize size = -1;
            u32 align = -1;
            b32 was_alloced = false;
        };
        
        struct Chain
        {
            Block* first = nullptr;
            Block* last = nullptr;
        };

        struct Snapshot_Data
        {
            Block* from_block;
            u8* available_from;

            #ifdef ALLOCATOR_UNBOUND_STACK_DO_STATS
            isize magic_number = MAGIC_NUMBER;
            isize bytes_alloced = 0;
            #endif // ALLOCATOR_UNBOUND_STACK_DO_STATS
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

        u8* available_from = nullptr;
        u8* available_to = nullptr;
        Slice<u8> last_allocation = {dummy_data, 0};

        Chain blocks;
        Block* current_block = nullptr;

        Allocator* parent = allocator_globals::DEFAULT;
        isize chunk_size = ALLOCATOR_UNBOUND_STACK_DEF_SIZE;
        isize chunk_grow = ALLOCATOR_UNBOUND_STACK_DEF_GROW;

        isize used_blocks = 0;
        isize max_used_blocks = 0;

        isize bytes_alloced_ = 0; 
        isize bytes_used_ = 0;
        isize max_bytes_alloced_ = 0;
        isize max_bytes_used_ = 0;

        u8 dummy_data[8] = {0};

        Unbound_Stack_Allocator(
            Allocator* parent = allocator_globals::DEFAULT, 
            size_t chunk_size = ALLOCATOR_UNBOUND_STACK_DEF_SIZE,
            size_t chunk_grow = ALLOCATOR_UNBOUND_STACK_DEF_GROW) 
            : parent(parent), chunk_size(chunk_size), chunk_grow(chunk_grow)
        {
            reset_last_allocation();
            assert(is_invariant());
        }

        ~Unbound_Stack_Allocator()
        {
            assert(is_invariant());

            isize dealloced_bytes = 0;
            Block* current = blocks.first;
            Block* prev = nullptr;
            while(current != nullptr && prev != blocks.last)
            {
                prev = current;
                current = current->next;

                Slice<u8> total_block_data = used_by_block(prev);
                dealloced_bytes += total_block_data.size;

                if(prev->was_alloced)
                    parent->deallocate(total_block_data, prev->align);
            }

            assert(prev == blocks.last && "must be a valid chain!");
            assert(dealloced_bytes == bytes_used_);
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
            return Chain{current_block->next, blocks.first};
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

        virtual void 
        deallocate(Slice<u8> allocated, isize align) noexcept override 
        {
            if(allocated != last_allocation)
                return;

            reset_last_allocation();

            #ifdef ALLOCATOR_UNBOUND_STACK_DO_STATS
            bytes_alloced_ -= allocated.size;
            assert(bytes_alloced_ >= 0);
            #endif
        } 

        virtual Allocation_Result 
        resize(Slice<u8> allocated, isize new_size) noexcept override
        {
            u8* used_to = available_from + new_size;
            if(allocated != last_allocation || used_to > available_to)
            {
                if(new_size < allocated.size)
                    return Allocation_Result{Allocator_State::OK, trim(allocated, new_size)};

                return Allocation_Result{Allocator_State::NOT_RESIZABLE};
            }

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

        virtual Snapshot_Result 
        snapshot() noexcept override 
        {
            Allocation_Result result = allocate(sizeof(Snapshot_Data), alignof(Snapshot_Data));
            if(result.state == ERROR)
                return {result.state};

            Snapshot_Data* data = cast(Snapshot_Data*) cast(void*) result.items.data;

            #ifdef ALLOCATOR_UNBOUND_STACK_DO_STATS
                *data = Snapshot_Data{current_block, available_from, MAGIC_NUMBER, bytes_alloced_};
            #else
                *data = Snapshot_Data{current_block, available_from};
            #endif

            Allocator_Snapshot snapshot = Allocator_Snapshot{cast(isize) data};
            return Snapshot_Result{Allocator_State::OK, snapshot};
        }

        void
        reset(Allocator_Snapshot snapshot) noexcept
        {
            assert(is_invariant());

            Snapshot_Data* snapshot_data = cast(Snapshot_Data*) cast(void*) cast(isize) snapshot;

            assert(snapshot_data->magic_number == MAGIC_NUMBER && "invalid snapshot");
            assert(snapshot_data->bytes_alloced <= bytes_alloced_ && "invalid snapshot");

            reset_last_allocation();
            if(blocks.first == nullptr)
            {
                assert(snapshot_data->from_block == nullptr);
                assert(snapshot_data->available_from == nullptr);
                assert(snapshot_data->bytes_alloced == 0);

                return;
            }

            if(snapshot_data->from_block == nullptr)
                current_block = blocks.first;
            else
                current_block = snapshot_data->from_block;

            Slice<u8> block_data = data(current_block);
            available_from = snapshot_data->available_from;
            available_to = block_data.data + block_data.size;
            bytes_alloced_ = snapshot_data->bytes_alloced;

            assert(is_invariant());
        }

        
        static Block*
        extract_node(Chain* from, Block* extract_after, Block* what) noexcept
        {
            assert(is_valid_chain(*from));
            assert(what != nullptr);
            assert(from->first != nullptr && "cant extract from empty chain");

            //if is start of chain
            if(extract_after == nullptr)
            {
                from->first = what->next;
            }
            //if is end of chain
            else if(what == from->last)
            {
                from->last = extract_after;
                extract_after->next = what->next;
            }
            else
            {
                extract_after->next = what->next;
            }

            if(from->first == nullptr || from->last == nullptr)
            {
                from->first = nullptr;
                from->last = nullptr;
            }
            assert(is_valid_chain(*from));

            what->next = nullptr;
            return what;
        }

        static void
        insert_node(Chain* to, Block* insert_after, Block* what) noexcept
        {
            assert(is_valid_chain(*to));
            assert(what != nullptr);

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
                what->next = to->first->next;
                to->first = what;
            }
            //if is end of chain
            else if(insert_after == to->last)
            {
                to->last = what;
                insert_after->next = what;
                what->next = nullptr;
            }
            else
            {
                what->next = insert_after->next;
                insert_after->next = what;
            }

            assert(is_valid_chain(*to));
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

        void 
        reset_last_allocation() noexcept 
        {
            last_allocation = Slice<u8>{dummy_data, 0};
        }

        struct Found_Block
        {
            Block* found;
            Block* before;
        };

        static Found_Block
        find_block_to_fit(Chain chain, isize size, isize align) noexcept
        {
            Block* current = chain.first;
            Block* prev = nullptr;
            while(current != nullptr)
            {
                Slice<u8> block_data = data(current);
                Slice<u8> aligned = align_forward(block_data, align);
                if(aligned.size >= size)
                    return Found_Block{prev, current};

                prev = current;
                current = current->next;
            }

            return Found_Block{nullptr, nullptr};
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
            Found_Block found = find_block_to_fit(free_chain(), size, align);
            if(found.found == nullptr)
                return allocate_block(size, align);

            Block* extracted = extract_node(&blocks, found.before, found.found);
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
            *block = Block{nullptr, required_size - cast(isize) sizeof(Block), cast(u32) required_align, true};

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
            #ifdef ALLOCATOR_UNBOUND_STACK_DO_STATS
            bytes_alloced_ += delta;
            max_bytes_alloced_ = max(max_bytes_alloced_, bytes_alloced_);
            assert(bytes_alloced_ >= 0);
            #endif
        }

    };
}

#include "undefs.h"