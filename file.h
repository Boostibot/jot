#pragma once

#define _LARGEFILE64_SOURCE
#define _LARGE_TIME_API
#define _POSIX_SOURCE
#define _CRT_NONSTDC_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS_GLOBALS
#pragma warning(disable : 4996)

#include <stdio.h>
#include <assert.h>
#include <locale.h>
#include <stddef.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "unistd.h"

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

using isize = ptrdiff_t;

namespace jot 
{
    using File_Descriptor = int;
    using File_Stats = Stat64;

    //A thin wrapper around the posix file interface
    // handles closing the file automatically
    struct File 
    {
        File_Descriptor descriptor = -1;
        File() {}
        explicit File(File_Descriptor from) : descriptor(from) {}
        File(File && other) noexcept;
        File(File const&) = delete;

        ~File() noexcept;
    };

    //Shared flags
    enum class File_Open_Mode 
    {
        READ = O_RDONLY,
        WRITE = O_WRONLY,
        READ_WRITE = O_RDWR,

        APPEND = O_APPEND,
        CREATE = O_CREAT,
        EXCLUSIVE = O_EXCL,
        TRUNCATE = O_TRUNC,

        WINDOWS_TEXT       = O_TEXT,
        WINDOWS_BINARY     = O_BINARY,
        WINDOWS_RAW        = O_BINARY,
        WINDOWS_TEMPORARY  = O_TEMPORARY,
        WINDOWS_NOINHERIT  = O_NOINHERIT,
        WINDOWS_SEQUENTIAL = O_SEQUENTIAL,
        WINDOWS_RANDOM     = O_RANDOM,
        
        LINUX_DSYNC = O_DSYNC,
        LINUX_NOCTTY = O_NOCTTY,
        LINUX_NONBLOCK = O_NONBLOCK,
        LINUX_RSYNC = O_RSYNC,
        LINUX_SYNC = O_SYNC,
    };

    enum class File_Permission_Mode 
    {
        IRWXU = 00700, //user (file owner) has read, write, and execute permission
        IRUSR = 00400, //user has read permission
        IWUSR = 00200, //user has write permission
        IXUSR = 00100, //user has execute permission
        IRWXG = 00070, //group has read, write, and execute permission
        IRGRP = 00040, //group has read permission
        IWGRP = 00020, //group has write permission
        IXGRP = 00010, //group has execute permission
        IRWXO = 00007, //others have read, write, and execute permission
        IROTH = 00004, //others have read permission
        IWOTH = 00002, //others have write permission
        IXOTH = 00001, //others have execute permission

        //??windows
        IFMT   = 0xF000, // File type mask
        IFDIR  = 0x4000, // Directory
        IFCHR  = 0x2000, // Character special
        IFIFO  = 0x1000, // Pipe
        IFREG  = 0x8000, // Regular
        IREAD  = 0x0100, // Read permission, owner
        IWRITE = 0x0080, // Write permission, owner
        IEXEC  = 0x0040, // Execute/search permission, owner
    };

    enum class File_Access_Permission : int {
        READ = R_OK,
        WRITE = W_OK,
        EXECUTE = X_OK,
        EXISTS = F_OK,
    };

    enum class Seek_From : int 
    {
        Begin = SEEK_SET,
        End = SEEK_CUR,
        Current = SEEK_END,
    };

    static const File_Permission_Mode DEFAULT_OPEN_MODE = (File_Permission_Mode) 0644;

    //this is a wild guess cause the standard only says more than 32767B but on probably all systems
    // the following should hold on all systems see: https://stackoverflow.com/a/29723318
    static const isize MAX_READ_WRITE_CHUNK = (1 << 30) - 1;
        
    static File_Open_Mode operator |(File_Open_Mode a, File_Open_Mode b) 
    {
        return (File_Open_Mode) ((int) a | (int) b);
    }
    static File_Permission_Mode operator |(File_Permission_Mode a, File_Permission_Mode b) 
    {
        return (File_Permission_Mode) ((int) a | (int) b);
    }
    
    namespace file_globals
    {
        //Rewires all open/close operations so that they save their descriptors into its internal buffer 
        // intstead of the default global one. Calls close on all yet not closed descriptors in destructor.
        //Is used for sanboxing big parts of a code where we use longjump and thus cannot trust destructors.
        struct File_Guard_Swap;

