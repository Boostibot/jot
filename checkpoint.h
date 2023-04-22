#pragma once

#include <setjmp.h>
#include <string.h>
#include <stdint.h>

namespace jot
{
    #ifndef GET_LINE_INFO
    #ifndef _MSC_VER
        #define __FUNCTION__ __func__
    #endif

    struct Line_Info
    {
        const char* file = "";
        const char* func = "";
        ptrdiff_t line = -1;
    };
    
    #define GET_LINE_INFO() ::jot::Line_Info{__FILE__, __FUNCTION__, __LINE__}
    #endif

    struct Jump_State
    {
        uint32_t flag;
        Line_Info from;
        void* data;
        bool do_deallocate_data;
        bool did_jump;
    };

    namespace checkpoint_globals
    {
        inline Jump_State* jump_state_ptr()
        {
            thread_local static Jump_State from = {};
            return &from;
        }

        inline jmp_buf* jump_buf_ptr()
        {
            thread_local static jmp_buf jump_buffer = {};
            return &jump_buffer;
        }
    }

    struct Checkpoint 
    {
        jmp_buf old_jump_buffer;

        Checkpoint() noexcept
        {
            memmove(old_jump_buffer, *checkpoint_globals::jump_buf_ptr(), sizeof(jmp_buf));
        }
        
        ~Checkpoint() noexcept
        {
            memmove(*checkpoint_globals::jump_buf_ptr(), old_jump_buffer, sizeof(jmp_buf));
        }
    };
    
    static void jump_to_checkpoint(Jump_State state)
    {
        *checkpoint_globals::jump_state_ptr() = state;
        longjmp(*checkpoint_globals::jump_buf_ptr(), 1);
    }

    static Jump_State checkpoint(Checkpoint* current)
    {
        (void) current; //this is just so the user is forced to actually make one
        Jump_State out = {};
        if (setjmp(*checkpoint_globals::jump_buf_ptr()) != 0)
        {
            out = *checkpoint_globals::jump_state_ptr();
            out.did_jump = true;
        }
        else
            out.from = GET_LINE_INFO();
        
        return out;
    }

    #ifdef CHECKPOINT_EXAMPLE
    static void checkpoint_example()
    {
        Malloc_Allocator allocator;
        File_Descriptor descriptors[64] = {0};
        
        memory_globals::Allocator_Swap alloc_swap(&allocator);
        file_globals::File_Guard_Swap file_swap(descriptors, 64);

        void* jump_data = nullptr;
        Line_Info jump_from = GET_LINE_INFO();

        Checkpoint current;
        int flag = checkpoint(&current, &jump_data, &jump_from);
        if(flag == 0) 
        {
            //do user code...
            my_maybe_breaking_fn();
        }
        else
        {
            //Handle errors
            printf("Error occured (jump) with flag %d from file %s at line %t", flag, jump_from.file, jump_from.line);
        }
    }
    #endif
}