#pragma once

#define _LARGEFILE64_SOURCE
#define _LARGE_TIME_API
#define _POSIX_SOURCE
#define _CRT_NONSTDC_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS_GLOBALS
#pragma warning(disable : 4996)

#include <cstdio>
#include <cassert>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <clocale>
#include <cstddef>

#include "unistd.h"

#define cast(a) (a)
#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

namespace jot 
{
    using File_Descriptor = int;
    using File_Flag = int;
    using File_Mode = mode_t;
    using File_Stats = Stat64;
    
    using isize = ptrdiff_t;
    using cstring = const char*;

    //A thin wrapper around the posix file interface
    // handles closing the file automatically
    struct File 
    {
        File_Descriptor descriptor = -1;
        File() {}
        File(File_Descriptor from) : descriptor(from) {}
        File(File const&) = delete;

        ~File() {
            if(descriptor == -1)
                return;

            ::close(descriptor);
        }
    };

    //Shared flags
    namespace Flags {
        enum : File_Flag {
            READ = O_RDONLY,
            WRITE = O_WRONLY,
            READ_WRITE = O_RDWR,

            APPEND = O_APPEND,
            CREATE = O_CREAT,
            EXCLUSIVE = O_EXCL,
            TRUNCATE = O_TRUNC,
        };
    };

    namespace Windows_Flags {
        enum : File_Flag {
            TEXT       = O_TEXT,
            BINARY     = O_BINARY,
            RAW        = O_BINARY,
            TEMPORARY  = O_TEMPORARY,
            NOINHERIT  = O_NOINHERIT,
            SEQUENTIAL = O_SEQUENTIAL,
            RANDOM     = O_RANDOM,
        };
    };

    namespace Linux_Flags {
        enum : File_Flag {
            DSYNC = O_DSYNC,
            NOCTTY = O_NOCTTY,
            NONBLOCK = O_NONBLOCK,
            RSYNC = O_RSYNC,
            SYNC = O_SYNC,
        };
    };

    namespace All_Flags {
        using namespace Flags;           
        using namespace Windows_Flags;           
        using namespace Linux_Flags;           
    };

    namespace enum_detail {
        enum File_Open_Mode : File_Mode {
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
    }

    enum class Access_Permission : int {
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

    using File_Open_Mode = enum_detail::File_Open_Mode;

    static const File_Mode DEFAULT_OPEN_MODE = 0644;

    //this is a wild guess cause the standard only says more than 32767B but on probably all systems
    // the following should hold: 
    static const isize MAX_READ_WRITE_CHUNK = (1 << 30) - 1;
    // see: https://stackoverflow.com/a/29723318
        
    inline File open(cstring filename, File_Flag oflag = Flags::READ_WRITE | Windows_Flags::BINARY, File_Mode pmode = DEFAULT_OPEN_MODE) noexcept
    {
        return File(::open(filename, oflag | Windows_Flags::BINARY, pmode));
    }

    inline File create(cstring filename, File_Flag oflag = Flags::READ_WRITE | Windows_Flags::BINARY | Flags::CREATE, File_Mode pmode = DEFAULT_OPEN_MODE) noexcept
    {
        return open(filename, oflag, pmode);
    }

    inline bool close(File fd) noexcept
    {
        if(fd.descriptor == -1)
            return true;

        bool state = ::close(fd.descriptor) == 0;
        fd.descriptor = -1;
        return state;
    }

    inline bool has_access(cstring path, Access_Permission permission) noexcept
    {
        return ::access(path, cast(int) permission) != -1;
    }

    inline File copy(File const& fd) noexcept
    {
        return File(::dup(fd.descriptor));
    }

    inline bool copy(File const& fd, File* to) noexcept
    {
        return ::dup2(fd.descriptor, to->descriptor) == 0;
    }

    //Truncates file specified by fd to specified size in bytes
    inline bool truncate(File* fd, long size) noexcept
    {
        return ::ftruncate(fd->descriptor, cast(long) size) == 0;
    }

    //deletes a name and possibly the file it refers to
    // the file remains in existance untill all other processes
    // closed the file only then it is deleted.
    inline bool unlink(cstring filename) noexcept
    {
        return ::unlink(filename) == 0;
    }

    inline File from_c_file(FILE* stream) noexcept
    {
        return File(::fileno(stream));
    }

    inline FILE* to_c_file(File fd, cstring mode) noexcept
    {
        return ::fdopen(fd.descriptor, mode);
    }

    //Fills buffer with the current dir cstring. If it is too small mallocs it instead
    // returns a pointer to the potentionally alloced destination. This needs to be freed
    // Might also fail and return NULL instead.
    inline cstring fill_or_alloc_current_dir_cstring(char *buffer, int maxlen) noexcept
    {
        return ::getcwd(buffer, maxlen);
    }

