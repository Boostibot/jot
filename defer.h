#pragma once

template <typename Fn>
struct Defered 
{
    Fn fn;

    Defered(Fn fn_) : fn(fn_) {}
    ~Defered() {fn();}
};

#ifndef  __COUNTER__ 
#define __COUNTER__ 0
#endif  

#define PP_CONCAT2(a, b, c, d) a ## b ## c ## d
#define PP_CONCAT(a, b, c, d) PP_CONCAT2(a, b, c, d)
#define defer(...) Defered PP_CONCAT(defer_,__LINE__, _, __COUNTER__) ([&]{__VA_ARGS__;})
