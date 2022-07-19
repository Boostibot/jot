#pragma once

#define _LARGEFILE64_SOURCE
#define _LARGE_TIME_API
#define _POSIX_SOURCE
#define _CRT_NONSTDC_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS_GLOBALS
#pragma warning(disable : 4996)

#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "unistd.h"
#include "resource.h"
#include "types.h"
#include "defines.h"

namespace jot 
{
    struct C_Alloc {};

    namespace File 
    {
        typedef int Descriptor;
        using Fd = Descriptor;
        using Flag = int;
        using Mode = mode_t;
        using Stats = Stat64;

        //Shared flags
        namespace Flags {

            enum : Flag {
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
            enum : Flag {
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
            enum : Flag {
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
            enum Open_Mode : Mode {
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

        using Open_Mode = enum_detail::Open_Mode;

        static constexpr Mode DEFAULT_OPEN_MODE = 0644;
        static constexpr u64 MAX_READ_WRITE_CHUNK = 1 << 30;


        Res<Fd> open(cstring filename, Flag oflag = Flags::READ_WRITE | Windows_Flags::BINARY, Mode pmode = DEFAULT_OPEN_MODE) noexcept
        {
            return unsafe::make<Fd>(::open(filename, oflag | Windows_Flags::BINARY, pmode));
        }

        Res<Fd> create(cstring filename, Flag oflag = Flags::READ_WRITE | Windows_Flags::BINARY | Flags::CREATE, Mode pmode = DEFAULT_OPEN_MODE) noexcept
        {
            return open(filename, oflag, pmode);
        }

        bool close(Res<Fd> fd) noexcept
        {
            if(fd == -1)
            {
                unsafe::drop(fd);
                return true;
            }

            bool res = ::close(fd) == 0;
            unsafe::drop(fd);
            return res;
        }

        bool has_access(cstring path, Access_Permission permission) noexcept
        {
            return ::access(path, cast(int) permission) != -1;
        }

        Res<Fd> copy(Fd fd) noexcept
        {
            return unsafe::make<Fd>(::dup(fd));
        }
        bool copy(Fd fd, Res<Fd>* to) noexcept
        {
            return ::dup2(fd, *to) == 0;
        }

        bool truncate(Res<Fd>* fd, u64 size) noexcept
        {
            return ::ftruncate(*fd, cast(long) size) == 0;
        }

        bool unlink(cstring filename) noexcept
        {
            return ::unlink(filename) == 0;
        }

        Res<Fd> from_c_file(FILE* stream) noexcept
        {
            return unsafe::make<Fd>(::fileno(stream));
        }

        FILE* to_c_file(Res<Fd> fd, cstring mode) noexcept
        {
            unsafe::drop(fd);
            return ::fdopen(*fd, mode);
        }

        Res<char *, C_Alloc> get_current_dir(char *buffer, int maxlen) noexcept
        {
            return unsafe::make<char*, C_Alloc>(::getcwd(buffer, maxlen));
        }

        bool change_dir(cstring dirname) noexcept
        {
            return ::chdir(dirname) == 0;
        }

        bool is_open(Fd fd) noexcept
        {
            return fd != -1;
        }

        bool is_character_device(Fd fd) noexcept
        {
            return is_open(fd) && ::isatty(fd) != 0;
        }


        u64 seek(Res<Fd>* fd, u64 offset, Seek_From from = Seek_From::Begin) noexcept
        {
            return cast(u64) ::lseek64(*fd, cast(off64_t) offset, cast(int) from);
        } 

        u64 tell(Fd fd) noexcept
        {
            return cast(u64) ::tell64(fd);
        }

        bool make_dir(cstring dirname) noexcept
        {
            return ::mkdir(dirname) == 0;
        }

        bool remove_dir(cstring dirname) noexcept
        {
            return ::rmdir(dirname) == 0;
        }

        bool rename(cstring old, cstring new_name) noexcept
        {
            return ::rename(old, new_name) == 0;
        }

        //Returns the number of bytes read or
        // EOF (-1) when end of file is reached
        // -2... Signaling other errros
        u64 read(Res<Fd>* fd, void * buffer, u64 buffer_size) noexcept
        {
            if(buffer_size == 0)
                return 0;

            int res = ::read(*fd, buffer, cast(unsigned) buffer_size);
            if(res == 0) 
                return cast(u64) EOF;
            if(res == -1)
                return cast(u64) -2;
            return res;
        }

        //Returns the number of bytes written
        u64 write(Res<Fd>* fd, const void *buffer, u64 buffer_size) noexcept
        {
            return cast(u64) ::write(*fd, buffer, cast(unsigned) buffer_size);
        }

        Stats get_stats(Fd fd) noexcept
        {
            Stats stat;
            ::fstat64(fd, &stat);
            return stat;
        }

        Stats get_stats(cstring path) noexcept
        {
            Stats stat;
            ::stat64(path, &stat);
            return stat;
        }

    }
}