        inline isize* descriptors_size_ptr()
        {
            thread_local static isize size = 0;
            return &size;
        }

        inline File_Descriptor** descriptors_ptr()
        {
            thread_local static File_Descriptor* descriptors_ptr = nullptr;
            return &descriptors_ptr;
        }

        inline File_Descriptor* descriptors()
        {
            return *descriptors_ptr();
        }

        //returns index of descriptor or -1 if not found or -2 if no guard is set
        static isize find_descriptor(isize value);
    }

    static File open(const char* filename, File_Open_Mode oflag = File_Open_Mode::READ_WRITE | File_Open_Mode::WINDOWS_BINARY, File_Permission_Mode pmode = DEFAULT_OPEN_MODE) noexcept
    {
        isize open_i = file_globals::find_descriptor(-1);
        if(open_i == -1)
            return File{-1};

        File_Descriptor descriptor = ::open(filename, (int) oflag, (mode_t) (int) pmode);
        if(descriptor < 0)
            return File{-1};
            
        if(open_i >= 0) 
            file_globals::descriptors()[open_i] = descriptor;
            
        return File{descriptor};
    }

    static File create(const char* filename, File_Open_Mode oflag = File_Open_Mode::READ_WRITE | File_Open_Mode::WINDOWS_BINARY | File_Open_Mode::CREATE, File_Permission_Mode pmode = DEFAULT_OPEN_MODE) noexcept
    {
        return open(filename, oflag, pmode);
    }

    static bool close(File file) noexcept
    {
        if(file.descriptor < 0)
            return true;
        
        isize used_i = file_globals::find_descriptor(file.descriptor);
        if(used_i >= 0)
            file_globals::descriptors()[used_i] = -1;
            
        bool state = ::close(file.descriptor) == 0;
        file.descriptor = -1;
        return state;
    }

    static bool has_access(const char* path, File_Access_Permission permission) noexcept
    {
        return ::access(path, (int) permission) != -1;
    }

    static File copy(File const& file) noexcept
    {
        return File(::dup(file.descriptor));
    }

    static bool copy(File const& file, File* to) noexcept
    {
        return ::dup2(file.descriptor, to->descriptor) == 0;
    }

    //Truncates file specified by file to specified size in bytes
    static bool truncate(File* file, long size) noexcept
    {
        return ::ftruncate(file->descriptor, (long) size) == 0;
    }

    //deletes a name and possibly the file it refers to
    // the file remains in existance untill all other processes
    // closed the file only then it is deleted.
    static bool unlink(const char* filename) noexcept
    {
        return ::unlink(filename) == 0;
    }

    static File to_file(FILE* stream) noexcept
    {
        return File(::fileno(stream));
    }

    static FILE* to_c_file(File file, const char* mode) noexcept
    {
        return ::fdopen(file.descriptor, mode);
    }

    //Fills buffer with the current dir const char*. If it is too small mallocs it instead
    // returns a pointer to the potentionally alloced destination. This needs to be freed
    // Might also fail and return NULL instead.
    static const char* fill_or_alloc_current_dir_cstring(char *buffer, int maxlen) noexcept
    {
        return ::getcwd(buffer, maxlen);
    }

    static bool change_dir(const char* dirname) noexcept
    {
        return ::chdir(dirname) == 0;
    }

    static bool is_open(File const& file) noexcept
    {
        return file.descriptor >= 0;
    }

    static bool is_character_device(File const& file) noexcept
    {
        return is_open(file) && ::isatty(file.descriptor) != 0;
    }

    static isize seek(File* file, isize offset, Seek_From from = Seek_From::Begin) noexcept
    {
        if(from == Seek_From::Begin)
            assert(offset >= 0 && "cannot seek before start");
        
        if(from == Seek_From::End)
            assert(offset <= 0 && "cannot seek past end");
        
        off64_t cast_offset = (off64_t) offset;
        return (isize) ::lseek64(file->descriptor, cast_offset, (int) from);
    } 

    static isize tell(File const& file) noexcept
    {
        return (isize) ::tell64(file.descriptor);
    }

