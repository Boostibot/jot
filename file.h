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
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "unistd.h"

namespace jot 
{
    namespace file 
    {
        typedef int             Descriptor;
        typedef int             Flag;
        typedef mode_t          Mode;
        typedef Stat64          Stats;

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

        typedef enum_detail::Open_Mode Open_Mode;

        static const Mode DEFAULT_OPEN_MODE = 0644;
        static const int MAX_READ_WRITE_CHUNK = 1 << 30;

        struct File_Descriptor 
        {
            Descriptor descriptor = -1;
            File_Descriptor() {}
            File_Descriptor(Descriptor from) : descriptor(from) {}
            File_Descriptor(File_Descriptor const&) = delete;

            ~File_Descriptor() {
                if(descriptor == -1)
                    return;

                ::close(descriptor);
            }
        };

        File_Descriptor open(const char* filename, Flag oflag = Flags::READ_WRITE | Windows_Flags::BINARY, Mode pmode = DEFAULT_OPEN_MODE) noexcept
        {
            return File_Descriptor(::open(filename, oflag | Windows_Flags::BINARY, pmode));
        }

        File_Descriptor create(const char* filename, Flag oflag = Flags::READ_WRITE | Windows_Flags::BINARY | Flags::CREATE, Mode pmode = DEFAULT_OPEN_MODE) noexcept
        {
            return open(filename, oflag, pmode);
        }

        bool close(File_Descriptor fd) noexcept
        {
            if(fd.descriptor == -1)
                return true;

            bool state = ::close(fd.descriptor) == 0;
            fd.descriptor = -1;
            return state;
        }

        bool has_access(const char* path, Access_Permission permission) noexcept
        {
            return ::access(path, (int) permission) != -1;
        }

        File_Descriptor copy(File_Descriptor const& fd) noexcept
        {
            return File_Descriptor(::dup(fd.descriptor));
        }

        bool copy(File_Descriptor const& fd, File_Descriptor* to) noexcept
        {
            return ::dup2(fd.descriptor, to->descriptor) == 0;
        }

        bool truncate(File_Descriptor* fd, long size) noexcept
        {
            return ::ftruncate(fd->descriptor, (long) size) == 0;
        }

        bool unlink(const char* filename) noexcept
        {
            return ::unlink(filename) == 0;
        }

        File_Descriptor from_c_file(FILE* stream) noexcept
        {
            return File_Descriptor(::fileno(stream));
        }

        FILE* to_c_file(File_Descriptor fd, const char* mode) noexcept
        {
            return ::fdopen(fd.descriptor, mode);
        }

        //Fills buffer with the current dir const char*. If it is too small mallocs it instead
        // returns a pointer to the potentionally alloced destination. This needs to be freed
        // Might also fail and return NULL instead.
        const char* fill_or_alloc_current_dir_cstring(char *buffer, int maxlen) noexcept
        {
            return ::getcwd(buffer, maxlen);
        }

        bool change_dir(const char* dirname) noexcept
        {
            return ::chdir(dirname) == 0;
        }

        bool is_open(File_Descriptor const& fd) noexcept
        {
            return fd.descriptor != -1;
        }

        bool is_character_device(File_Descriptor const& fd) noexcept
        {
            return is_open(fd) && ::isatty(fd.descriptor) != 0;
        }

        off64_t seek(File_Descriptor* fd, off64_t offset, Seek_From from = Seek_From::Begin) noexcept
        {
            return ::lseek64(fd->descriptor, offset, (int) from);
        } 

        off64_t tell(File_Descriptor const& fd) noexcept
        {
            return ::tell64(fd.descriptor);
        }

        bool make_dir(const char* dirname) noexcept
        {
            return ::mkdir(dirname) == 0;
        }

        bool remove_dir(const char* dirname) noexcept
        {
            return ::rmdir(dirname) == 0;
        }

        bool rename(const char* old, const char* new_name) noexcept
        {
            return ::rename(old, new_name) == 0;
        }

        //Returns the number of bytes read or
        // EOF (-1) when end of file is reached
        // -2... Signaling other errros (offset by one from the posix)
        int read(File_Descriptor* fd, void * buffer, unsigned buffer_size) noexcept
        {
            assert(buffer_size <= MAX_READ_WRITE_CHUNK && "past maximum buffer size");
            if(buffer_size == 0)
                return 0;

            int res = ::read(fd->descriptor, buffer, (unsigned) buffer_size);
            if(res == 0) 
                return EOF;
            if(res < 0)
                return res - 1;
            return res;
        }

        //Returns the number of bytes written
        int write(File_Descriptor* fd, const void *buffer, unsigned buffer_size) noexcept
        {
            assert(buffer_size <= MAX_READ_WRITE_CHUNK && "past maximum buffer size");
            return ::write(fd->descriptor, buffer, (unsigned) buffer_size);
        }

        Stats get_stats(File_Descriptor const& fd) noexcept
        {
            Stats stat;
            ::fstat64(fd.descriptor, &stat);
            return stat;
        }

        Stats get_stats(const char* path) noexcept
        {
            Stats stat;
            ::stat64(path, &stat);
            return stat;
        }
    }
}
