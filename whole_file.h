#pragma once

#include "file.h"
#include "string.h"

namespace jot
{
    enum Whole_File_State
    {
        WHOLE_FILE_OK,
        WHOLE_FILE_OUT_OF_MEM,
        WHOLE_FILE_NOT_FOUND,
        WHOLE_FILE_READING_ERROR
    };

    Whole_File_State read_whole_file(String_Builder* appender, const char* path) noexcept
    {
        File descriptor = open(path);
        if(is_open(descriptor) == false)
            return WHOLE_FILE_NOT_FOUND;
        
        File_Stats stats = get_stats(descriptor);
        isize needed_size = stats.st_size;
        if(reserve_failing(appender, needed_size) == false)
            return WHOLE_FILE_OUT_OF_MEM;
        
        resize_for_overwrite(appender, needed_size);
        File_IO_Result read_state = read(&descriptor, data(appender), size(appender));

        resize_for_overwrite(appender, read_state.processed_size);
        if(read_state.ok == false || read_state.processed_size != needed_size)
            return WHOLE_FILE_READING_ERROR;
        else
            return WHOLE_FILE_OK;
    }
}
