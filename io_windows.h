
#pragma once
#include "io.h"

#include <string.h>
#include <assert.h>
#include <windows.h>
#undef min
#undef max

#include <stdio.h>
#include <stdlib.h>
#include <stringapiset.h>
//@TODO: remove this as a dependency!
//@TODO: handle allocations properly + swap allocator tests
//@TODO: JAPI all the implementations
//@TODO: Move into actual c file 
//@TODO: make private functions start with _ (maybe)

#include <direct.h>
#include <locale.h> //for tests alone

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
#define IO_LOCAL_BUFFER_SIZE 3


//We assume allocations never fail and if they do we just abort the entire program. If you wish to handle it
// differently feel free to change this function
static void* wio_default_allocator(void* allocated, size_t size, void* context)
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

IO_Allocator WIO_ALLOCATOR = {wio_default_allocator, NULL};

void io_set_allocator(IO_Allocator allocator)
{
    WIO_ALLOCATOR = allocator;
}

IO_Allocator io_get_allocator()
{
    return WIO_ALLOCATOR;
}

void* io_realloc(void* allocated, size_t size)
{
    if(WIO_ALLOCATOR.reallocate == NULL)
        return NULL;

    return WIO_ALLOCATOR.reallocate(allocated, size, WIO_ALLOCATOR.context);
}
void* io_malloc(size_t size)
{
    return io_realloc(NULL, size);
}
void io_free(void* ptr)
{
    io_realloc(ptr, 0);
}

typedef struct WIO_Buffer
{
    void* data;
    int32_t item_size;
    int32_t is_alloced;
    int64_t size;
    int64_t capacity;
} WIO_Buffer;

typedef enum Normalize_Flags
{
    IO_NORMALIZE_WINDOWS = 1,
    IO_NORMALIZE_LINUX = 2,
    IO_NORMALIZE_LONG = 4,
    IO_NORMALIZE_DIRECTORY = 8,
    IO_NORMALIZE_FILE = 16,
} Normalize_Flags;

static wchar_t*     wio_wstring(WIO_Buffer stack);
static char*        wio_string(WIO_Buffer stack);
static WIO_Buffer   wio_buffer_init_backed(void* backing, int64_t backing_size, int32_t item_size);
static WIO_Buffer   wio_buffer_init(int64_t item_size);
static void*        wio_buffer_resize(WIO_Buffer* stack, int64_t new_size);
static int64_t      wio_buffer_push(WIO_Buffer* stack, const void* item, int64_t item_size);
static int64_t      wio_buffer_pop(WIO_Buffer* stack);
static void*        wio_buffer_at(WIO_Buffer* stack, int64_t index);
static void         wio_buffer_deinit(WIO_Buffer* stack);

static void wio_utf16_to_utf8(const wchar_t* utf16, int64_t utf16len, WIO_Buffer* output);
static void wio_utf8_to_utf16(const char* utf8, int64_t utf8len, WIO_Buffer* output);
    
static void wio_w_concat(const wchar_t* a, const wchar_t* b, const wchar_t* c, WIO_Buffer* output);
static void wio_normalize_allocate_path(const char* path, int64_t path_size, int flags, WIO_Buffer* output);
static WIO_Buffer wio_normalize_convert_to_utf16_path(const char* path, int64_t path_size, int flags, char* buffer, int64_t buffer_size);
static WIO_Buffer wio_convert_to_utf8_normalize_path(const wchar_t* path, int64_t path_size, int flags, char* buffer, int64_t buffer_size);
    
static HANDLE* wio_get_file_handle_ptr(File* file)
{
    return (HANDLE*) (void*) file->state;
}

static HANDLE wio_get_file_handle(File CREF file)
{
    return *(HANDLE*) (void*) file.state;
}
    
#ifndef JOT_IO_C
//@TODO: remove this and instead have 0 init
File::File(int)
{
    memset(this, 0, sizeof(*this));
    *wio_get_file_handle_ptr(this) = INVALID_HANDLE_VALUE;
}
    
File::File(File && other) noexcept
{
    memset(this, 0, sizeof(*this));
    *wio_get_file_handle_ptr(this) = *wio_get_file_handle_ptr(&other);
    *wio_get_file_handle_ptr(&other) = INVALID_HANDLE_VALUE;
}
    
File& File::operator =(File&& other) noexcept
{
    HANDLE temp = *wio_get_file_handle_ptr(&other);
    *wio_get_file_handle_ptr(&other) = *wio_get_file_handle_ptr(this);
    *wio_get_file_handle_ptr(this) = temp;
    return *this;
}

File::~File() noexcept
{
    file_close(this);
}
#endif

File file_open(const char* path, int64_t path_size, int open_mode)
{       
    char normalized_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    WIO_Buffer normalized_path = wio_normalize_convert_to_utf16_path(path, path_size, 0, normalized_path_buffer, IO_LOCAL_BUFFER_SIZE);
        
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
    else if(open_mode & FILE_OPEN_CREATE_ELSE_FAIL)
        disposition = CREATE_NEW;
    else
        disposition = OPEN_EXISTING;

    File f;
    f.open_mode = open_mode;
    *wio_get_file_handle_ptr(&f) = CreateFileW(wio_wstring(normalized_path), access, sharing, NULL, disposition, 0, NULL);

    wio_buffer_deinit(&normalized_path);
    return f;
}

void file_close(File* file)
{
    HANDLE handle = *wio_get_file_handle_ptr(file);
    CloseHandle(handle);
    *wio_get_file_handle_ptr(file) = INVALID_HANDLE_VALUE;
}
    
bool file_is_open(File CREF file)
{
    HANDLE h = wio_get_file_handle(file);
    return h != INVALID_HANDLE_VALUE && h != NULL;
}

int64_t file_read(File* file, void* read_into, int64_t size, File_IO_State* state)
{
    HANDLE handle = *wio_get_file_handle_ptr(file);
    if(!file_is_open(*file))
    {
        *state = FILE_IO_STATE_FILE_CLOSED;
        return 0;
    }
    
    *state = FILE_IO_STATE_OK;
    int64_t processed_size = 0;
    while(processed_size < size)
    {
        int64_t remaining_size = size - processed_size;
        DWORD to_read = (DWORD) (remaining_size & 0x8FFFFF);
        DWORD read = 0;
        bool error = !ReadFile(handle, (char*) read_into + processed_size, to_read, &read, NULL);
            
        if(error)
        {
            *state = FILE_IO_STATE_ERROR;
            break;
        }

        processed_size += read;
        // Check for eof
        if (read == 0) 
        {
            *state = FILE_IO_STATE_EOF;
            break;
        }
    }

    return processed_size;
}
    