    static bool make_dir(const char* dirname) noexcept
    {
        return ::mkdir(dirname) == 0;
    }

    static bool remove_dir(const char* dirname) noexcept
    {
        return ::rmdir(dirname) == 0;
    }

    static bool rename(const char* old, const char* new_name) noexcept
    {
        return ::rename(old, new_name) == 0;
    }
    
    static File_Stats get_stats(File const& file) noexcept
    {
        File_Stats stat = {0};
        ::fstat64(file.descriptor, &stat);
        return stat;
    }

    static File_Stats get_stats(const char* path) noexcept
    {
        File_Stats stat = {0};
        ::stat64(path, &stat);
        return stat;
    }

    struct File_IO_Result
    {
        isize processed_size = 0;
        errno_t errno_code = 0; // (has the value of errno at the time of erro)
        bool ok = false; //if an error was encountered (does NOT include EOF)
        bool eof = false; //if end of file was reached
        bool file_closed = false; //if file was not opened;
        bool continue_io_loop = false; //a control variable used for reading

        // while(continue_io_loop)
        //   //read...
        //
        // if is set to false further reads wont be possible
        // the following always holds: continue_io_loop = ok && !eof;
    };

    //Reads any buffer size to buffer and returns state
    //During reading one of these scenerios can happen:
    // 1 - read some buffer_size and end was not encountered => ok
    // 2 - read some buffer_size and end was encountered => ok & eof
    // 3 - read some buffer_size and error was encountered => !ok & errno_code
    // 4 - file was closed => !ok & file_closed
    static File_IO_Result partial_read(File* file, void* buffer, isize buffer_size) noexcept
    {
        assert(buffer_size >= 0 && "size must be valid");
        assert(buffer != NULL && "buffer must be valid");

        File_IO_Result result;
        result.ok = true;
        if(is_open(*file) == false)
        {
            result.file_closed = true;
            result.ok = false;
        }
        else if(buffer_size != 0)
        {
            //in case buffer_size > MAX_READ_WRITE_CHUNK we read multiple times
            while(result.processed_size < buffer_size)
            {
                isize remaining = buffer_size - result.processed_size;;
                unsigned single_read = (unsigned) min(remaining, MAX_READ_WRITE_CHUNK);
                void* read_to = (char*) buffer + result.processed_size;

                int res = ::read(file->descriptor, read_to, single_read);
                if(res == 0) 
                {
                    result.eof = true;
                    break;
                }

                if(res < 0)
                {
                    result.errno_code = errno;
                    result.ok = false;
                    break;
                }

                result.processed_size += res;
                if(res != MAX_READ_WRITE_CHUNK)
                    break;
            }
        }

        result.continue_io_loop = result.ok && !result.eof;
        return result;
    }
    
    //Reads buffer size to buffer and returns state. If reads smaller size
    // then requested without error, keeps trying until whole size is read, error
    // is encountered or eof is reached. During reading one of these scenerios can happen:
    // 1 - read all buffer_size and end was not encountered => ok
    // 2 - read all buffer_size and end was encountered => ok & eof
    // 3 - read some buffer_size and end was not encountered => !ok & errno_code
    // 4 - read some buffer_size and end was encountered => ok & eof
    // 5 - file was closed => !ok & file_closed
    static File_IO_Result read(File* file, void* buffer, isize buffer_size) noexcept
    {
        File_IO_Result result;
        isize processed_size = 0;
        while(true)
        {
            result = partial_read(file, buffer, buffer_size);
            if(result.continue_io_loop == false)
                break;

            processed_size += result.processed_size;
            if(processed_size >= buffer_size)
                break;
        }

        result.processed_size = processed_size;
        return result;
    }

