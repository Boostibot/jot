#pragma once

#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "unistd.h"
#include "defer.h"
#include "resource.h"
#include "defines.h"
namespace jot 
{
    typedef int FileDescriptor;
    using FileD = FileDescriptor;

}
namespace jot 
{

    struct C_Alloc {};

    Res<FileD> open(const char *filename, int oflag, int pmode = 0)
    {
        return unsafe::make<FileD>(::open(filename, oflag, pmode));
    }

    bool close(Res<FileD> fd) 
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

    bool set_access(const char *path, int mode)
    {
        return ::access(path, mode) != -1;
    }

    Res<FileD> copy(FileD fd)
    {
        return unsafe::make<FileD>(::dup(fd));
    }
    bool copy(FileD fd, Res<FileD>* to)
    {
        return ::dup2(fd, *to) == 0;
    }

    bool truncate(Res<FileD>* fd, long size) 
    {
        return ::ftruncate(*fd, size) == 0;
    }

    bool remove(const char *filename) 
    {
        return ::unlink(filename) == 0;
    }

    Res<FileD> from_c_file(FILE *stream) 
    {
        return unsafe::make<FileD>(::fileno(stream));
    }

    FILE* to_c_file(Res<FileD> fd, const char *mode) 
    {
        unsafe::drop(fd);
        return ::fdopen(*fd, mode);
    }

    Res<char *, C_Alloc> get_current_dir(char *buffer, int maxlen)
    {
        return unsafe::make<char*, C_Alloc>(::getcwd(buffer, maxlen));
    }

    bool change_dir(const char *dirname)
    {
        return ::chdir(dirname) == 0;
    }

    bool is_open(FileD fd )
    {
        return ::isatty(fd) != 0;
    }

    off64_t seek(Res<FileD>* fd, off64_t offset, int origin)
    {
        return ::lseek64(*fd, offset, origin);
    } 

    off64_t tell(FileD fd)
    {
        return ::tell64(fd);
    }

    bool make_dir(const char *dirname) 
    {
        return ::mkdir(dirname) == 0;
    }

    bool renove_dir(const char *dirname) 
    {
        return ::rmdir(dirname) == 0;
    }

    int read(Res<FileD>* fd, void * buffer, unsigned buffer_size) 
    {
        return ::read(*fd, buffer, buffer_size);
    }

    int write(Res<FileD>* fd, const void *buffer, unsigned buffer_size)
    {
        return ::write(*fd, buffer, buffer_size);
    }

    void main() 
    {
        auto fd = open("hello", 10);
        defer(close(fd));

        int read_size = read(&fd, &fd, 10);
    }

}