int64_t file_write(File* file, const void* write_from, int64_t size, File_IO_State* state)
{
    HANDLE handle = *wio_get_file_handle_ptr(file);
    if(!file_is_open(*file))
    {
        *state = FILE_IO_STATE_FILE_CLOSED;
        return 0;
    }
        
    *state = FILE_IO_STATE_OK;
    int64_t processed_size = 0;
    while(processed_size < size)
    {
        int64_t remaining_size = size - processed_size;
        DWORD to_write = (DWORD) (remaining_size & 0x8FFFFF);
        DWORD written = 0;
        bool error = !WriteFile(handle, (char*) write_from + processed_size, to_write, &written, NULL);
        if(error || written <= 0)
        {
            *state = FILE_IO_STATE_ERROR;
            break;
        }

        processed_size += written;
    }

    return processed_size;
}

//Offsets the current possition in file by offset relative to from
bool file_seek(File* file, int64_t offset, File_Seek from)
{
    if(!file_is_open(*file))
        return false;
            
    HANDLE handle = *wio_get_file_handle_ptr(file);
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
            
    HANDLE handle = wio_get_file_handle(file);
    LARGE_INTEGER move;
    LARGE_INTEGER curr_offset;
    move.QuadPart = 0;
    curr_offset.QuadPart = 0;
        
    bool state = !!SetFilePointerEx(handle, move, &curr_offset, 1);
    if(!state)
        return -1;

    return (int64_t) curr_offset.QuadPart;
}
    
bool file_trim(File* file, int64_t max_size)
{
    if(!file_is_open(*file))
        return false;

    bool state = true;
    int64_t offset = file_tell(*file);
    if(offset == -1)
        state = false;
        
    if(offset != max_size)
        state = state && file_seek(file, max_size, FILE_SEEK_START);
    
    HANDLE handle = wio_get_file_handle(*file);
    state = state && SetEndOfFile(handle);

    if(offset < max_size && offset != -1)
        state = file_seek(file, offset, FILE_SEEK_START) && state;

    return state;
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
        char normalized_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; \
        WIO_Buffer normalized_path = wio_normalize_convert_to_utf16_path(path, path_size, 0, normalized_path_buffer, IO_LOCAL_BUFFER_SIZE); \
        \
        execute\
        \
        wio_buffer_deinit(&normalized_path); \

    IO_NORMALIZE_PATH_OP(file_path, path_size,
        SetFileAttributesW(wio_wstring(normalized_path), FILE_ATTRIBUTE_NORMAL);
        bool state = !!DeleteFileW(wio_wstring(normalized_path));
    )
    return state;
}

bool file_move(const char* new_path, int64_t new_path_size, const char* old_path, int64_t old_path_size)
{       
    char new_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    char old_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    WIO_Buffer new_path_norm = wio_normalize_convert_to_utf16_path(new_path, new_path_size, 0, new_path_buffer, IO_LOCAL_BUFFER_SIZE);
    WIO_Buffer old_path_norm = wio_normalize_convert_to_utf16_path(old_path, old_path_size, 0, old_path_buffer, IO_LOCAL_BUFFER_SIZE);
        
    bool state = !!MoveFileExW(wio_wstring(old_path_norm), wio_wstring(new_path_norm), MOVEFILE_COPY_ALLOWED);

    wio_buffer_deinit(&new_path_norm);
    wio_buffer_deinit(&old_path_norm);
    return state;
}

bool file_copy(const char* copy_to_path, int64_t to_path_size, const char* copy_from_path, int64_t from_path_size)
{
    char to_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    char from_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    WIO_Buffer to_path_norm = wio_normalize_convert_to_utf16_path(copy_to_path, to_path_size, 0, to_path_buffer, IO_LOCAL_BUFFER_SIZE);
    WIO_Buffer from_path_norm = wio_normalize_convert_to_utf16_path(copy_from_path, from_path_size, 0, from_path_buffer, IO_LOCAL_BUFFER_SIZE);
        
    bool state = !!CopyFileW(wio_wstring(from_path_norm), wio_wstring(to_path_norm), true);

    wio_buffer_deinit(&to_path_norm);
    wio_buffer_deinit(&from_path_norm);
    return state;
}

static time_t wio_filetime_to_time_t(FILETIME ft)  
{    
    ULARGE_INTEGER ull;    
    ull.LowPart = ft.dwLowDateTime;    
    ull.HighPart = ft.dwHighDateTime;    
    return (time_t) (ull.QuadPart / 10000000ULL - 11644473600ULL);  
}

