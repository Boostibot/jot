#pragma once
#pragma warning(disable : 4996)
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    #define _CRT_NONSTDC_NO_WARNINGS

    #include <stdlib.h>
    #include <io.h>
    #include <process.h> /* for getpid() and the exec..() family */
    #include <direct.h> /* for _getcwd() and _chdir() */

    /* Values for the second argument to access.
    These may be OR'd together.  */
    #define R_OK    4       /* Test for read permission.  */
    #define W_OK    2       /* Test for write permission.  */
    #define X_OK    1       /* execute permission - unsupported in windows*/
    #define F_OK    0       /* Test for existence.  */

    using off64_t = __int64;
    using off_t = long;
    using mode_t = int;
    //using stat = struct _stat;

    typedef struct _stat64 Stat64;

    int ftruncate(int fd,long size) 
    { 
        return _chsize(fd, size); 
    }

    off64_t lseek64(int fd, off64_t offset, int whence)
    { 
        return _lseeki64(fd, offset, whence); 
    }

    off64_t tell64(int fd) 
    { 
        return _telli64(fd); 
    }

    int stat64(const char* name, Stat64* stat) 
    { 
        return _stat64(name, stat); 
    }

    int fstat64(int fd, Stat64* stat) 
    { 
        return _fstat64(fd, stat); 
    }

    //Linux file flags
    #define O_DSYNC 0
    #define O_NOCTTY 0
    #define O_NONBLOCK 0
    #define O_RSYNC 0
    #define O_SYNC 0

#else
    typedef struct stat64 Stat64;

    //Windows file flags
    #define O_TEXT 0
    #define O_BINARY 0
    #define O_BINARY 0
    #define O_TEMPORARY 0
    #define O_NOINHERIT 0
    #define O_SEQUENTIAL 0
    #define O_RANDOM 0

    #include <unistd.h>
#endif // _WIN32) || defined(_WIN64) || defined(__CYGWIN__)