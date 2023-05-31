#pragma once

#include <stddef.h>
#include <stdint.h>
#include <time.h>


//This is a simple filesystem io wrapper. It focuses on simplicity and usability.

//This interface does NOT support sockets, pipes, network drives, 
//windows UNC (unified naming convention), windows \\.\ prefix (physical drive path). 
//If you need those expand the interface or use a proper library

//All paths have the following properties:
// 1: All paths are in utf8
// 2: All paths can be of any length
// 3: User supllied paths can use both / and \ as separators
// 4: User supllied paths can be relative or absolute
// 5: Returned paths use / as separators
// 6: Returned paths are in the normalized absolute form (see below)
// 7: Returned directory paths always end in /
// 8: Returned absolute paths have drive letter capitilized

//Normalized form:
//  1: Absolute
//     a: "C:/folder/file.txt"
//     b: "/folder/file.txt"
//  
//     the roots portions of the string are:
//     a) "C:/" (note that drive letter will always be uppercase!)
//     b) "/"
// 
//     The above are the only supported absolute path styles. 
//     We dont support windows UNC or physical drives
// 
//  2: Relative paths
//     - folder/file.txt
//     - ./ (when refering to the current directory because / means absolute root)
//     - ../
//     - folder/
// 
//     In particular in normalized relative path the ../ can never appear anywhere besides the start of the path
//     and ./ is not present

//Accepted form:
// 1: Any number of "../" and "./" fragments anywhere in the string
//    example: "C:/../././../file.txt" -> "C:/file.txt"
//             "C:/folder/.././file.txt" -> "C:/file.txt"
// 
// 2: Folder path when refering to file and file path when refering to folder 
//    passed into file_open: "C:/file.txt/" -> "C:/file.txt"
//
// 3: The windows long path prefix 
//    example: "//?/C:/long/path/to/file.txt" -> "C:/long/path/to/file.txt"
//
// 4: Both styles of drive letter separators
//    example: "C:\folder\file.txt" -> "C:/folder/file.txt"
//    example: "C:/folder\file.txt" -> "C:/folder/file.txt"
//
// 5: Missing drive letter separator
//    example: "C:folder\file.txt" -> "C:/folder/file.txt"
//
// 6: Lovercase drive letter 
//    example: "c:/folder\file.txt" -> "C:/folder/file.txt"

//How to work with paths:
//
// 1) splitting
//    Normalize the path and call the path_get_info() and use it to strip the path off its root and filename (you can add them back in the end)
//    split the remainder into path fragments.
// 
// 2) Concatenating
//    Normalize all inputs then (assert all paths are relative!) join the strings together then normalize the output.
//    (Note that the normalization of imputs is necessary. If we were to concatenate "" / "my_path/to_file/" we would
//     get "/my_path/to_file/" which is no longer relative!. If we normalize the inputs we get "./" / "my_path/to_file"
//     which produces "./my_path/to_file/" which gets normalized to "my_path/to_file/")
//
//    Alterantively we could check each of the inputs if it is empty and if that is the case skip it. Then only normalize at end
//    This approach is of course much faster.

#ifndef JOT_IO_C
    #ifndef __cplusplus
        #define JOT_IO_C
    #endif // !cplusplus
#endif

#ifdef JOT_IO_C
#include <stdbool.h>

#define JOT_IO_BEGIN 
#define JOT_IO_END 
#define CREF 
#else
#define JOT_IO_BEGIN namespace jot{
#define JOT_IO_END   }
#define CREF const&
#endif

#define JAPI

JOT_IO_BEGIN

typedef enum File_Open_Mode 
{
    FILE_OPEN_READ = 1,
    FILE_OPEN_WRITE = 2,
    FILE_OPEN_READ_WRITE = 1 | 2,

    FILE_OPEN_CREATE = 4,           //the file can exist or not not in both cases it is opened
    FILE_OPEN_CREATE_ELSE_FAIL = 8, //if the file does exist fail
    //FILE_OPEN_TRANSLATE = 16,  //probably remove
    //FILE_OPEN_TEMPORARY = 32,  //@TODO: implement

    FILE_OPEN_ALLOW_OTHER_READ = 64,
    FILE_OPEN_ALLOW_OTHER_WRITE = 128,
    FILE_OPEN_ALLOW_OTHER_DELETE = 256,
} File_Open_Mode;

typedef enum File_Seek
{
    FILE_SEEK_START = 0,
    FILE_SEEK_CURRENT = 1,
    FILE_SEEK_END = 2,
} File_Seek;

typedef enum File_Type
{
    FILE_TYPE_NOT_FOUND = 0,
    FILE_TYPE_FILE = 1,
    FILE_TYPE_DIRECTORY = 4,
    FILE_TYPE_CHARACTER_DEVICE = 2,
    FILE_TYPE_PIPE_ = 3,
    FILE_TYPE_OTHER = 5,
} File_Type;

typedef enum File_IO_State {
    FILE_IO_STATE_OK = 0,
    FILE_IO_STATE_ERROR = 1,
    FILE_IO_STATE_EOF = 2,
    FILE_IO_STATE_FILE_CLOSED = 3,
} File_IO_State;

typedef struct File
{
    int open_mode;
    uint64_t state[4];
        
    #ifndef JOT_IO_C
    //takes non explicit dummy to allow = {0} initialization
    File(int dummy = 0); 
    ~File() noexcept;

    File(File && other) noexcept;
    File& operator =(File&& other) noexcept;

    File(File const&) = delete;
    File& operator =(File const&) = delete;
    #endif
} File;

