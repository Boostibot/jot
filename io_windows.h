
#pragma once
#include "io.h"

#include <string.h>
#include <assert.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stringapiset.h>
//@TODO: remove this as a dependency!x
#include <direct.h>

#ifdef JOT_IO_C
#define JOT_IO_BEGIN 
#define JOT_IO_END 
#define CREF 
#else
#define JOT_IO_BEGIN namespace jot{
#define JOT_IO_END   }
#define CREF const&
#endif

JOT_IO_BEGIN

//Local buffers are used to reduce allocations. 
//Note that allocations might still accur if the size is not sufficient!
#define IO_LOCAL_BUFFER_SIZE 1024
    
typedef struct WIO_String
{
    char* data;
    int64_t size;
    int64_t capacity;
    char* buffer;
    int64_t buffer_size;
} WIO_String;
    
typedef struct WIO_w_String
{
    wchar_t* data;
    int64_t size;
    int64_t capacity;
    wchar_t* buffer;
    int64_t buffer_size;
} WIO_w_String;

//We assume allocations never fail and if they do we just abort the entire program. If you wish to handle it
// differently feel free to change this function
static void* wio_sure_realloc(void* allocated, int64_t size)
{
    if(size == 0)
    {
        free(allocated);
        return NULL;
    }

    void* reallocated = realloc(allocated, (size_t) size);
    if(reallocated == NULL)
    {
        fprintf(stderr, "io.h allocation failed! Attempted to allocate %lld bytes\n", (long long) size);
        abort();
    }

    return reallocated;
}
    
typedef enum Normalize_Flags
{
    IO_NORMALIZE_WINDOWS = 1,
    IO_NORMALIZE_LINUX = 2,
    IO_NORMALIZE_LONG = 4,
    IO_NORMALIZE_DIRECTORY = 8,
    IO_NORMALIZE_FILE = 16,
    IO_NORMALIZE_NO_RESIZE = 32,
} Normalize_Flags;
    
static void wio_string_free(WIO_String* string);
static void wio_string_resize(WIO_String* string, int64_t size);
    
static void wio_w_string_free(WIO_w_String* string);
static void wio_w_string_resize(WIO_w_String* string, int64_t size);
    
static void wio_utf16_to_utf8(const wchar_t* utf16, int64_t utf16len, WIO_String* output);
static void wio_utf8_to_utf16(const char* utf8, int64_t utf8len, WIO_w_String* output);
    
static void wio_w_concat(const wchar_t* a, const wchar_t* b, const wchar_t* c, WIO_w_String* output);
static void wio_normalize_allocate_path(const char* path, int64_t path_size, int flags, WIO_String* output);
static WIO_w_String wio_normalize_convert_to_utf16_path(const char* path, int64_t path_size, int flags, wchar_t* buffer, int64_t buffer_size);
static WIO_String   wio_convert_to_utf8_normalize_path(const wchar_t* path, int64_t path_size, int flags, char* buffer, int64_t buffer_size);
    
HANDLE* wio_get_file_handle(File* file)
{
    return (HANDLE*) (void*) file->state;
}

HANDLE wio_get_file_handle_val(File CREF file)
{
    return *(HANDLE*) (void*) file.state;
}
    
#ifndef JOT_IO_C
File::File()
{
    memset(this, 0, sizeof(*this));
    *wio_get_file_handle(this) = INVALID_HANDLE_VALUE;
}
    
File::File(File && other) noexcept
{
    memset(this, 0, sizeof(*this));
    *wio_get_file_handle(this) = *wio_get_file_handle(&other);
    *wio_get_file_handle(&other) = INVALID_HANDLE_VALUE;
}
    
File& File::operator =(File&& other) noexcept
{
    HANDLE temp = *wio_get_file_handle(&other);
    *wio_get_file_handle(&other) = *wio_get_file_handle(this);
    *wio_get_file_handle(this) = temp;
    return *this;
}

File::~File() noexcept
{
    file_close(this);
}
#endif

File file_open(const char* path, int64_t path_size, int open_mode)
{       
    wchar_t normalized_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    WIO_w_String normalized_path = wio_normalize_convert_to_utf16_path(path, path_size, 0, normalized_path_buffer, IO_LOCAL_BUFFER_SIZE);
        
    DWORD access = 0;
    if(open_mode & FILE_OPEN_READ)
        access |= GENERIC_READ;
    if(open_mode & FILE_OPEN_WRITE)
        access |= GENERIC_WRITE;

    DWORD sharing = 0;
    if(open_mode & FILE_OPEN_ALLOW_OTHER_READ)
        sharing |= FILE_SHARE_READ;
    if(open_mode & FILE_OPEN_ALLOW_OTHER_WRITE)
        sharing |= FILE_SHARE_WRITE;
    if(open_mode & FILE_OPEN_ALLOW_OTHER_DELETE)
        sharing |= FILE_SHARE_DELETE;

    DWORD disposition = 0;
    if(open_mode & FILE_OPEN_CREATE)
        disposition |= OPEN_ALWAYS;
    else
        disposition = OPEN_EXISTING;

    File f;
    f.open_mode = open_mode;
    *wio_get_file_handle(&f) = CreateFileW(normalized_path.data, access, sharing, NULL, disposition, 0, NULL);

    wio_w_string_free(&normalized_path);
    return f;
}

void file_close(File* file)
{
    HANDLE handle = *wio_get_file_handle(file);
    CloseHandle(handle);
    *wio_get_file_handle(file) = INVALID_HANDLE_VALUE;
}
    