    //Writes any buffer size to buffer and returns state
    //During writing one of these scenerios can happen:
    // 1 - read some buffer_size => ok
    // 2 - read some buffer_size and error was encountered => !ok & errno_code
    // 3 - file was closed => !ok & file_closed
    //Note: cannot produce EOF
    static File_IO_Result partial_write(File* file, const void *buffer, isize buffer_size) noexcept
    {
        assert(buffer_size >= 0 && "size must be valid");
        assert(buffer != NULL && "buffer must be valid");
        
        File_IO_Result result;
        result.ok = true;
        if(is_open(*file) == false)
        {
            result.file_closed = true;
            result.ok = false;
        }
        else if(buffer_size != 0)
        {
            while(result.processed_size < buffer_size)
            {
                isize remaining = buffer_size - result.processed_size;;
                unsigned single_read = (unsigned) min(remaining, MAX_READ_WRITE_CHUNK);
                void* read_to = (char*) buffer + result.processed_size;

                int res = ::write(file->descriptor, read_to, single_read);
                if(res < 0)
                {
                    result.ok = false;
                    result.errno_code = errno;
                    break;
                }

                assert(res != 0 && "shoudlnt be zero here");
                result.processed_size += res;
            }
        }

        result.continue_io_loop = result.ok && !result.eof;
        return result;
    }

    //Writes buffer size to buffer and returns state. If writes smaller size
    // then requested without error, keeps trying until whole size is written, 
    // or error is encountered. During writing one of these scenerios can happen:
    // 1 - written all buffer_size => ok
    // 2 - written some buffer_size and error was encountered => !ok & errno_code
    // 3 - file was closed => !ok & file_closed
    //Note: cannot produce EOF
    static File_IO_Result write(File* file, const void *buffer, isize buffer_size) noexcept
    {   
        File_IO_Result result;
        isize processed_size = 0;
        while(true)
        {
            result = partial_write(file, buffer, buffer_size);
            if(result.continue_io_loop == false)
                break;

            processed_size += result.processed_size;
            if(processed_size >= buffer_size)
                break;
        }

        result.processed_size = processed_size;
        return result;
    }

    //====== Cplusplus-y things =======
    File::File(File&& other) noexcept
    {
        File_Descriptor temp = other.descriptor;
        other.descriptor = descriptor;
        descriptor = temp;
    }

    File::~File() noexcept
    {
        if(descriptor < 0)
            return;

        close((File&&) *this);
    }
    
    namespace file_globals
    {
        inline isize* descriptors_size_ptr()
        {
            thread_local static isize size = 0;
            return &size;
        }

        inline File_Descriptor** descriptors_ptr()
        {
            thread_local static File_Descriptor* descriptors_ptr = nullptr;
            return &descriptors_ptr;
        }

        static isize find_descriptor(isize value)
        {
            isize descriptors_size = *file_globals::descriptors_size_ptr();
            File_Descriptor* descriptors = *file_globals::descriptors_ptr();
            if(descriptors_size <= 0 || descriptors == nullptr)
                return -2;

            for(isize i = 0; i < descriptors_size; i++)
                if(descriptors[i] == value)
                    return i;

            return -1;
        }

        //@TODO: make better
        struct File_Guard_Swap
        {
            File_Descriptor* new_descriptors = nullptr;
            File_Descriptor* old_descriptors = nullptr;
            isize new_descriptors_size = 0;
            isize old_descriptors_size = 0;

            File_Guard_Swap(File_Descriptor* new_descriptor_array, isize new_descriptor_array_size) 
            {
                new_descriptors = new_descriptor_array;
                new_descriptors_size = new_descriptor_array_size;
                old_descriptors = *descriptors_ptr();
                old_descriptors_size = *descriptors_size_ptr();

                for(isize i = 0; i < new_descriptors_size; i++)
                    new_descriptors[i] = -1;

                *descriptors_ptr() = new_descriptors;
            }

            ~File_Guard_Swap()
            {
                for(isize i = 0; i < new_descriptors_size; i++)
                    close(File(new_descriptors[i]));

                *descriptors_ptr() = old_descriptors;
                *descriptors_size_ptr() = old_descriptors_size;
            }
        };
    }
    
    #ifndef SET_UTF8_LOCALE
    #define SET_UTF8_LOCALE
    static bool set_utf8_locale(bool english = false)
    {
        if(english)
            return setlocale(LC_ALL, "en_US.UTF-8") != NULL;
        else
            return setlocale(LC_ALL, ".UTF-8") != NULL;
    }

    namespace {
        const static bool _locale_setter = set_utf8_locale(true);
    }
    #endif // !SET_UTF8_LOCALE
}

#undef min
#undef max