typedef struct File_Info
{
    int64_t size;
    File_Type type;
    time_t created_time;
    time_t last_write_time;  
    time_t last_access_time; //The last time file was either read or written
    bool is_link; //if file/dictionary is actually just a link (hardlink or softlink or symlink)
} File_Info;
    
typedef struct Path_Info
{
    int64_t prefix_size;
    int64_t root_size;
    int64_t filename_size;
    int64_t extension_size;
    bool is_absolute;
    bool is_directory;
    bool is_linux_style_absolute;
    bool is_drive_style_absolute;
    char drive_letter; //only set when is_drive_style_absolute is true!
} Path_Info;

typedef struct Directory_Entry
{
    char* path;
    int64_t path_size;
    int64_t index_within_directory;
    int64_t directory_depth;
    File_Info info;
} Directory_Entry;

typedef struct IO_Allocator {
    void* (*reallocate)(void* old_data, size_t new_size, void* context);
    void* context;
} IO_Allocator;

//Sets the default allocator for this module
JAPI void io_set_allocator(IO_Allocator allocator);
//Queries the currently set default alloctaor for this module
JAPI IO_Allocator io_get_allocator();
//Reallocates memory by the currenlty set allocator
JAPI void* io_realloc(void* allocated, size_t size);
//Allocates memory by the currenlty set allocator
JAPI void* io_malloc(size_t size);
//Frees memory by the currenlty set allocator
JAPI void  io_free(void* ptr);

//Opens a file given the open_mode and permission_mode.
JAPI File file_open(const char* path, int64_t path_size, int open_mode);
//Closes previously opened file
JAPI void file_close(File* file);
//returns true if the provied file is open
JAPI bool file_is_open(File CREF file);

//Attempts to read up to size bytes from file into read_into.
JAPI int64_t file_read(File* file, void* read_into, int64_t size, File_IO_State* state);
//Attempts to write up to size bytes into file from write_from.
JAPI int64_t file_write(File* file, const void* write_from, int64_t size, File_IO_State* state);

//Offsets the current possition in file by offset relative to from. If offset is illegal (ie. negative when from start) fails.
JAPI bool  file_seek(File* file, int64_t offset, File_Seek from);
//Returns the current offset in bytes from start of the file. This can also be used as argument to file_seek.
//If fails returns -1.
JAPI int64_t file_tell(File CREF file);
//trims the file so that it is not larger than max_size. If the current reading position is greater than max_size it is rewinded back to max_size.
JAPI bool file_trim(File* file, int64_t max_size);

//retrieves info about the specified file or directory
JAPI bool file_info(const char* file_path, int64_t path_size, File_Info* info);
//Creates an empty file at the specified path
JAPI bool file_create(const char* file_path, int64_t path_size);
//Removes a file at the specified path
JAPI bool file_remove(const char* file_path, int64_t path_size);
//Moves or renames a file. If the file cannot be found or renamed to file already exists fails
JAPI bool file_move(const char* new_path, int64_t new_path_size, const char* old_path, int64_t old_path_size);
//Copies a file. If the file cannot be found or copy_to_path file already exists fails
JAPI bool file_copy(const char* copy_to_path, int64_t to_path_size, const char* copy_from_path, int64_t from_path_size);

//Makes an empty directory
JAPI bool directory_create(const char* dir_path, int64_t path_size);
//Removes an empty directory
JAPI bool directory_remove(const char* dir_path, int64_t path_size);
//changes the current working directory to the new_working_dir.  
JAPI bool directory_set_current_working(const char* new_working_dir, int64_t path_size);    
//Retrieves the current working directory as allocated string. Needs to be freed using io_free()
JAPI char* directory_get_current_working_malloc();    

//Gathers and allocates list of files in the specified directory. Saves a pointer to array of entries to entries and its size to entries_count. 
//Needs to be freed using directory_list_contents_free()
JAPI bool directory_list_contents_malloc(const char* directory_path, int64_t path_size, Directory_Entry** entries, int64_t* entries_count, bool recursive);
//Frees previously allocated file list
JAPI void directory_list_contents_free(Directory_Entry* entries);
   
//Paths are decomposed into the following: 
// path:      "//?/C:/path/to/file.txt"
// prefix:    "//?/" (never present in normalized)
// root:      "C:/"
// filename:  "file.txt"
// extension: "txt"

//Does basic parsing of the path. Does not involve any filesystem calls.
JAPI Path_Info path_get_info(const char* path, int64_t path_size);

//Converts a relative (or absolute) path to absolute path but does not validate if the target path is actually valid.
//Returns an allocated string that needs to be freed using io_free()
JAPI char* path_get_full_malloc(const char* path, int64_t path_size);   
//attempst to normalize into the provided buffer. Does not involve any filesystem calls. 
//The norm_as_filetype takes values of File_Type specifies if the path should be transformed into directory or file path.
//If any other values such as 0 are given does leave the path directory/file state as it is.
//Returns the size of the written string not including null termination.
//When the buffer is too small returns size bigger than buffer_size and the buffer should grow (generally by twice its size).
JAPI int64_t path_normalize(const char* path, int64_t path_size, int norm_as_filetype, char* buffer, int64_t buffer_size);

JOT_IO_END
