#pragma once

///Executes a function at the end of its lifetime.
template <typename Fn>
struct Defered 
{
    Fn fn;

    Defered& operator=(Defered const&) = delete;
    Defered(Fn fn_) : fn(fn_) {}
    ~Defered() {fn();}
};

#ifndef  __COUNTER__ 
#define __COUNTER__ 0
#endif  

#define PP_CONCAT4_(a, b, c, d) a ## b ## c ## d
#define PP_CONCAT4(a, b, c, d) PP_CONCAT4_(a, b, c, d)

#define defer(...) Defered PP_CONCAT4(defer_,__LINE__, _, __COUNTER__) ([&]{__VA_ARGS__;})

// This can be used in the following way:
// 
//      FILE* file = fopen("...");
//      defer(fclose(file));
//      // ....
// 
// We can now use the file liberaly without having to
// worry about closing it. This is also perfectly exception safe.
