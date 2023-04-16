#pragma once
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>

namespace jot
{
    #ifndef GET_LINE_INFO
    #ifndef _MSC_VER
        #define __FUNCTION__ __func__
    #endif

    struct Line_Info
    {
        const char* file = "";
        const char* func = "";
        ptrdiff_t line = -1;
    };
    
    #define GET_LINE_INFO() ::jot::Line_Info{__FILE__, __FUNCTION__, __LINE__}
    #endif

    struct Panic
    {
        Line_Info line_info;
        const char* _cstring_message;
        bool _is_malloced;
        
        Panic(Line_Info line_info, const char* message, bool malloced = false);
        Panic(Panic const&) = delete;
        Panic(Panic&& other) noexcept;

        virtual const char* what() const noexcept;
        virtual ~Panic() noexcept;
    };

    //Gets called before throw is executed. Can be used to for example to capture and print stack trace
    struct Panic_Handler
    {
        virtual void handle(Panic const&) noexcept {}
        virtual ~Panic_Handler() noexcept {}
    };

    namespace panic_globals
    {
        inline Panic_Handler* idle_handler_ptr()
        {
            static Panic_Handler idle;
            return &idle;
        }
        
        inline Panic_Handler** panic_handler_ptr()
        {
            thread_local static Panic_Handler* handler = idle_handler_ptr();
            return &handler;
        }
        
        inline Panic_Handler* panic_handler()
        {
            return *panic_handler_ptr();
        }
    }

    //Formats arguments according to cformat
    // returns the result in malloc'ed string that needs to be free'd
    static char* malloc_vcformat(const char* cformat, va_list args);
    static char* malloc_cformat(const char* cformat, ...);
    static Panic panic_cformat(Line_Info line_info, const char* cformat, ...);

    //Constructs a panic via make_panic from the given arguments and then throws it
    #define PANIC_WITH(condition, panicable)                               \
        while(condition) {                                                 \
            auto _local_panicable_ = panicable;                            \
            ::jot::panic_globals::panic_handler()->handle(_local_panicable_);          \
            throw _local_panicable_;                                       \
        }                                                                  \
    
    //invokes a panic handler then constructs a Panic out out of the given arguments and current Line_Info
    #define PANIC(panic, ...)       PANIC_WITH(true, panic(GET_LINE_INFO(), ##__VA_ARGS__))
    
    //An assert that gets called even in release. Panics if the check fails.
    #define force(condition) PANIC_WITH(!(condition), Panic(GET_LINE_INFO(), "Check failed: force(" #condition ")")) 
}

namespace jot
{
    Panic::Panic(Line_Info line_info, const char* message, bool alloced) 
        : line_info(line_info), 
        _cstring_message(message), 
        _is_malloced(alloced) {}

    Panic::Panic(Panic&& other) noexcept
        : line_info(other.line_info), 
        _cstring_message(other._cstring_message), 
        _is_malloced(other._is_malloced)
    {
        other._is_malloced = false;
    }

    const char* Panic::what() const noexcept 
    {
        return _cstring_message;
    }

    Panic::~Panic() noexcept 
    {
        if(_is_malloced)
            free((void*) _cstring_message);
    }

    static char* malloc_vcformat(const char* cformat, va_list args)
    {
        //we try to save one call of vsnprintf so we uses some local buffer
        const size_t BUFF_SIZE = 8;
        char local_buffer[BUFF_SIZE] = {0};

        va_list args_copy;
        va_copy(args_copy, args);
        int count = vsnprintf(local_buffer, BUFF_SIZE, cformat, args);
        if(count < 0)
            count = 1;

        char* return_buffer = (char*) malloc((size_t) count+1);
        if(return_buffer == nullptr)
            return return_buffer;

        if(count < BUFF_SIZE)
            memcpy(return_buffer, local_buffer, (size_t) count);
        else
            vsnprintf(return_buffer, (size_t) count, cformat, args_copy);

        return_buffer[count] = '\0';
        return return_buffer;
    }
    
    static char* malloc_cformat(const char* cformat, ...)
    {
        va_list args;
        va_start(args, cformat);
        char* out = malloc_vcformat(cformat, args);
        va_end(args);

        return out;
    }

    static Panic panic_cformat(Line_Info line_info, const char* cformat, ...)
    {
        va_list args;
        va_start(args, cformat);
        char* out = malloc_vcformat(cformat, args);
        va_end(args);

        return Panic(line_info, out, true);
    }
}