bool file_is_open(File CREF file)
{
    return wio_get_file_handle_val(file) != INVALID_HANDLE_VALUE;
}

File_IO_Result file_read(File* file, void* read_into, int64_t size)
{
    File_IO_Result result = {0};
    HANDLE handle = *wio_get_file_handle(file);
    result.file_closed = !file_is_open(*file);
    if(result.file_closed)
        return result;

    int64_t total_read = 0;
    while(total_read < size)
    {
        int64_t remaining_size = size - total_read;
        DWORD to_read = (DWORD) (remaining_size & 0x8FFFFF);
        DWORD read = 0;
        result.error = (int) !ReadFile(handle, (char*) read_into + total_read, to_read, &read, NULL);
            
        if(result.error)
            break;

        total_read += read;
        // Check for eof
        if (read == 0) 
        {
            result.eof = true;
            break;
        }
    }

    return result;
}
    
File_IO_Result file_write(File* file, const void* write_from, int64_t size)
{
    File_IO_Result result = {0};
    HANDLE handle = *wio_get_file_handle(file);
    result.file_closed = !file_is_open(*file);
    if(result.file_closed)
        return result;

    int64_t total_written = 0;
    while(total_written < size)
    {
        int64_t remaining_size = size - total_written;
        DWORD to_write = (DWORD) (remaining_size & 0x8FFFFF);
        DWORD written = 0;
        result.error = (int) !ReadFile(handle, (char*) write_from + total_written, to_write, &written, NULL);
            
        if(result.error)
            break;

        total_written += written;
    }

    return result;
}

//Offsets the current possition in file by offset relative to from
bool file_seek(File* file, int64_t offset, File_Seek from)
{
    if(!file_is_open(*file))
        return false;
            
    HANDLE handle = *wio_get_file_handle(file);
    LARGE_INTEGER offset_;
    offset_.QuadPart = offset;
    DWORD move_method = 0;
    if(from == FILE_SEEK_START)
    {
        move_method = 0;

        //file_tell fails by returning negative
        // so this makes this function fail instead of it
        // reducing the points of failiure
        if(offset < 0)
            return false;
    }
    if(from == FILE_SEEK_CURRENT)
        move_method = 1;
    if(from == FILE_SEEK_END)
        move_method = 2;

    SetFilePointerEx(handle, offset_, NULL, move_method);
    return true;
}

//Returns the current position in file relative to start of the file. This can be used as argument to file_seek
int64_t file_tell(File CREF file)
{
    if(!file_is_open(file))
        return -1;
            
    HANDLE handle = wio_get_file_handle_val(file);
    LARGE_INTEGER move;
    LARGE_INTEGER curr_offset;
    move.QuadPart = 0;
    curr_offset.QuadPart = 0;
        
    bool state = !!SetFilePointerEx(handle, move, &curr_offset, 1);
    if(!state)
        return -1;

    return (int64_t) curr_offset.QuadPart;
}
    
bool file_create(const char* file_path, int64_t path_size)
{       
    File f = file_open(file_path, path_size, FILE_OPEN_READ | FILE_OPEN_CREATE_ELSE_FAIL);
    bool state = file_is_open(f);
    file_close(&f);
    return state;
}

bool file_remove(const char* file_path, int64_t path_size)
{       
    #define IO_NORMALIZE_PATH_OP(path, path_size, execute)  \
        wchar_t normalized_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; \
        WIO_w_String normalized_path = wio_normalize_convert_to_utf16_path(path, path_size, 0, normalized_path_buffer, IO_LOCAL_BUFFER_SIZE); \
        \
        execute\
        \
        wio_w_string_free(&normalized_path); \

    IO_NORMALIZE_PATH_OP(file_path, path_size,
        SetFileAttributesW(normalized_path.data, FILE_ATTRIBUTE_NORMAL);
        bool state = !!DeleteFileW(normalized_path.data);
    )
    return state;
}

bool file_move(const char* new_path, int64_t new_path_size, const char* old_path, int64_t old_path_size)
{       
    wchar_t new_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    wchar_t old_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    WIO_w_String new_path_norm = wio_normalize_convert_to_utf16_path(new_path, new_path_size, 0, new_path_buffer, IO_LOCAL_BUFFER_SIZE);
    WIO_w_String old_path_norm = wio_normalize_convert_to_utf16_path(old_path, old_path_size, 0, old_path_buffer, IO_LOCAL_BUFFER_SIZE);
        
    bool state = !!MoveFileExW(old_path_norm.data, new_path_norm.data, MOVEFILE_COPY_ALLOWED);

    wio_w_string_free(&new_path_norm);
    wio_w_string_free(&old_path_norm);
    return state;
}

bool file_copy(const char* copy_to_path, int64_t to_path_size, const char* copy_from_path, int64_t from_path_size)
{
    wchar_t to_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    wchar_t from_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    WIO_w_String to_path_norm = wio_normalize_convert_to_utf16_path(copy_to_path, to_path_size, 0, to_path_buffer, IO_LOCAL_BUFFER_SIZE);
    WIO_w_String from_path_norm = wio_normalize_convert_to_utf16_path(copy_from_path, from_path_size, 0, from_path_buffer, IO_LOCAL_BUFFER_SIZE);
        
    bool state = !!CopyFileW(from_path_norm.data, to_path_norm.data, true);

    wio_w_string_free(&to_path_norm);
    wio_w_string_free(&from_path_norm);
    return state;
}

