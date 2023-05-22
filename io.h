#pragma once

#include <stddef.h>
#include <stdint.h>
#include <time.h>


//This is a simple filesystem io wrapper. It focuses on simplicity and usability.
//Note that all file IO is unbuffered.

//This interface does NOT support sockets, pipes, sym links, network drives, 
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

JOT_IO_BEGIN

enum File_Open_Mode 
{
    FILE_OPEN_READ = 1,
    FILE_OPEN_WRITE = 2,
    FILE_OPEN_READ_WRITE = 1 | 2,

    FILE_OPEN_CREATE = 4,           //the file can exist or not not in both cases it is opened
    FILE_OPEN_CREATE_ELSE_FAIL = 8, //if the file does exist fail
    FILE_OPEN_TRANSLATE = 16,
    FILE_OPEN_TEMPORARY = 32,

    FILE_OPEN_ALLOW_OTHER_READ = 64,
    FILE_OPEN_ALLOW_OTHER_WRITE = 128,
    FILE_OPEN_ALLOW_OTHER_DELETE = 256,
};

enum File_Seek
{
    FILE_SEEK_CURRENT = SEEK_CUR,
    FILE_SEEK_START = SEEK_SET,
    FILE_SEEK_END = SEEK_END,
};

enum File_Type
{
    FILE_TYPE_NOT_FOUND = 0,
    FILE_TYPE_FILE = 1,
    FILE_TYPE_DIRECTORY = 4,
    FILE_TYPE_CHARACTER_DEVICE = 2,
    FILE_TYPE_PIPE_ = 3,
    FILE_TYPE_OTHER = 5,
};

typedef struct File
{
    int open_mode;
    uint64_t state[4];
        
    #ifndef JOT_IO_C
    File();
    ~File() noexcept;

    File(File && other) noexcept;
    File& operator =(File&& other) noexcept;

    File(File const&) = delete;
    File& operator =(File const&) = delete;
    #endif
} File;

//FILE_ATTRIBUTE_DIRECTORY
typedef struct File_IO_Result
{
    int64_t processed_size;  // how many bytes were successfully read/written. If is not the requested ammount then error or eof was encountered
    int64_t error;           // has the value of errno at the time of error or 0 if no error ofccured
    bool eof;              // if end of file was reached
    bool file_closed;      // if file was not opened
} File_IO_Result;

typedef struct File_Info
{
    File_Type type;
    int64_t size;
    time_t created_time;
    time_t last_write_time;  
    time_t last_access_time; //The last time file was either read or written
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
    bool is_directory;
} Directory_Entry;

//Opens a file given the open_mode and permission_mode.
File file_open(const char* path, int64_t path_size, int open_mode);
//Closes previously opened file
void file_close(File* file);
//returns true if the provied file is open
bool file_is_open(File CREF file);

//Attempts to read up to size bytes from file into read_into.
File_IO_Result file_read(File* file, void* read_into, int64_t size);
//Attempts to write up to size bytes into file from write_from.
File_IO_Result file_write(File* file, const void* write_from, int64_t size);

//Offsets the current possition in file by offset relative to from
bool  file_seek(File* file, int64_t offset, File_Seek from);
//Returns the current position in file relative to start of the file. This can be used as argument to file_seek
int64_t file_tell(File CREF file);

bool file_create(const char* file_path, int64_t path_size);
bool file_remove(const char* file_path, int64_t path_size);
//moves or renames a file. If the file cannot be found or renamed to file already exists fails
bool file_move(const char* new_path, int64_t new_path_size, const char* old_path, int64_t old_path_size);
bool file_copy(const char* copy_to_path, int64_t to_path_size, const char* copy_from_path, int64_t from_path_size);
bool file_info(const char* file_path, int64_t path_size, File_Info* info);

bool directory_create(const char* dir_path, int64_t path_size);
bool directory_remove(const char* dir_path, int64_t path_size);
bool directory_move(const char* new_path, int64_t new_path_size, const char* old_path, int64_t old_path_size);
//changes the current working directory to the new_working_dir.  
bool directory_set_current_working(const char* new_working_dir, int64_t path_size);    
char* directory_get_current_working_malloc();    

//Gathers and allocates list of files in the current directory.
int64_t directory_list_contents_malloc(const char* directory_path, int64_t path_size, Directory_Entry** entries);
//Frees previously allocated file list
void  directory_list_contents_free(Directory_Entry* entries);
   
//Paths are decomposed into the following: 
// path:      "//?/C:/path/to/file.txt"
// prefix:    "//?/" (never present in normalized)
// root:      "C:/"
// filename:  "file.txt"
// extension: "txt"

//Does basic parsing of the path. Does not involve any filesystem calls.
Path_Info path_get_info(const char* path, int64_t path_size);

//converts a relative (or absolute) path to absolute path. Writes to buffer up to buffer_capacity characters (including the null termination).
//Returns the needed size for the whole path which may be bigger than buffer_capacity.
char* path_get_full_malloc(const char* path, int64_t path_size);   
//Normalizes path into normalized form. Does not involve any filesystem calls.
//The norm_as_filetype takes values of File_Type specifies if the path should be transformed into directory or file path.
//If any other values such as 0 are given does leave the path directory/file state as it is.
char* path_normalize_malloc(const char* path, int64_t path_siz, int norm_as_filetype);
//attempst to normalize into the provided buffer. Does not involve any filesystem calls. 
//The norm_as_filetype takes values of File_Type specifies if the path should be transformed into directory or file path.
//If any other values such as 0 are given does leave the path directory/file state as it is.
//Returns the size of the written string not including null termination.
//When the buffer is too small returns size bigger than buffer_size and the buffer should grow (generally by twice its size).
int64_t path_normalize(const char* path, int64_t path_size, int norm_as_filetype, char* buffer, int64_t buffer_size);
//Returns true if path leads to file or directory. Also reports if the found item is directory. The is_directory argument can be null.
bool path_validate(const char* path, int64_t path_size, bool* is_directory);

JOT_IO_END
