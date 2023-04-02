#pragma once
#include <cstddef>

#ifndef _MSC_VER
#define __FUNCTION__ __func__
#endif

namespace jot
{
    struct Line_Info
    {
        const char* file = "";
        const char* func = "";
        ptrdiff_t line = -1;
    };

    struct Panic
    {
        Line_Info line_info;
        const char* cstring_message = "";
        
        Panic(Line_Info line_info, const char* message) 
            : line_info(line_info), cstring_message(message) {}

        virtual const char* what() const noexcept 
        {
            return cstring_message;
        }
        virtual ~Panic() noexcept {}
    };

    //Gets called before throw is executed. Can be used to for example to capture and print stack trace
    struct Panic_Handler
    {
        virtual void handle(Panic const&) noexcept {}
        virtual ~Panic_Handler() noexcept {}
    };

    namespace panic_globals
    {
        inline static Panic_Handler IDLE_HANDLER;
        thread_local inline static Panic_Handler* PANIC_HANDLER = &IDLE_HANDLER;
    }
    
    //retrives the currently installed panic handler for this thread
    inline Panic_Handler* panic_handler() noexcept
    {
        return panic_globals::PANIC_HANDLER;
    }
    
    //Makes a panicable out of the given arguments. 
    //Gets called internally by the panic macro
    //You can safely override this for any type deriving from Panic
    //First argument needs to always be Line_Info. 
    inline Panic make_panic(Line_Info line_info, const char* string) noexcept
    {
        return Panic(line_info, string);
    }
    
    inline Panic make_panic(Line_Info line_info) noexcept
    {
        return Panic(line_info, "<empty panic>");
    }

    //panics with panicable of any type. Can be used to make more sophisticated macros
    // if it is desired
    #define PANIC_WITH(panicable_expr)                          \
        do {                                                    \
            auto const& _local_panicable_ = panicable_expr;     \
            ::jot::panic_handler()->handle(_local_panicable_);  \
            throw _local_panicable_;                            \
        } while(false)                                          \
        
    //This is necessary for force to compile on MSVC (I dont know why exactly)
    #define MAKE_PANIC_MACRO(...) ::jot::make_panic(GET_LINE_INFO(), ##__VA_ARGS__)

    #define GET_LINE_INFO() ::jot::Line_Info{__FILE__, __FUNCTION__, (ptrdiff_t) (__LINE__)}

    //Constructs a panic via make_panic from the given arguments and then throws it
    #define panic(...) PANIC_WITH(MAKE_PANIC_MACRO(__VA_ARGS__))

    //An assert that gets called even in release.
    //If condition evaluates to false panics with the rest of the arguments.
    //If doesnt have any additional arguments prints the stringified condition similar to assert
    //This means that both force(false && "Hello") and force(false, "Hello") can be used with similar result
    #define force(condition, ...)                                   \
        do                                                          \
        {                                                           \
            if(!(condition))                                        \
            {                                                       \
                /* if empty args => empty string => len == 1 */     \
                if constexpr (sizeof(""#__VA_ARGS__) == 1)          \
                    panic("Check failed: force(" #condition ")");   \
                else                                                \
                    panic(__VA_ARGS__);                             \
            }                                                       \
        }                                                           \
        while (false)                                               \

}