static time_t wio_filetime_to_time_t(FILETIME ft)  
{    
    ULARGE_INTEGER ull;    
    ull.LowPart = ft.dwLowDateTime;    
    ull.HighPart = ft.dwHighDateTime;    
    return (time_t) (ull.QuadPart / 10000000ULL - 11644473600ULL);  
}

bool file_info(const char* file_path, int64_t path_size, File_Info* info)
{    
    WIN32_FILE_ATTRIBUTE_DATA native_info;
    memset(&native_info, 0, sizeof(native_info)); 
    memset(info, 0, sizeof(*info)); 
    IO_NORMALIZE_PATH_OP(file_path, path_size, 
        bool state = !!GetFileAttributesExW(normalized_path.data, GetFileExInfoStandard, &native_info);
    )   
    if(!state)
        return false;
            
    info->created_time = wio_filetime_to_time_t(native_info.ftCreationTime);
    info->last_access_time = wio_filetime_to_time_t(native_info.ftLastAccessTime);
    info->last_write_time = wio_filetime_to_time_t(native_info.ftLastWriteTime);
    info->size = ((int64_t) native_info.nFileSizeHigh << 32) | ((int64_t) native_info.nFileSizeLow);
        
    if(native_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        info->type = FILE_TYPE_DIRECTORY;
    else
        info->type = FILE_TYPE_FILE;

    return state;
}
    
bool directory_create(const char* dir_path, int64_t path_size)
{
    IO_NORMALIZE_PATH_OP(dir_path, path_size, 
        bool state = !!CreateDirectoryW(normalized_path.data, NULL);
    )
    return state;
}

bool directory_remove(const char* dir_path, int64_t path_size)
{
    IO_NORMALIZE_PATH_OP(dir_path, path_size, 
        bool state = !!RemoveDirectoryW(normalized_path.data);
    )
    return state;
}

bool directory_move(const char* new_path, int64_t new_path_size, const char* old_path, int64_t old_path_size)
{
    return file_move(new_path, new_path_size, old_path, old_path_size);
}
    
static WIO_String wio_malloc_full_path(WIO_w_String* local_path, int flags)
{
    wchar_t full_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
        
    WIO_w_String full_path = {0};
    full_path.buffer = full_path_buffer;
    full_path.buffer_size = IO_LOCAL_BUFFER_SIZE;

    wio_w_string_resize(&full_path, full_path.buffer_size - 1);

    int64_t needed_size = GetFullPathNameW(local_path->data, (DWORD) full_path.size, full_path.data, NULL);
    if(needed_size > full_path.size)
    {
        wio_w_string_resize(&full_path, needed_size + 1);
        needed_size = GetFullPathNameW(local_path->data, (DWORD) full_path.size, full_path.data, NULL);
    }
    wio_w_string_resize(&full_path, needed_size);
    WIO_String out_string = wio_convert_to_utf8_normalize_path(full_path.data, full_path.size, flags, NULL, 0);
        
    wio_w_string_free(&full_path);
    return out_string;
}

//File_Stats file_stats(const char* file_path, int64_t path_size);
int64_t directory_list_contents_malloc(const char* directory_path, int64_t path_size, Directory_Entry** entries)
{
    if(directory_path == NULL)
        return 0;

    assert(entries != NULL);
        
    wchar_t built_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    char normalized_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
        
    WIO_w_String built_path = {0};
    built_path.buffer = built_path_buffer;
    built_path.buffer_size = IO_LOCAL_BUFFER_SIZE;

    WIO_String normalized_path = {0};
    normalized_path.buffer = normalized_path_buffer;
    normalized_path.buffer_size = IO_LOCAL_BUFFER_SIZE;
        
    //wchar_t dir_mask_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0};
    //wIo
    wchar_t dir_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0};
    WIO_w_String dir_path = wio_normalize_convert_to_utf16_path(directory_path, path_size, IO_NORMALIZE_WINDOWS | IO_NORMALIZE_DIRECTORY, dir_path_buffer, IO_LOCAL_BUFFER_SIZE);

    int64_t entries_size = 0;
    int64_t entries_capacity = 8;
    Directory_Entry* local_entries = (Directory_Entry*) wio_sure_realloc(NULL, (int64_t) sizeof(Directory_Entry) * (entries_capacity+1));

    assert(dir_path.size != 0);
    //Specify a file mask. *.* = We want everything! 
    wio_w_concat(dir_path.data, L"\\*.*", NULL, &built_path);
        
    WIN32_FIND_DATAW found_file;
    HANDLE first_found = FindFirstFileW(built_path.data, &found_file);
    if(first_found != INVALID_HANDLE_VALUE) 
    {
        do
        { 
            //Find first file will always return "."
            //    and ".." as the first two directories. 
            if(wcscmp(found_file.cFileName, L".") == 0
                || wcscmp(found_file.cFileName, L"..") == 0)
                continue;
                    
            //Is the entity a File or Folder? 
            bool is_directory = !!(found_file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
            int flag = IO_NORMALIZE_LINUX;
            if(is_directory)
                flag |= IO_NORMALIZE_DIRECTORY;
            else
                flag |= IO_NORMALIZE_FILE;

            //Build up our file path using the passed in 
            //  [dir_path] and the file/foldername we just found: 
            wio_w_concat(dir_path.data, L"\\", found_file.cFileName, &built_path);
            WIO_String out_string = wio_malloc_full_path(&built_path, flag);
            //printf("found: \"%s\"\n", out_string.data);

            Directory_Entry entry = {0};
            entry.is_directory = is_directory;
            entry.path = out_string.data;
            entry.path_size = out_string.size;

            if(entries_size >= entries_capacity)
            {
                entries_capacity *= 2;
                local_entries = (Directory_Entry*) wio_sure_realloc(local_entries, (int64_t) sizeof(Directory_Entry) * (entries_capacity+1));
            }

            local_entries[entries_size++] = entry;
        } 
        while(FindNextFileW(first_found, &found_file)); //Find the next file. 
        FindClose(first_found); 
    }
        
    //null terminate the entries so that we can retrieve their capacity in free
    Directory_Entry null_entry = {0};
    local_entries[entries_size] = null_entry; 
    *entries = local_entries;

    wio_w_string_free(&built_path);
    wio_w_string_free(&dir_path);
    wio_string_free(&normalized_path);

    return true; 
}

//Frees previously allocated file list
void directory_list_contents_free(Directory_Entry* entries)
{
    if(entries == NULL)
        return;

    for(int64_t i = 0; entries[i].path != NULL; i++)
        wio_sure_realloc(entries[i].path, 0);
            
    wio_sure_realloc(entries, 0);
}

bool directory_set_current_working(const char* new_working_dir, int64_t path_size)
{
    IO_NORMALIZE_PATH_OP(new_working_dir, path_size,
        bool state = _wchdir(normalized_path.data) == 0;
    )
    return state;
}
    
char* directory_get_current_working_malloc()
{
    wchar_t* current_working = _wgetcwd(NULL, 0);
    if(current_working == NULL)
        abort();

    WIO_String output = wio_convert_to_utf8_normalize_path(current_working, (int64_t) wcslen(current_working), IO_NORMALIZE_LINUX | IO_NORMALIZE_DIRECTORY, NULL, 0);
    free(current_working);
    return output.data;
}
    
bool path_validate(const char* path, int64_t path_size, bool* is_directory)
{
    IO_NORMALIZE_PATH_OP(path, path_size, 
        DWORD attributes = GetFileAttributesW(normalized_path.data);
        bool state = attributes != INVALID_FILE_ATTRIBUTES;
        if(is_directory != NULL)
            *is_directory = !!(attributes & FILE_ATTRIBUTE_DIRECTORY) && state;
    )

    return state;
}

static bool wio_is_separator(char c)
{
    return c == '\\' || c == '/';
}
    
static bool wio_is_prefixed_with(const char* str, int64_t str_size, const char* prefix)
{
    assert(prefix != NULL);
    int64_t prefix_len = (int64_t) strlen(prefix);
    if(str_size < prefix_len)
        return false;

    for(int64_t j = 0; j < prefix_len; j++)
        if(str[j] != prefix[j])
            return false;

    return true;
}

static Path_Info wio_analyze_root(const char* path, int64_t path_size)
{
    #define PATH_PREFIX_LONG        "\\\\?\\"
    Path_Info info = {0};

    //so that we dont have to keep variables in sync
    #define REMAINING_PATH (path + info.prefix_size)
    #define REMAINING_SIZE (path_size - info.prefix_size)

    if(wio_is_prefixed_with(REMAINING_PATH, REMAINING_SIZE, PATH_PREFIX_LONG))
    {
        info.prefix_size = 4;
    }
        
    if(REMAINING_SIZE > 0 && wio_is_separator(REMAINING_PATH[0]))
    {
        info.root_size = info.prefix_size + 1;
        info.is_linux_style_absolute = true;
        return info;
    }
        
    info.root_size = info.prefix_size;
    if(REMAINING_SIZE >= 2)
    {
        char c = REMAINING_PATH[0];
        bool is_alpha_lower = ('a' <= c && c <= 'z');
        bool is_alpha_upper = ('A' <= c && c <= 'Z');

        //needs to have some letter as first
        if(!is_alpha_lower && !is_alpha_upper)
            return info;

        //needs to have colon second
        if(REMAINING_PATH[1] != ':') 
            return info;
            
        info.is_drive_style_absolute = true;
        if(is_alpha_upper)
            info.drive_letter = c;
        else
            info.drive_letter = c - 'a' + 'A';

        //if has optional slash
        if(REMAINING_SIZE >= 3 && wio_is_separator(REMAINING_PATH[2]))
            info.root_size = info.prefix_size + 3;
        else
            info.root_size = info.prefix_size + 2;
    }
        
    #undef REMAINING_PATH
    #undef REMAINING_SIZE
    return info;
}
    
Path_Info path_get_info(const char* path, int64_t path_size)
{
    Path_Info path_info = wio_analyze_root(path, path_size);
    path_info.is_absolute = path_info.is_drive_style_absolute || path_info.is_linux_style_absolute;
    if(path_info.is_drive_style_absolute)
        assert('A' <= path_info.drive_letter && path_info.drive_letter <= 'Z');
    else
        assert(path_info.drive_letter == 0);

    assert(path_info.prefix_size <= path_info.root_size);
    assert(path_info.root_size <= path_size);


    int64_t first_folder = path_size;
    for(; first_folder-- > path_info.root_size; )
    {
        if(wio_is_separator(path[first_folder]))
            break;
    }
        
    first_folder ++;
    path_info.filename_size = path_size - first_folder;
    //if ends with .. then is a folder
    if(path_info.filename_size == 2 && path[first_folder] == '.' && path[first_folder + 1] == '.')
        path_info.filename_size = 0;

    //if ends with . then is a folder
    if(path_info.filename_size == 1 && path[first_folder] == '.')
        path_info.filename_size = 0;

    if(path_info.filename_size == 0)
        path_info.is_directory = true;

    int64_t first_dot = path_size;
    for(; first_dot-- > first_folder; )
    {
        if(path[first_dot] == '.')
            break;
    }
    first_dot++;

    path_info.extension_size = path_size - first_dot;
    assert(path_info.filename_size >= path_info.extension_size);
    return path_info;
}

char* path_get_full_malloc(const char* path, int64_t path_size)
{
    IO_NORMALIZE_PATH_OP(path, path_size, 
        WIO_String full = wio_malloc_full_path(&normalized_path, IO_NORMALIZE_LINUX);
    )
    return full.data;
}
    
//void realpath(const char *filename, wchar_t *pathbuf, int size)
//{
//    OFSTRUCT of;
//    HANDLE file = (HANDLE)OpenFile(filename,&of,OF_READ);
//    GetFinalPathNameByHandle(file,pathbuf,size,FILE_NAME_OPENED);
//    CloseHandle(file);
//}
    
#define IO_NORMALIZE_NEEDED_EXTRA_SIZE 32
static int64_t wio_path_normalize(const char* path, int64_t path_size, int flags, char* output, int64_t output_size)
{
    assert(path_size >= 0);
    assert(output_size >= 0);

    //we need our extra IO_NORMALIZE_NEEDED_EXTRA_SIZE bytes of size
    //That ammount should cover all added chars to path 
    //(the number does not depend on path at all so its always correct)
    if(output_size < path_size + IO_NORMALIZE_NEEDED_EXTRA_SIZE)
        return path_size + IO_NORMALIZE_NEEDED_EXTRA_SIZE;

    //if output too big then auto add the long flag on windows
    if(path_size >= MAX_PATH && (flags & IO_NORMALIZE_WINDOWS))
        flags |= IO_NORMALIZE_LONG;

    char separator = '/';
    if((flags & IO_NORMALIZE_WINDOWS))
        separator = '\\';
            
    Path_Info path_info = path_get_info(path, path_size);
        
    int64_t prefix_writing_to = 0;

    //Add //?/
    if(flags & IO_NORMALIZE_LONG)
    {
        for(int64_t j = 0; PATH_PREFIX_LONG[j]; j++)
            output[prefix_writing_to++] = PATH_PREFIX_LONG[j];
    }

    //Add /
    if(path_info.is_linux_style_absolute)
        output[prefix_writing_to++] = separator;

    //Add C:/
    if(path_info.is_drive_style_absolute)
    {
        assert(path_info.drive_letter != '\0');
        output[prefix_writing_to++] = path_info.drive_letter;
        output[prefix_writing_to++] = ':';
        output[prefix_writing_to++] = separator;
    }
        
    char* prefixed_output = output + prefix_writing_to;
    const char* path_remainder = path + path_info.root_size;
    int64_t path_remainder_size = path_size - path_info.root_size;
        
    int64_t writing_to = 0;
    int64_t reading_from = 0;
    assert(writing_to + prefix_writing_to < output_size);

    //Copy characters to output removing double slashes 
    for(; reading_from < path_remainder_size; reading_from++, writing_to++)
    {
        if(!wio_is_separator(path_remainder[reading_from]))
        {
            prefixed_output[writing_to] = path_remainder[reading_from];
            continue;
        }
            
        prefixed_output[writing_to] = separator;
            
        //If last char was slash ignore this one (move one back)
        if(writing_to >= 1 && wio_is_separator(prefixed_output[writing_to - 1]))
            writing_to--;
    }
        
    //and also adding one final slash at the end
    //(the final added / saves us from having to specifically handle .. and . at the end)
    if(writing_to >= 1 && !wio_is_separator(prefixed_output[writing_to - 1]))
        prefixed_output[writing_to++] = separator;

    //trims the string
    prefixed_output[writing_to] = '\0';
    //printf("removed doubles: \"%s\"\n", output);
        
    assert(writing_to + prefix_writing_to < output_size);
    int64_t curr_output_size = writing_to;
    //removes ./ ../
    writing_to = 0;
    reading_from = 0;
    for(; reading_from < curr_output_size; reading_from++, writing_to++)
    {
        prefixed_output[writing_to] = prefixed_output[reading_from];
        if(wio_is_separator(prefixed_output[writing_to]))
        {
            //if ../ is encountered iterate backwards until a the parent directory is found
            //then set the wrting position back on the parent diretory overriding it
            //with the next parts of the escaped path
            if(writing_to >= 2 && prefixed_output[writing_to - 1] == '.' && prefixed_output[writing_to - 2] == '.')
            {
                int64_t found_last = writing_to;
                int64_t found_separators_count = 0;
                for(; found_last-- > 0; )
                {
                    if(wio_is_separator(prefixed_output[found_last]))
                    {
                        found_separators_count ++;
                        if(found_separators_count == 2)
                            break;
                    }
                }

                if(found_separators_count > 0 || path_info.is_absolute)
                    writing_to = found_last;
            }
            //assert(fragment_size != 0);
            //if ./ is encountered go back to the last separator
            else if(writing_to >= 1 && prefixed_output[writing_to - 1] == '.')
                writing_to -= 2;
        }
    }
        
    bool to_dir = !!(flags & IO_NORMALIZE_DIRECTORY);
    bool to_file = !!(flags & IO_NORMALIZE_FILE);

    //if indetermined target select target based on path_info
    if(!to_dir && !to_file)
    {
        to_dir = path_info.is_directory;
        to_file = !path_info.is_directory;
    }

    //If should transform into directory path add the final /
    if(to_dir && writing_to != 0)
    {
        if(!wio_is_separator(prefixed_output[writing_to - 1]))
            prefixed_output[writing_to++] = separator;
    }
        
    //If should transform into file path remove the / (if present)
    if(to_file && writing_to != 0)
    {
        if(wio_is_separator(prefixed_output[writing_to - 1]))
            writing_to--;
    }

    //if empty add ./
    if(writing_to == 0)
    {
        prefixed_output[writing_to++] = '.';
        prefixed_output[writing_to++] = separator;
    }

    prefixed_output[writing_to] = '\0';
    assert(writing_to + prefix_writing_to < output_size);
    return writing_to + prefix_writing_to;
}
    
char* path_normalize_malloc(const char* path, int64_t path_size, int norm_as_filetype)
{
    int flags = IO_NORMALIZE_LINUX;
    if(norm_as_filetype == FILE_TYPE_DIRECTORY)
        flags |= IO_NORMALIZE_DIRECTORY;
    else if (norm_as_filetype == FILE_TYPE_FILE)
        flags |= IO_NORMALIZE_FILE;

    WIO_String normalized = {0};
    wio_normalize_allocate_path(path, path_size, flags, &normalized);
    return normalized.data;
}

int64_t path_normalize(const char* path, int64_t path_size, int norm_as_filetype, char* buffer, int64_t buffer_size)
{
    int flags = IO_NORMALIZE_LINUX;
    if(norm_as_filetype == FILE_TYPE_DIRECTORY)
        flags |= IO_NORMALIZE_DIRECTORY;
    else if (norm_as_filetype == FILE_TYPE_FILE)
        flags |= IO_NORMALIZE_FILE;

    return wio_path_normalize(path, path_size, flags, buffer, buffer_size);
}
    
static void wio_normalize_allocate_path(const char* path, int64_t path_size, int flags, WIO_String* output)
{
    wio_string_resize(output, path_size + IO_NORMALIZE_NEEDED_EXTRA_SIZE);  
    int64_t new_size = wio_path_normalize(path, path_size, flags, output->data, output->size);
    assert(new_size < output->size && "must suceed");
    wio_string_resize(output, new_size);  
}
    

static void wio_string_free(WIO_String* string)
{
    if(string->data != string->buffer)
        wio_sure_realloc(string->data, 0);

    string->capacity = 0;
    string->size = 0;
    string->data = NULL;
}

static void wio_string_resize(WIO_String* string, int64_t size)
{
    if(size >= string->capacity)
    {
        if(size < string->buffer_size && string->buffer != NULL)
        {
            string->data = string->buffer;
            string->capacity = string->buffer_size;
        }
        else
        {
            char* prev_data = string->data != string->buffer ? string->data : NULL;
            string->data = (char*) wio_sure_realloc(prev_data, size+1);
            string->capacity = size;
        }
    }
        
    string->size = size;
    string->data[size] = '\0';
}
    
static void wio_w_string_free(WIO_w_String* string)
{
    if(string->data != string->buffer)
        wio_sure_realloc(string->data, 0);
}

static void wio_w_string_resize(WIO_w_String* string, int64_t size)
{
    if(size >= string->capacity)
    {
        if(size < string->buffer_size && string->buffer != NULL)
        {
            string->data = string->buffer;
            string->capacity = string->buffer_size;
        }
        else
        {
            wchar_t* prev_data = string->data != string->buffer ? string->data : NULL;
            string->data = (wchar_t*) wio_sure_realloc(prev_data, (int64_t) sizeof(wchar_t) * (size+1));
            string->capacity = size;
        }
    }
        
    string->size = size;
    string->data[size] = '\0';
}

static void wio_utf16_to_utf8(const wchar_t* utf16, int64_t utf16len, WIO_String* output) 
{
    if (utf16 == NULL || utf16len == 0) 
    {
        wio_string_resize(output, 0);
        return;
    }

    //int utf16len = wcslen(utf16);
    int utf8len = WideCharToMultiByte(CP_UTF8, 0, utf16, (int) utf16len, NULL, 0, NULL, NULL);
    wio_string_resize(output, utf8len);
    WideCharToMultiByte(CP_UTF8, 0, utf16, (int) utf16len, output->data, (int) utf8len, 0, 0);
}
    
static void wio_utf8_to_utf16(const char* utf8, int64_t utf8len, WIO_w_String* output) 
{
    if (utf8 == NULL || utf8len == 0) 
    {
        wio_w_string_resize(output, 0);
        return;
    }

    //int utf8len = strlen(utf8);
    int utf16len = MultiByteToWideChar(CP_UTF8, 0, utf8, (int) utf8len, NULL, 0);
    wio_w_string_resize(output, utf16len);
    MultiByteToWideChar(CP_UTF8, 0, utf8, (int) utf8len, output->data, (int) utf16len);
}

static void wio_w_concat(const wchar_t* a, const wchar_t* b, const wchar_t* c, WIO_w_String* output)
{
    int64_t a_size = a != NULL ? (int64_t) wcslen(a) : 0;
    int64_t b_size = b != NULL ? (int64_t) wcslen(b) : 0;
    int64_t c_size = c != NULL ? (int64_t) wcslen(c) : 0;
    int64_t composite_size = a_size + b_size + c_size;
        
    wio_w_string_resize(output, composite_size);
    memmove(output->data,                   a, sizeof(wchar_t) * a_size);
    memmove(output->data + a_size,          b, sizeof(wchar_t) * b_size);
    memmove(output->data + a_size + b_size, c, sizeof(wchar_t) * c_size);
}

WIO_w_String wio_normalize_convert_to_utf16_path(const char* path, int64_t path_size, int flags, wchar_t* buffer, int64_t buffer_size)
{
    char norm_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    WIO_String norm_path = {0};
    norm_path.buffer = norm_path_buffer;
    norm_path.buffer_size = IO_LOCAL_BUFFER_SIZE;

    WIO_w_String output = {0};
    output.buffer = buffer;
    output.buffer_size = buffer_size;
    wio_normalize_allocate_path(path, path_size, IO_NORMALIZE_WINDOWS | flags, &norm_path);
    wio_utf8_to_utf16(norm_path.data, norm_path.size, &output);
    wio_string_free(&norm_path);

    return output;
}
    
WIO_String wio_convert_to_utf8_normalize_path(const wchar_t* path, int64_t path_size, int flags, char* buffer, int64_t buffer_size)
{
    char local_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    WIO_String local = {0};
    local.buffer = local_buffer;
    local.buffer_size = IO_LOCAL_BUFFER_SIZE;
    wio_utf16_to_utf8(path, path_size, &local);

    WIO_String output = {0};
    output.buffer = buffer;
    output.buffer_size = buffer_size;
    wio_normalize_allocate_path(local.data, local.size, flags, &output);
    wio_string_free(&local);

    return output;
}

void test_normalize_path_single(const char* to_simplify, int style, const char* expected_result)
{
    char normalized_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    for(int64_t i = 0; i < IO_LOCAL_BUFFER_SIZE; i++)
        normalized_buffer[i] = '$'; //initialize to something else then null ermination

    WIO_String normalized = {0};
    normalized.buffer = normalized_buffer;
    normalized.buffer_size = IO_LOCAL_BUFFER_SIZE;
        
    printf("\nbefore:   \"%s\"\n", to_simplify);
    int64_t len = (int64_t) strlen(to_simplify);
    wio_normalize_allocate_path(to_simplify, len, style, &normalized);

    printf("simplify: \"%s\"\n", normalized.data);
    printf("expected: \"%s\"\n", expected_result);

    //int64_t expected_len = strlen(expected_result);
    assert(strcmp(normalized.data, expected_result) == 0);

    wio_string_free(&normalized);
}

typedef enum Path_Info_Test_Flags
{
    TEST_PATH_INFO_ABSOLUTE = 1,
    TEST_PATH_INFO_ABSOLUTE_LINUX = 2,
    TEST_PATH_INFO_ABSOLUTE_DRIVE = 4,
    TEST_PATH_INFO_DIRECTORY = 8,
} Path_Info_Test_Flags;
    
void test_path_get_info_single(const char* path, int64_t prefix, int64_t root, int64_t file, int64_t ext, int flag, char letter)
{
    int64_t len = (int64_t) strlen(path);
    Path_Info info = path_get_info(path, len);

    assert(info.prefix_size == prefix);
    assert(info.root_size == root);
    assert(info.filename_size == file);
    assert(info.extension_size == ext);

    assert(info.is_absolute == !!(flag & TEST_PATH_INFO_ABSOLUTE));
    assert(info.is_linux_style_absolute == !!(flag & TEST_PATH_INFO_ABSOLUTE_LINUX));
    assert(info.is_drive_style_absolute == !!(flag & TEST_PATH_INFO_ABSOLUTE_DRIVE));
    assert(info.is_directory == !!(flag & TEST_PATH_INFO_DIRECTORY));

    assert(info.drive_letter == letter);
}

void test_normalize_path()
{
    #define L_PREF PATH_PREFIX_LONG

    {
        int ABSL = TEST_PATH_INFO_ABSOLUTE_LINUX | TEST_PATH_INFO_ABSOLUTE;
        int ABSD = TEST_PATH_INFO_ABSOLUTE_DRIVE | TEST_PATH_INFO_ABSOLUTE;
        int DIR  = TEST_PATH_INFO_DIRECTORY;
        
        test_path_get_info_single("C:\\path/to/file.txt",       0, 3, 8, 3, ABSD, 'C');
        test_path_get_info_single("c:/path/file.txt",           0, 3, 8, 3, ABSD, 'C');
        test_path_get_info_single("c://path/file.b",            0, 3, 6, 1, ABSD, 'C');
        test_path_get_info_single("f://path/file.",             0, 3, 5, 0, ABSD, 'F');
        test_path_get_info_single(L_PREF "q:/path/file.txt",    4, 7, 8, 3, ABSD, 'Q');
        test_path_get_info_single(L_PREF "a:/path/folder/",     4, 7, 0, 0, ABSD | DIR, 'A');
            
        test_path_get_info_single("/file.txt",                  0, 1, 8, 3, ABSL, 0);
        test_path_get_info_single("/file.txt/",                 0, 1, 0, 0, ABSL | DIR, 0);
        test_path_get_info_single(L_PREF"/file.txt/",           4, 5, 0, 0, ABSL | DIR, 0);
        test_path_get_info_single("",                           0, 0, 0, 0, DIR, 0);
        test_path_get_info_single("..",                         0, 0, 0, 0, DIR, 0);
        test_path_get_info_single(".",                          0, 0, 0, 0, DIR, 0);
        test_path_get_info_single(L_PREF ".",                   4, 4, 0, 0, DIR, 0);
        test_path_get_info_single(L_PREF "",                    4, 4, 0, 0, DIR, 0);
        test_path_get_info_single("G:\\",                       0, 3, 0, 0, ABSD | DIR, 'G');
        test_path_get_info_single("z:",                         0, 2, 0, 0, ABSD | DIR, 'Z');
        test_path_get_info_single("/",                          0, 1, 0, 0, ABSL | DIR, 0);
        test_path_get_info_single(L_PREF"h:",                   4, 6, 0, 0, ABSD | DIR, 'H');
        test_path_get_info_single(L_PREF"/",                    4, 5, 0, 0, ABSL | DIR, 0);
    }

    {
        int WIN = IO_NORMALIZE_WINDOWS;
        int LIN = IO_NORMALIZE_LINUX;
        int LWIN = IO_NORMALIZE_WINDOWS | IO_NORMALIZE_LONG;

        int D = IO_NORMALIZE_DIRECTORY;
        int F = IO_NORMALIZE_FILE;

        test_normalize_path_single("path/to/file.txt",             WIN,  "path\\to\\file.txt");
        test_normalize_path_single("path///to//file.txt",          WIN,  "path\\to\\file.txt");
        test_normalize_path_single("C:path/to/file.txt",           WIN,  "C:\\path\\to\\file.txt");
        test_normalize_path_single("c:/path/to/file.txt",          WIN,  "C:\\path\\to\\file.txt");
        test_normalize_path_single("g:\\path/to/file.txt",         WIN,  "G:\\path\\to\\file.txt");
        test_normalize_path_single("z:path/to/file.txt",           WIN,  "Z:\\path\\to\\file.txt");
        test_normalize_path_single("\\path/to/file.txt",           WIN,  "\\path\\to\\file.txt");
        test_normalize_path_single("path\\to\\file.txt",           LIN,  "path/to/file.txt");
            
        test_normalize_path_single("",                             WIN,  ".\\");
        test_normalize_path_single("",                             WIN | D, ".\\");
        test_normalize_path_single("",                             WIN | F, ".\\");
        test_normalize_path_single("",                             LIN,  "./");
        test_normalize_path_single("",                             LIN | D,  "./");
        test_normalize_path_single(L_PREF "",                      WIN,  ".\\");
        test_normalize_path_single(L_PREF "",                      LIN,  "./");
        test_normalize_path_single("..",                           WIN,  "..\\");
        test_normalize_path_single("..",                           LIN,  "../");
        test_normalize_path_single(L_PREF "some/path/..",          WIN,  "some\\");
        test_normalize_path_single(L_PREF "some/path/.",           LIN,  "some/path/");
        test_normalize_path_single(L_PREF "..",                    LIN,  "../");

        test_normalize_path_single(L_PREF "server\\",              LIN,  "server/");
        test_normalize_path_single(L_PREF "server\\folder",        LIN,  "server/folder");
        test_normalize_path_single(L_PREF "server\\folder\\..\\",  LIN,  "server/");

        test_normalize_path_single("\\server\\",                   LIN,  "/server/");
        test_normalize_path_single("\\server/",                    WIN,  "\\server\\");
        test_normalize_path_single("path/to/file.txt",             LWIN, L_PREF "path\\to\\file.txt");
        test_normalize_path_single(L_PREF "path\\to\\file.txt",    LIN,  "path/to/file.txt");

        test_normalize_path_single("path/to/../file.txt",          WIN,  "path\\file.txt");
        test_normalize_path_single("path\\to\\..\\..\\file.txt",   LIN,  "file.txt");
        test_normalize_path_single("path/to//..//..//file.txt",    LIN,  "file.txt");
        test_normalize_path_single("../file.txt",                  LWIN, L_PREF "..\\file.txt");
        test_normalize_path_single(L_PREF "/./file.txt",           LWIN, L_PREF "\\file.txt");
        test_normalize_path_single(L_PREF "./../file.txt",         LWIN, L_PREF "..\\file.txt");
        test_normalize_path_single(L_PREF "/./.././file.txt",      LIN,  "/file.txt");
        test_normalize_path_single(L_PREF "/./.././file.txt",      LIN,  "/file.txt");
        test_normalize_path_single("C:/../../../file.txt",         LWIN, L_PREF "C:\\file.txt");
        test_normalize_path_single("C:/../././../file.txt",        LWIN, L_PREF "C:\\file.txt");

        test_normalize_path_single("path/to/./file.txt",           WIN | D, "path\\to\\file.txt\\");
        test_normalize_path_single("path/./to/file.txt\\",         WIN | D, "path\\to\\file.txt\\");
        test_normalize_path_single("path/to/file.txt",             WIN | F, "path\\to\\file.txt");
        test_normalize_path_single("path/to/file.txt/",            WIN | F, "path\\to\\file.txt");
        test_normalize_path_single("path/to/file.txt",             LWIN | D, L_PREF "path\\to\\file.txt\\");
    }
        
}

JOT_IO_END