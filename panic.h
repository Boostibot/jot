#pragma once

#ifndef _MSC_VER
#define __FUNCTION__ __func__
#endif

namespace jot
{

    struct Panic
    {
        Line_Info line_info;
        const char* _cstring_message;
        
        Panic(Line_Info line_info, const char* message = 
            "<This is an uninit panic message member! "
            "This is probably because you are catching Panic by value and displaying its what() message. "
            "Catch Panic const& instead and try again>") 
            : line_info(line_info), _cstring_message(message) {}

        virtual const char* what() const noexcept 
        {
            return _cstring_message;
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
        
    //This is necessary for empty panic to compile on MSVC
    #define MAKE_PANIC(...) ::jot::make_panic(GET_LINE_INFO(), ##__VA_ARGS__)

    //Constructs a panic via make_panic from the given arguments and then throws it
    #define PANIC_WITH(condition, ...)                                     \
        while(condition) {                                                 \
            auto const& _local_panicable_ = MAKE_PANIC(__VA_ARGS__); \
            ::jot::panic_handler()->handle(_local_panicable_);             \
            throw _local_panicable_;                                       \
        }                                                                  \
    
    //invokes a panic handler then constructs a Panic out out of the given arguments and current Line_Info
    #define PANIC(...)       PANIC_WITH(true, __VA_ARGS__)
    
    //An assert that gets called even in release. Panics if the check fails.
    #define force(condition) PANIC_WITH(!(condition), "Check failed: force(" #condition ")") 
}
