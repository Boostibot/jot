#pragma once

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
    //#define   X_OK    1       /* execute permission - unsupported in windows*/
    #define F_OK    0       /* Test for existence.  */

    using off64_t = __int64;
    using off_t = long;

    int ftruncate(int fd,long size) 
        { return _chsize(fd, size); }

    off64_t lseek64(int fd, off64_t offset, int whence)
        { return _lseeki64(fd, offset, whence); }

    off64_t tell64(int fd) 
        { return _telli64(fd); }

#else
    #include <unistd.h>
#endif // _WIN32) || defined(_WIN64) || defined(__CYGWIN__)