bool wio_is_file_link(const wchar_t* directory_path)
{
    HANDLE file = CreateFileW(directory_path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    size_t requiredSize = ::GetFinalPathNameByHandleW(file, NULL, 0, FILE_NAME_NORMALIZED);
    CloseHandle(file);

    return requiredSize == 0;
}

bool file_info(const char* file_path, int64_t path_size, File_Info* info)
{    
    WIN32_FILE_ATTRIBUTE_DATA native_info;
    memset(&native_info, 0, sizeof(native_info)); 
    memset(info, 0, sizeof(*info)); 
    IO_NORMALIZE_PATH_OP(file_path, path_size, 
        bool state = !!GetFileAttributesExW(wio_wstring(normalized_path), GetFileExInfoStandard, &native_info);
        
        if(native_info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
            info->is_link = wio_is_file_link(wio_wstring(normalized_path));
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
        bool state = !!CreateDirectoryW(wio_wstring(normalized_path), NULL);
    )
    return state;
}
    
bool directory_remove(const char* dir_path, int64_t path_size)
{
    IO_NORMALIZE_PATH_OP(dir_path, path_size, 
        bool state = !!RemoveDirectoryW(wio_wstring(normalized_path));
    )
    return state;
}

static WIO_Buffer wio_malloc_full_path(const wchar_t* local_path, int flags)
{
    char full_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    WIO_Buffer full_path = wio_buffer_init_backed(full_path_buffer, IO_LOCAL_BUFFER_SIZE, sizeof(wchar_t));

    int64_t needed_size = GetFullPathNameW(local_path, 0, NULL, NULL);
    if(needed_size > full_path.size)
    {
        wio_buffer_resize(&full_path, needed_size);
        needed_size = GetFullPathNameW(local_path, (DWORD) full_path.size, wio_wstring(full_path), NULL);
    }
    WIO_Buffer out_string = wio_convert_to_utf8_normalize_path(wio_wstring(full_path), full_path.size, flags, NULL, 0);
        
    wio_buffer_deinit(&full_path);
    return out_string;
}

typedef struct Directory_Visitor
{
    WIN32_FIND_DATAW current_entry;
    HANDLE first_found;
    bool failed;
} Directory_Visitor;

#define WIO_FILE_MASK_ALL L"\\*.*"

static Directory_Visitor wio_directory_iterate_init(const wchar_t* dir_path, const wchar_t* file_mask)
{
    char built_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    WIO_Buffer built_path = wio_buffer_init_backed(built_path_buffer, IO_LOCAL_BUFFER_SIZE, sizeof(wchar_t));
    
    wio_w_concat(dir_path, file_mask, NULL, &built_path);

    Directory_Visitor visitor = {0};
    assert(built_path.data != NULL);
    visitor.first_found = FindFirstFileW(wio_wstring(built_path), &visitor.current_entry);
    while(visitor.failed == false && visitor.first_found != INVALID_HANDLE_VALUE)
    {
        if(wcscmp(visitor.current_entry.cFileName, L".") == 0
            || wcscmp(visitor.current_entry.cFileName, L"..") == 0)
            visitor.failed = !FindNextFileW(visitor.first_found, &visitor.current_entry);
        else
            break;
    }
    return visitor;
}

static bool wio_directory_iterate_has(const Directory_Visitor* visitor)
{
    return visitor->first_found != INVALID_HANDLE_VALUE && visitor->failed == false;
}

static void wio_directory_iterate_next(Directory_Visitor* visitor)
{
    visitor->failed = visitor->failed || !FindNextFileW(visitor->first_found, &visitor->current_entry);
}

static void wio_directory_iterate_deinit(Directory_Visitor* visitor)
{
    FindClose(visitor->first_found);
}

//Iterates all contents of directory potentially recursively using BFS and potentially stopping when iter_fn returns false
static bool wio_directory_list_contents_malloc(const wchar_t* directory_path, WIO_Buffer* entries, bool recursive)
{
    typedef struct Dir_Context
    {
        Directory_Visitor visitor;
        const wchar_t* path;    
        int64_t depth;          
        int64_t index;          
    } Dir_Context;

    Dir_Context first = {0};

    
    first.visitor = wio_directory_iterate_init(directory_path, WIO_FILE_MASK_ALL);
    first.path = directory_path;
    if(wio_directory_iterate_has(&first.visitor) == false)
        return false;
        
    char stack_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    char built_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    
    WIO_Buffer stack = wio_buffer_init_backed(stack_buffer, IO_LOCAL_BUFFER_SIZE, sizeof(Dir_Context));
    WIO_Buffer built_path = wio_buffer_init_backed(built_path_buffer, IO_LOCAL_BUFFER_SIZE, sizeof(wchar_t));

    wio_buffer_push(&stack, &first, sizeof(first));

    const int64_t MAX_RECURSION = 1000;
        
    //While the queue is not empty iterate
    for(int64_t reading_from = 0; reading_from < stack.size; reading_from++)
    {
        Dir_Context* dir_context = (Dir_Context*) wio_buffer_at(&stack, reading_from);
        for(; wio_directory_iterate_has(&dir_context->visitor); wio_directory_iterate_next(&dir_context->visitor), dir_context->index++)
        {
            //Build up our file path using the passed in 
            //  [path] and the file/foldername we just found: 
            wio_w_concat(dir_context->path, L"\\", dir_context->visitor.current_entry.cFileName, &built_path);
            assert(built_path.data != 0);
        
            File_Info info = {0};
            info.created_time = wio_filetime_to_time_t(dir_context->visitor.current_entry.ftCreationTime);
            info.last_access_time = wio_filetime_to_time_t(dir_context->visitor.current_entry.ftLastAccessTime);
            info.last_write_time = wio_filetime_to_time_t(dir_context->visitor.current_entry.ftLastWriteTime);
            info.size = ((int64_t) dir_context->visitor.current_entry.nFileSizeHigh << 32) | ((int64_t) dir_context->visitor.current_entry.nFileSizeLow);
        
            info.type = FILE_TYPE_FILE;
            if(dir_context->visitor.current_entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                info.type = FILE_TYPE_DIRECTORY;
            else
                info.type = FILE_TYPE_FILE;

            if(info.is_link)
                info.is_link = wio_is_file_link(wio_wstring(built_path));  

            int flag = IO_NORMALIZE_LINUX;
            if(info.type == FILE_TYPE_DIRECTORY)
                flag |= IO_NORMALIZE_DIRECTORY;
            else
                flag |= IO_NORMALIZE_FILE;
            WIO_Buffer out_string = wio_malloc_full_path(wio_wstring(built_path), flag);
            printf("found: \"%s\"\n", wio_string(out_string));

            Directory_Entry entry = {0};
            entry.info = info;
            entry.path = wio_string(out_string);
            entry.path_size = out_string.size;
    
            wio_buffer_push(entries, &entry, sizeof(entry));

            if(info.type == FILE_TYPE_DIRECTORY && info.is_link == false && recursive)
            {
                WIO_Buffer next_path = wio_buffer_init(sizeof(wchar_t));
                wio_w_concat(wio_wstring(built_path), NULL, NULL, &next_path);

                Dir_Context next = {0};
                next.visitor = wio_directory_iterate_init(wio_wstring(next_path), WIO_FILE_MASK_ALL);
                next.depth = dir_context->depth + 1;
                next.path = wio_wstring(next_path);
                assert(next.depth < MAX_RECURSION && "must not get stuck in an infinite loop");
                wio_buffer_push(&stack, &next, sizeof(next));
            }
        }
    }

    for(int64_t i = 0; i < stack.size; i++)
    {
        Dir_Context* dir_context = (Dir_Context*) wio_buffer_at(&stack, i);
        if(dir_context->path != directory_path)
            io_realloc((void*) dir_context->path, 0);
        wio_directory_iterate_deinit(&dir_context->visitor);
    }
    
    //Null terminate the entries
    Directory_Entry terminator = {0};
    wio_buffer_push(entries, &terminator, sizeof(terminator));

    wio_buffer_deinit(&stack);
    wio_buffer_deinit(&built_path);
    return true;
}

bool directory_list_contents_malloc(const char* directory_path, int64_t path_size, Directory_Entry** entries, int64_t* entries_count, bool recursive)
{
    assert(entries != NULL && entries_count != NULL);
    WIO_Buffer entries_stack = wio_buffer_init(sizeof(Directory_Entry));
    IO_NORMALIZE_PATH_OP(directory_path, path_size,
        bool ok = wio_directory_list_contents_malloc(wio_wstring(normalized_path), &entries_stack, recursive);
    )

    *entries = (Directory_Entry*) entries_stack.data;
    *entries_count = entries_stack.size;
    return ok;
}

//Frees previously allocated file list
void directory_list_contents_free(Directory_Entry* entries)
{
    if(entries == NULL)
        return;

    for(int64_t i = 0; entries[i].path != NULL; i++)
        io_realloc(entries[i].path, 0);
            
    io_realloc(entries, 0);
}

bool directory_set_current_working(const char* new_working_dir, int64_t path_size)
{
    IO_NORMALIZE_PATH_OP(new_working_dir, path_size,
        bool state = _wchdir(wio_wstring(normalized_path)) == 0;
    )
    return state;
}
    
char* directory_get_current_working_malloc()
{
    //@TODO: use the local malloc
    wchar_t* current_working = _wgetcwd(NULL, 0);
    if(current_working == NULL)
        abort();

    WIO_Buffer output = wio_convert_to_utf8_normalize_path(current_working, (int64_t) wcslen(current_working), IO_NORMALIZE_LINUX | IO_NORMALIZE_DIRECTORY, NULL, 0);
    free(current_working);
    return wio_string(output);
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
    #define WIO_PATH_PREFIX_LONG        "\\\\?\\"
    Path_Info info = {0};

    //so that we dont have to keep variables in sync
    #define REMAINING_PATH (path + info.prefix_size)
    #define REMAINING_SIZE (path_size - info.prefix_size)

    if(wio_is_prefixed_with(REMAINING_PATH, REMAINING_SIZE, WIO_PATH_PREFIX_LONG))
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
        WIO_Buffer full = wio_malloc_full_path(wio_wstring(normalized_path), IO_NORMALIZE_LINUX);
    )
    return wio_string(full);
}
    
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
        for(int64_t j = 0; WIO_PATH_PREFIX_LONG[j]; j++)
            output[prefix_writing_to++] = WIO_PATH_PREFIX_LONG[j];
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

    //if empty and relative add ./
    if(writing_to == 0 && path_info.is_absolute == false)
    {
        prefixed_output[writing_to++] = '.';
        prefixed_output[writing_to++] = separator;
    }

    prefixed_output[writing_to] = '\0';
    assert(writing_to + prefix_writing_to < output_size);
    return writing_to + prefix_writing_to;
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
    
static void wio_normalize_allocate_path(const char* path, int64_t path_size, int flags, WIO_Buffer* output)
{
    wio_buffer_resize(output, path_size + IO_NORMALIZE_NEEDED_EXTRA_SIZE);  
    int64_t new_size = wio_path_normalize(path, path_size, flags, wio_string(*output), output->size);
    assert(new_size < output->size && "must suceed");
    wio_buffer_resize(output, new_size);  
}
    
static wchar_t* wio_wstring(WIO_Buffer stack)
{
    assert(stack.item_size == sizeof(wchar_t));
    return (wchar_t*) stack.data;
}

static char* wio_string(WIO_Buffer stack)
{
    assert(stack.item_size == sizeof(char));
    return (char*) stack.data;
}

static WIO_Buffer wio_buffer_init_backed(void* backing, int64_t backing_size, int32_t item_size)
{
    WIO_Buffer out = {0};
    out.data = backing;
    out.item_size = item_size;
    out.is_alloced = false;
    out.capacity = backing_size/item_size;
    memset(backing, 0, backing_size);

    return out;
}

static WIO_Buffer wio_buffer_init(int64_t item_size)
{
    WIO_Buffer out = {0};
    out.item_size = item_size;
    return out;
}

static void* wio_buffer_resize(WIO_Buffer* stack, int64_t new_size)
{
    int64_t i_size = stack->item_size;
    assert(i_size != 0);
    assert(stack->size <= stack->capacity);
    assert(stack->item_size != 0);

    if(new_size >= stack->capacity)
    {
        int64_t new_capaity = 8;
        while(new_capaity < new_size + 1)
            new_capaity *= 2;

        void* prev_ptr = NULL;
        if(stack->is_alloced)
            prev_ptr = stack->data;
        stack->data = io_realloc(prev_ptr, (size_t) (new_capaity * i_size));
        stack->is_alloced = true;

        //null newly added portion
        memset((char*) stack->data + stack->capacity*i_size, 0, (new_capaity - stack->capacity)*i_size);
        stack->capacity = new_capaity;
    }

    //Null terminates the buffer
    stack->size = new_size;
    memset((char*) stack->data + new_size*i_size, 0, i_size);
    return stack->data;
}

static int64_t wio_buffer_push(WIO_Buffer* stack, const void* item, int64_t item_size)
{
    if(stack->item_size == 0)
        stack->item_size = item_size;

    assert(stack->item_size == item_size);
    wio_buffer_resize(stack, stack->size + 1);

    memmove((char*) stack->data + (stack->size - 1)*item_size, item, item_size);
    return stack->size;
}

static int64_t wio_buffer_pop(WIO_Buffer* stack)
{
    assert(stack->size > 0);
    
    stack->size -= 1;
    return stack->size;
}

static void* wio_buffer_at(WIO_Buffer* stack, int64_t index)
{
    assert(0 <= index && index < stack->size);
    return (char*) stack->data + index*stack->item_size;
}

static void wio_buffer_deinit(WIO_Buffer* stack)
{
    if(stack->is_alloced)
        (void) io_realloc(stack->data, 0);
    
    WIO_Buffer empty = {0};
    *stack = empty;
}

static void wio_utf16_to_utf8(const wchar_t* utf16, int64_t utf16len, WIO_Buffer* output) 
{
    if (utf16 == NULL || utf16len == 0) 
    {
        wio_buffer_resize(output, 0);
        return;
    }

    int utf8len = WideCharToMultiByte(CP_UTF8, 0, utf16, (int) utf16len, NULL, 0, NULL, NULL);
    wio_buffer_resize(output, utf8len);
    WideCharToMultiByte(CP_UTF8, 0, utf16, (int) utf16len, wio_string(*output), (int) utf8len, 0, 0);
}
    
static void wio_utf8_to_utf16(const char* utf8, int64_t utf8len, WIO_Buffer* output) 
{
    if (utf8 == NULL || utf8len == 0) 
    {
        wio_buffer_resize(output, 0);
        return;
    }

    int utf16len = MultiByteToWideChar(CP_UTF8, 0, utf8, (int) utf8len, NULL, 0);
    wio_buffer_resize(output, utf16len);
    MultiByteToWideChar(CP_UTF8, 0, utf8, (int) utf8len, wio_wstring(*output), (int) utf16len);
}

static void wio_w_concat(const wchar_t* a, const wchar_t* b, const wchar_t* c, WIO_Buffer* output)
{
    int64_t a_size = a != NULL ? (int64_t) wcslen(a) : 0;
    int64_t b_size = b != NULL ? (int64_t) wcslen(b) : 0;
    int64_t c_size = c != NULL ? (int64_t) wcslen(c) : 0;
    int64_t composite_size = a_size + b_size + c_size;
        
    wio_buffer_resize(output, composite_size);
    memmove(wio_wstring(*output),                   a, sizeof(wchar_t) * a_size);
    memmove(wio_wstring(*output) + a_size,          b, sizeof(wchar_t) * b_size);
    memmove(wio_wstring(*output) + a_size + b_size, c, sizeof(wchar_t) * c_size);
}

WIO_Buffer wio_normalize_convert_to_utf16_path(const char* path, int64_t path_size, int flags, char* buffer, int64_t buffer_size)
{
    char norm_path_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    WIO_Buffer norm_path = wio_buffer_init_backed(norm_path_buffer, IO_LOCAL_BUFFER_SIZE, sizeof(char));

    WIO_Buffer output = wio_buffer_init_backed(buffer, buffer_size, sizeof(wchar_t));
    wio_normalize_allocate_path(path, path_size, IO_NORMALIZE_WINDOWS | flags, &norm_path);
    wio_utf8_to_utf16(wio_string(norm_path), norm_path.size, &output);
    wio_buffer_deinit(&norm_path);

    return output;
}
    
WIO_Buffer wio_convert_to_utf8_normalize_path(const wchar_t* path, int64_t path_size, int flags, char* buffer, int64_t buffer_size)
{
    char local_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    WIO_Buffer local = wio_buffer_init_backed(local_buffer, IO_LOCAL_BUFFER_SIZE, sizeof(char));
    wio_utf16_to_utf8(path, path_size, &local);

    WIO_Buffer output = wio_buffer_init_backed(buffer, buffer_size, sizeof(char));
    wio_normalize_allocate_path(wio_string(local), local.size, flags, &output);
    wio_buffer_deinit(&local);

    return output;
}

#define TESTING_DIR "__temp_wio_testing"
#define TESTING_DIR_ TESTING_DIR "/"

const char* const TESTING_FILE_PATHS[] = {
    TESTING_DIR_"file1.txt",
    TESTING_DIR_"no_extension_file",
    TESTING_DIR_"utf8_yey_file_šřžýá.txt",
    TESTING_DIR_"very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_long.ini",
    TESTING_DIR_"file2.ini",
    TESTING_DIR_"file3.ini",
};

const char* const TESTING_DIRECTORY_PATHS[] = {
    TESTING_DIR_"directory1",
    TESTING_DIR_"utf8_yey_dir_šřžýá",
    TESTING_DIR_"very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_very_long",
    TESTING_DIR_"directory2",
    TESTING_DIR_"directory3",
};

#define TESTING_ARRAY_SIZE(str) (sizeof(str) / sizeof(*str))
#define TESTING_PANIC_WITH(...) do { fprintf(stderr, __VA_ARGS__); abort(); } while(0)

#ifdef TEST
#undef TEST
#endif

#define TEST(a) if(!(a)) TESTING_PANIC_WITH("TEST: failed \"" #a "\" at " __FILE__ " line %d\n", __LINE__);  

#define TESTING_STRING_SPACE_MAX_CONCURENT 10
#define TESTING_STRING_SPACE_MAX_LEN 1024

int     TESTING_STRING_SPACE_CURRENT = 0;
char*   TESTING_STRING_SPACE[TESTING_STRING_SPACE_MAX_CONCURENT] = {NULL};

#define TESTING_PATH_INFO_ABSOLUTE 1
#define TESTING_PATH_INFO_DIRECTORY 2
#define TESTING_PATH_INFO_ABSOLUTE_LINUX 4
#define TESTING_PATH_INFO_ABSOLUTE_DRIVE 8

static int64_t current_alloced = 0;
void* wio_testing_allocator(void* prev, size_t new_size)
{
    const int64_t KILL_ZONE_VAL = 0xFFABFF00FFCDFF;
    const int64_t KILL_ZONE_SIZE = 4;

    typedef struct Allocation_Header {
        int64_t kill_zone1[KILL_ZONE_SIZE]; 
        int64_t alloc_size;
        int64_t kill_zone2[KILL_ZONE_SIZE]; 
    } Allocation_Header;

    size_t new_size_corrected = new_size + 2*sizeof(Allocation_Header);

    Allocation_Header* preamble = NULL;
    Allocation_Header* postamble = NULL;
    if(prev != NULL)
    {
        preamble = (Allocation_Header*) prev - 1;
        for(int64_t i = 0; i < KILL_ZONE_SIZE; i++)
        {
            TEST(preamble->kill_zone1[i] == KILL_ZONE_VAL); 
            TEST(preamble->kill_zone2[i] == KILL_ZONE_VAL); 
        }
            
        postamble = (Allocation_Header*) ((char*) prev + preamble->alloc_size);
        for(int64_t i = 0; i < KILL_ZONE_SIZE; i++)
        {
            TEST(postamble->kill_zone1[i] == KILL_ZONE_VAL); 
            TEST(postamble->kill_zone2[i] == KILL_ZONE_VAL); 
        }

        TEST(postamble->alloc_size == preamble->alloc_size);
    }
    
    if(preamble != NULL)
        current_alloced -= preamble->alloc_size;

    if(new_size == 0)
    {
        free(preamble);
        return NULL;
    }
    
    current_alloced += new_size;

    char* realloced = (char*) realloc(preamble, (size_t) new_size_corrected);
    TEST(realloced != NULL && "Allocation must not fail");
    preamble = (Allocation_Header*) realloced;

    preamble->alloc_size = (int64_t) new_size;
    for(int64_t i = 0; i < KILL_ZONE_SIZE; i++)
    {
        preamble->kill_zone1[i] = KILL_ZONE_VAL; 
        preamble->kill_zone2[i] = KILL_ZONE_VAL; 
    }
        
    postamble = (Allocation_Header*) (realloced + sizeof(Allocation_Header) + new_size);
    *postamble = *preamble;

    return preamble + 1;
}

char* wio_testing_temp_buffer()
{
    char* str = TESTING_STRING_SPACE[TESTING_STRING_SPACE_CURRENT];
    if(str == nullptr)
    {
        TESTING_STRING_SPACE[TESTING_STRING_SPACE_CURRENT] = (char*) io_realloc(NULL, TESTING_STRING_SPACE_MAX_LEN);
        str = TESTING_STRING_SPACE[TESTING_STRING_SPACE_CURRENT];
        assert(str != NULL);
    }

    TESTING_STRING_SPACE_CURRENT = (TESTING_STRING_SPACE_CURRENT + 1) % TESTING_STRING_SPACE_MAX_CONCURENT;
    
    assert(str != nullptr);
    memset(str, 0, TESTING_STRING_SPACE_MAX_LEN);
    return str;
}

bool wio_testing_has_file_with(const char* path, const char* content)
{
    FILE* file = fopen(path, "rb");
    if(file == NULL)
        return false;
        
    size_t content_len = strlen(content);
    char* buffer = (char*) io_realloc(NULL, content_len + 1);
    assert(buffer != NULL);

    size_t read = fread(buffer, 1, content_len, file);
    bool state = read == content_len;
    state = state && memcmp(buffer, content, content_len);

    io_realloc(buffer, 0);
    fclose(file);

    return state;
}

void wio_testing_make_file_with(const char* path, const char* content)
{
    FILE* file = fopen(path, "wb");
    if(file == NULL)
        TESTING_PANIC_WITH("TESTING: couldnt open file for wio_testing: \"%s\"", path);
        
    size_t content_len = strlen(content);
    size_t written = fwrite(content, 1, content_len, file);
    fclose(file);

    if(written != content_len)
        TESTING_PANIC_WITH("TESTING: write all contents to file \"%s\"\n conetnts: \"%s\"\n", path, content);
}


void wio_testing_make_directory(const char* path)
{
    if(_mkdir(path) != 0)
        TESTING_PANIC_WITH("TESTING: couldnt create directory: \"%s\"", path);
}

void wio_testing_deinit_filesystem()
{
    (void) setlocale(LC_ALL, ".UTF-8");

    for(int64_t i = 0; i < TESTING_ARRAY_SIZE(TESTING_FILE_PATHS); i++)
        (void) _unlink(TESTING_FILE_PATHS[i]);

    for(int64_t i = 0; i < TESTING_ARRAY_SIZE(TESTING_DIRECTORY_PATHS); i++)
        (void) _rmdir(TESTING_DIRECTORY_PATHS[i]);
        
    for(int64_t i = 0; i < TESTING_ARRAY_SIZE(TESTING_STRING_SPACE); i++)
    {
        free(TESTING_STRING_SPACE[i]);
        TESTING_STRING_SPACE[i] = NULL;
    }

    (void) _rmdir(TESTING_DIR);
}

void wio_testing_init_filesystem()
{
    wio_testing_deinit_filesystem();
    WIO_ALLOCATOR = wio_testing_allocator;
    wio_testing_make_directory(TESTING_DIR);
}

void test_normalize_path_single(const char* to_simplify, int style, const char* expected_result)
{
    char normalized_buffer[IO_LOCAL_BUFFER_SIZE] = {0}; 
    for(int64_t i = 0; i < IO_LOCAL_BUFFER_SIZE; i++)
        normalized_buffer[i] = '$'; //initialize to something else then null ermination

    WIO_Buffer normalized = wio_buffer_init_backed(normalized_buffer, IO_LOCAL_BUFFER_SIZE, sizeof(char));
        
    int64_t len = (int64_t) strlen(to_simplify);
    wio_normalize_allocate_path(to_simplify, len, style, &normalized);

    //int64_t expected_len = strlen(expected_result);
    int differ_at = strcmp(wio_string(normalized), expected_result);
    if(differ_at != 0)
    {
        printf("\nbefore:   \"%s\"\n", to_simplify);
        printf("simplify: \"%s\"\n", wio_string(normalized));
        printf("expected: \"%s\"\n", expected_result);
        printf("differ at: %d\n", differ_at);
        TESTING_PANIC_WITH("TES: test_normalize_path_single failed!");
    }

    wio_buffer_deinit(&normalized);
}

void test_path_get_info_single(const char* path, int64_t prefix, int64_t root, int64_t file, int64_t ext, int flag, char letter)
{
    int64_t len = (int64_t) strlen(path);
    Path_Info info = path_get_info(path, len);

    TEST(info.prefix_size == prefix);
    TEST(info.root_size == root);
    TEST(info.filename_size == file);
    TEST(info.extension_size == ext);

    TEST(info.is_absolute == !!(flag & TESTING_PATH_INFO_ABSOLUTE));
    TEST(info.is_linux_style_absolute == !!(flag & TESTING_PATH_INFO_ABSOLUTE_LINUX));
    TEST(info.is_drive_style_absolute == !!(flag & TESTING_PATH_INFO_ABSOLUTE_DRIVE));
    TEST(info.is_directory == !!(flag & TESTING_PATH_INFO_DIRECTORY));

    TEST(info.drive_letter == letter);
}

void test_normalize_path()
{
    //@TODO
    void* alloced = wio_testing_allocator(NULL, 18);
    wio_testing_allocator(alloced, 0);

    WIO_ALLOCATOR = wio_testing_allocator;

    //@TODO add wio_testing prefixes
    #define L_PREF WIO_PATH_PREFIX_LONG
    #define LONG_SEG "a_very_long_segement_name_to_test_the_max_size_limit"

    #define LONG_PATH LONG_SEG "/" LONG_SEG "/" LONG_SEG "/" LONG_SEG "/" LONG_SEG
    #define VERY_LONG_PATH LONG_PATH "/" LONG_PATH "/" LONG_PATH
    
    #define WIN_LONG_PATH LONG_SEG "\\" LONG_SEG "\\" LONG_SEG "\\" LONG_SEG "\\" LONG_SEG
    #define WIN_VERY_LONG_PATH WIN_LONG_PATH "\\" WIN_LONG_PATH "\\" WIN_LONG_PATH

    {
        int ABSL = TESTING_PATH_INFO_ABSOLUTE_LINUX | TESTING_PATH_INFO_ABSOLUTE;
        int ABSD = TESTING_PATH_INFO_ABSOLUTE_DRIVE | TESTING_PATH_INFO_ABSOLUTE;
        int DIR  = TESTING_PATH_INFO_DIRECTORY;
        
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
        test_path_get_info_single("C:/" VERY_LONG_PATH "/file.txt", 0, 3, 8, 3, ABSD, 'C');
        test_path_get_info_single("/" VERY_LONG_PATH "/",       0, 1, 0, 0, ABSL | DIR, 0);
        test_path_get_info_single(L_PREF "/" VERY_LONG_PATH "/",4, 5, 0, 0, ABSL | DIR, 0);
    }

    {
        int WIN = IO_NORMALIZE_WINDOWS;
        int LIN = IO_NORMALIZE_LINUX;
        int LWIN = IO_NORMALIZE_WINDOWS | IO_NORMALIZE_LONG;

        int D = IO_NORMALIZE_DIRECTORY;
        int F = IO_NORMALIZE_FILE;

        //test_normalize_path_single("path/to/file.txt",             WIN,  "path\\to\\file.txt");
        //test_normalize_path_single("path///to//file.txt",          WIN,  "path\\to\\file.txt");
        //test_normalize_path_single("C:path/to/file.txt",           WIN,  "C:\\path\\to\\file.txt");
        //test_normalize_path_single("c:/path/to/file.txt",          WIN,  "C:\\path\\to\\file.txt");
        //test_normalize_path_single("g:\\path/to/file.txt",         WIN,  "G:\\path\\to\\file.txt");
        //test_normalize_path_single("z:path/to/file.txt",           WIN,  "Z:\\path\\to\\file.txt");
        //test_normalize_path_single("\\path/to/file.txt",           WIN,  "\\path\\to\\file.txt");
        //test_normalize_path_single("path\\to\\file.txt",           LIN,  "path/to/file.txt");
            
        test_normalize_path_single("",                             WIN,  ".\\");
        test_normalize_path_single("",                             WIN | D, ".\\");
        test_normalize_path_single("",                             WIN | F, ".\\");
        test_normalize_path_single("",                             LIN,  "./");
        test_normalize_path_single("",                             LIN | D,  "./");
        test_normalize_path_single("C:/",                          LIN | D,  "C:/");
        test_normalize_path_single("\\",                           LIN,  "/");
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
        test_normalize_path_single("z:" VERY_LONG_PATH "/f.txt",   LIN |  D, "Z:/" VERY_LONG_PATH "/f.txt/");
        test_normalize_path_single("z:" VERY_LONG_PATH "/f.txt",   WIN |  D, L_PREF "Z:\\" WIN_VERY_LONG_PATH "\\f.txt\\");
        test_normalize_path_single("z:" VERY_LONG_PATH "/f.txt",   LWIN | D, L_PREF "Z:\\" WIN_VERY_LONG_PATH "\\f.txt\\");
        test_normalize_path_single("/" VERY_LONG_PATH "/f.txt/",   WIN | F, L_PREF "\\" WIN_VERY_LONG_PATH "\\f.txt");
        test_normalize_path_single(L_PREF "/" VERY_LONG_PATH "/f.txt/f/../",   WIN | F, L_PREF "\\" WIN_VERY_LONG_PATH "\\f.txt");

        test_normalize_path_single("path/to/./file.txt",           WIN | D, "path\\to\\file.txt\\");
        test_normalize_path_single("path/./to/file.txt\\",         WIN | D, "path\\to\\file.txt\\");
        test_normalize_path_single("path/to/file.txt",             WIN | F, "path\\to\\file.txt");
        test_normalize_path_single("path/to/file.txt/",            WIN | F, "path\\to\\file.txt");
        test_normalize_path_single("path/to/file.txt",             LWIN | D, L_PREF "path\\to\\file.txt\\");
    }
}

void test_file_handle_functions()
{
    const char test_string1[] = "hello world!";
    const char test_string2[] = "hello world! longer string ýíýščěšěšč";
    const char test_string3[] = "utf8 yey! ýíýščěšěšč";

    int64_t test_string1_size = (int64_t) strlen(test_string1);
    int64_t test_string2_size = (int64_t) strlen(test_string2);
    int64_t test_string3_size = (int64_t) strlen(test_string3);

    const char* path1 = TESTING_FILE_PATHS[0];
    const char* path2 = TESTING_FILE_PATHS[1];
    const char* path3 = TESTING_FILE_PATHS[2];
    const char* path4 = TESTING_FILE_PATHS[3];
    const char* dir_path1 = TESTING_DIRECTORY_PATHS[0];
    const char* dir_path2 = TESTING_DIRECTORY_PATHS[1];

    
    time_t before_proc = time(NULL);
    time_t before_last_access = time(NULL);

    wio_testing_init_filesystem();

    wio_testing_make_file_with(path1, test_string1);
    wio_testing_make_file_with(path2, test_string2);
    wio_testing_make_directory(dir_path1);
    wio_testing_make_directory(dir_path2);
    
    File_IO_State state = FILE_IO_STATE_OK;
    int64_t processed = 0;

    {
        char* buffer = wio_testing_temp_buffer();
        File file = {0};
        TEST(file_is_open(file) == false);

        file = file_open(path1, strlen(path1), FILE_OPEN_READ);
        TEST(file_is_open(file));

        processed = file_read(&file, buffer, test_string1_size, &state);
        TEST(state == FILE_IO_STATE_OK);
        TEST(processed == test_string1_size);
        TEST(strcmp(buffer, test_string1) == 0);
        
        processed = file_read(&file, buffer+test_string1_size, test_string1_size, &state);
        TEST(state == FILE_IO_STATE_EOF);
        TEST(processed == 0);
        TEST(strcmp(buffer, test_string1) == 0 && "the function shouldnt write anything");

        file_close(&file);
        TEST(file_is_open(file) == false);
    }
    
    {
        char* buffer = wio_testing_temp_buffer();
        File file = file_open(path2, strlen(path2), FILE_OPEN_READ | FILE_OPEN_ALLOW_OTHER_DELETE | FILE_OPEN_ALLOW_OTHER_WRITE);
        TEST(file_is_open(file));

        processed = file_read(&file, buffer, test_string2_size + 1, &state);
        TEST(state == FILE_IO_STATE_EOF);
        TEST(processed == test_string2_size);
        TEST(strcmp(buffer, test_string2) == 0);
        
        processed = file_read(&file, buffer, 0, &state);
        TEST(state == FILE_IO_STATE_OK);
        TEST(processed == 0);
        TEST(strcmp(buffer, test_string2) == 0 && "the function shouldnt write anything");

        file_close(&file);
    }
    
    {
        char* buffer = wio_testing_temp_buffer();
        File file = file_open(path3, strlen(path3), FILE_OPEN_READ | FILE_OPEN_ALLOW_OTHER_READ);
        TEST(file_is_open(file) == false && "file at path3 is not yet created");

        file = file_open(dir_path1, strlen(dir_path1), FILE_OPEN_READ | FILE_OPEN_ALLOW_OTHER_READ);
        TEST(file_is_open(file) == false && "path3 is a directory created");

        file = file_open(path3, strlen(path3), FILE_OPEN_READ_WRITE | FILE_OPEN_CREATE_ELSE_FAIL);
        TEST(file_is_open(file));

        processed = file_write(&file, test_string3, test_string3_size, &state);
        TEST(state == FILE_IO_STATE_OK);
        TEST(processed == test_string3_size);

        int64_t offset = file_tell(file);
        TEST(offset == test_string3_size && "the offset should be precisely the ammount written");
        TEST(file_seek(&file, offset, FILE_SEEK_START));
        offset = file_tell(file);
        TEST(offset == test_string3_size && "seeking the current offset should have no effect");

        TEST(file_seek(&file, 0, FILE_SEEK_START));
        
        processed = file_read(&file, buffer, test_string3_size + 1, &state);
        TEST(state == FILE_IO_STATE_EOF);
        TEST(processed == test_string3_size);
        TEST(strcmp(buffer, test_string3) == 0);

        file_close(&file);

        file = file_open(path3, strlen(path3), FILE_OPEN_READ_WRITE | FILE_OPEN_CREATE_ELSE_FAIL);
        TEST(file_is_open(file) == false && "file at path3 is already created");
    }
    
    time_t before = time(NULL);
    time_t after = time(NULL);
    {
        char* buffer = wio_testing_temp_buffer();
        File file = file_open(path4, strlen(path4), FILE_OPEN_READ_WRITE | FILE_OPEN_CREATE_ELSE_FAIL);
        TEST(file_is_open(file));
        time_t created = time(NULL);

        int64_t total_written = 0;
        const int64_t iters = 30;
        for(int64_t i = 0; i < iters; i++)
        {
            before_last_access = time(NULL);
            total_written += file_write(&file, test_string3, test_string3_size, &state);
            TEST(state == FILE_IO_STATE_OK);
        }

        while(created == time(NULL)); //busy wait for a second.
        after = time(NULL);

        File_Info info = {0};
        TEST(file_info(path4, strlen(path4), &info));
        TEST(info.type == FILE_TYPE_FILE);
        TEST(total_written == info.size);
        TEST((int64_t) before <= info.created_time      && info.created_time     < (int64_t) after);
        TEST((int64_t) before <= info.last_access_time  && info.last_access_time < (int64_t) after);
        TEST((int64_t) before <= info.last_write_time   && info.last_write_time  < (int64_t) after);

        file_close(&file);

    }
    
    {
        File_Info info = {0};
        TEST(file_info(TESTING_DIR, strlen(TESTING_DIR), &info));
        TEST(info.type == FILE_TYPE_DIRECTORY);
        TEST(info.size >= test_string1_size + test_string2_size + test_string3_size);
        time_t now = time(NULL);
        
        TEST((int64_t) before_proc <= info.created_time            && info.created_time <= now);
        TEST((int64_t) before_last_access <= info.last_access_time && info.last_access_time <= now);
        TEST((int64_t) before_last_access <= info.last_write_time  && info.last_write_time  <= now);

        int y = 0;
    }

    {
        char* buffer = wio_testing_temp_buffer();

        
        File_Info info = {0};
        TEST(file_info(path4, strlen(path4), &info));

        TEST(file_info(path4, strlen(path4), &info));
        TEST(info.type == FILE_TYPE_FILE);
        TEST(info.size > 30);
        TEST((int64_t) before <= info.created_time      && info.created_time     < (int64_t) after);
        TEST((int64_t) before <= info.last_access_time  && info.last_access_time < (int64_t) after);
        TEST((int64_t) before <= info.last_write_time   && info.last_write_time  < (int64_t) after);
        
        time_t before_trim = time(NULL);
        File file = file_open(path4, strlen(path4), FILE_OPEN_READ_WRITE);
        TEST(file_is_open(file));
        TEST(file_trim(&file, 30));
        time_t after_trim = time(NULL);

        TEST(file_info(path4, strlen(path4), &info));
        TEST(info.size == 30);
        TEST((int64_t) before <= info.created_time           && info.created_time     < (int64_t) after);
        TEST((int64_t) before_trim <= info.last_access_time  && info.last_access_time <= (int64_t) after_trim);
        TEST((int64_t) before_trim <= info.last_write_time   && info.last_write_time  <= (int64_t) after_trim);

        file_close(&file);
    }

    int x = 10;
}

void test_make_dir()
{

}

void test_io()
{
    test_normalize_path();
    test_file_handle_functions();
    test_make_dir();
    //wio_testing_deinit_filesystem();
}

JOT_IO_END