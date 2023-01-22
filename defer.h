#pragma once

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