    inline bool change_dir(cstring dirname) noexcept
    {
        return ::chdir(dirname) == 0;
    }

    inline bool is_open(File const& fd) noexcept
    {
        return fd.descriptor != -1;
    }

    inline bool is_character_device(File const& fd) noexcept
    {
        return is_open(fd) && ::isatty(fd.descriptor) != 0;
    }

    inline isize seek(File* fd, isize offset, Seek_From from = Seek_From::Begin) noexcept
    {
        if(from == Seek_From::Begin)
            assert(offset >= 0 && "cannot seek before start");
        
        if(from == Seek_From::End)
            assert(offset <= 0 && "cannot seek past end");
        
        off64_t cast_offset = cast(off64_t) offset;
        return cast(isize) ::lseek64(fd->descriptor, cast_offset, cast(int) from);
    } 

    inline isize tell(File const& fd) noexcept
    {
        return cast(isize) ::tell64(fd.descriptor);
    }

    inline bool make_dir(cstring dirname) noexcept
    {
        return ::mkdir(dirname) == 0;
    }

    inline bool remove_dir(cstring dirname) noexcept
    {
        return ::rmdir(dirname) == 0;
    }

    inline bool rename(cstring old, cstring new_name) noexcept
    {
        return ::rename(old, new_name) == 0;
    }

    struct File_IO_Result
    {
        isize processed_size = 0;

        //if an error was encountered (does NOT include EOF)
        bool ok = false;

        //if end of file was reached
        bool eof = false;

        //if file was not opened;
        bool file_closed = false;
        
        //a control variable used for reading:
        bool continue_io_loop = false;
        // while(continue_io_loop)
        //   //read...
        //
        // if is set to false further reads wont be possible
        // the following holds: continue_io_loop = ok && !eof;

        // (has the value of errno at the time of erro)
        errno_t errno_code = 0;
    };

    //Reads any buffer size to buffer and returns state
    //During reading one of these scenerios can happen:
    // 1 - read some buffer_size and end was not encountered => ok
    // 2 - read some buffer_size and end was encountered => ok & eof
    // 3 - read some buffer_size and error was encountered => !ok & errno_code
    // 4 - file was closed => !ok & file_closed
    inline File_IO_Result partial_read(File* fd, void* buffer, isize buffer_size) noexcept
    {
        assert(buffer_size >= 0 && "size must be valid");
        assert(buffer != NULL && "buffer must be valid");

        File_IO_Result result;
        result.ok = true;
        if(is_open(*fd) == false)
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
                unsigned single_read = cast(unsigned) min(remaining, MAX_READ_WRITE_CHUNK);
                void* read_to = cast(char*) buffer + result.processed_size;

                int res = ::read(fd->descriptor, read_to, single_read);
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
    inline File_IO_Result read(File* fd, void* buffer, isize buffer_size) noexcept
    {
        File_IO_Result result;
        isize processed_size = 0;
        while(true)
        {
            result = partial_read(fd, buffer, buffer_size);
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
    inline File_IO_Result partial_write(File* fd, const void *buffer, isize buffer_size) noexcept
    {
        assert(buffer_size >= 0 && "size must be valid");
        assert(buffer != NULL && "buffer must be valid");
        
        File_IO_Result result;
        result.ok = true;
        if(is_open(*fd) == false)
        {
            result.file_closed = true;
            result.ok = false;
        }
        else if(buffer_size != 0)
        {
            while(result.processed_size < buffer_size)
            {
                isize remaining = buffer_size - result.processed_size;;
                unsigned single_read = cast(unsigned) min(remaining, MAX_READ_WRITE_CHUNK);
                void* read_to = cast(char*) buffer + result.processed_size;

                int res = ::write(fd->descriptor, read_to, single_read);
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
    inline File_IO_Result write(File* fd, const void *buffer, isize buffer_size) noexcept
    {   
        File_IO_Result result;
        isize processed_size = 0;
        while(true)
        {
            result = partial_write(fd, buffer, buffer_size);
            if(result.continue_io_loop == false)
                break;

            processed_size += result.processed_size;
            if(processed_size >= buffer_size)
                break;
        }

        result.processed_size = processed_size;
        return result;
    }

    inline File_Stats get_stats(File const& fd) noexcept
    {
        File_Stats stat = {0};
        ::fstat64(fd.descriptor, &stat);
        return stat;
    }

    inline File_Stats get_stats(cstring path) noexcept
    {
        File_Stats stat = {0};
        ::stat64(path, &stat);
        return stat;
    }
    
    #ifndef SET_UTF8_LOCALE
    #define SET_UTF8_LOCALE
    inline bool set_utf8_locale(bool english = false)
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

#undef cast
#undef min
#undef max
