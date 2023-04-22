#ifndef INTRIN_INCLUDED
#define INTRIN_INCLUDED
//This is a wrapper around compiler/platform specific code. It compiles in both C and C++ under almost all compilers
// (the ones where it didnt compile were pretty much broken)
//The functions implemented here are largly arbitrary and by no means exhaustive just the ones I needed at some point.
//This file is aboslutely self sufficient which is useful when checking its functionality with different compilers

//Options:
#if 0
#define INTRIN_DISSABLE_ALL
#define INTRIN_NO_FIND_FIRST_SET
#define INTRIN_NO_POPCOUNT
#define INTRIN_NO_UNREACHABLE
#define INTRIN_NO_TRAP
#endif

//Finds used compiler
// if was programmer already defined dont define anything else
// this gives the programmer the power to overrule automatic checks
#if defined(INTRIN_COMPILER__CLANG) || defined(INTRIN_COMPILER__GNUC) || defined(INTRIN_COMPILER__MSVC) || defined(INTRIN_COMPILER__OTHER)
    //[nothing]
#elif defined(__clang__)
    #define INTRIN_COMPILER__CLANG
#elif defined(__GNUC__)
    #define INTRIN_COMPILER__GNUC
#elif defined(_MSC_VER)
    #define INTRIN_COMPILER__MSVC
#else
    #define INTRIN_COMPILER__OTHER
#endif

#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif

//if unknown compiler dissable intrins
#if defined(INTRIN_COMPILER__OTHER)
    #define INTRIN_DISSABLE_ALL
#endif

//if msvc include the appropriate header and set
// msvc specific pragmas
#if defined(INTRIN_COMPILER__MSVC)
    #include <intrin.h>

    #if defined(_M_IX86)
        #define INTRIN_COMPILER__MSVC_X86
    #elif defined(_M_X64)
        #define INTRIN_COMPILER__MSVC_X64
    #elif defined(_M_ARM)
        #define INTRIN_COMPILER__MSVC_ARM32
    #elif defined(_M_ARM64EC) || defined(_M_ARM64)
        #define INTRIN_COMPILER__MSVC_ARM64
    #endif

    #if defined(INTRIN_COMPILER__MSVC_X86) && defined(__cplusplus)
        //I am not entirely sure why but just with x86 and c++ these instructions
        // are simply missing and the program doesnt even compile
        #define INTRIN_NO_POPCOUNT
        #define INTRIN_NO_FIND_FIRST_SET
    #else
        #pragma intrinsic(_BitScanForward)
        #pragma intrinsic(_BitScanForward64)
        #pragma intrinsic(__popcnt)
        #pragma intrinsic(__popcnt64)
    #endif
#endif

#ifdef INTRIN_DISSABLE_ALL
#define INTRIN_NO_FIND_FIRST_SET
#define INTRIN_NO_POPCOUNT
#define INTRIN_NO_UNREACHABLE
#define INTRIN_NO_TRAP
#endif 

static bool _fallback_intrin__find_first_set_64(size_t* out, uint64_t search_in)
{
    if(search_in == 0)
    {
        *out = (size_t) -1;
        return false;
    }

    size_t k = 0;
    
    if ((search_in & 0xFFFFFFFFu) == 0) { search_in >>= 32; k |= 32; }
    if ((search_in & 0x0000FFFFu) == 0) { search_in >>= 16; k |= 16; }
    if ((search_in & 0x000000FFu) == 0) { search_in >>= 8;  k |= 8;  }
    if ((search_in & 0x0000000Fu) == 0) { search_in >>= 4;  k |= 4;  }
    if ((search_in & 0x00000003u) == 0) { search_in >>= 2;  k |= 2;  }

    k |= (~search_in & 1);
    *out = k;
    return true;
}

static bool intrin__find_first_set_32(size_t* out, uint32_t search_in)
{
    #if defined(INTRIN_COMPILER__MSVC) \
        && !defined(INTRIN_NO_FIND_FIRST_SET) 

        unsigned long index = 0;
        bool ret = _BitScanForward(&index, (unsigned) search_in) != 0;
        *out = (size_t) index;
        return ret;
    #elif (defined(INTRIN_COMPILER__GNUC) || defined(INTRIN_COMPILER__CLANG)) \
        && !defined(INTRIN_NO_FIND_FIRST_SET) 

        int index = __builtin_ffsl((unsigned long) search_in);
        *out = (size_t) index - 1;
        return index != 0;
    #else
        //i am not going to write this two times because of single or 
        // which the compiler will hopefully optimize away
        return _fallback_intrin__find_first_set_64(out, (uint64_t) search_in);
    #endif
}

static bool intrin__find_first_set_64(size_t* out, uint64_t search_in)
{
    #if defined(INTRIN_COMPILER__MSVC) \
        && !defined(INTRIN_NO_FIND_FIRST_SET) 

        unsigned long index = 0;
        bool ret = _BitScanForward64(&index, (unsigned long long) search_in) != 0;
        *out = (size_t) index;
        return ret;
    #elif (defined(INTRIN_COMPILER__GNUC) || defined(INTRIN_COMPILER__CLANG)) \
        && !defined(INTRIN_NO_FIND_FIRST_SET) 

        int index = __builtin_ffsll((unsigned long long) search_in);
        *out = (size_t) index - 1;
        return index != 0;
    #else
        //i am not going to write this two times because of single or 
        // which the compiler will hopefully optimize away
        return _fallback_intrin__find_first_set_64(out, (uint64_t) search_in);
    #endif
}
//ctz <=> forward
//clz <=> backward

static size_t intrin__pop_count_32(uint32_t val)
{
    #if defined(INTRIN_COMPILER__MSVC) \
        && !defined(INTRIN_NO_POPCOUNT) 

        return (size_t) __popcnt((unsigned int) val);
    #elif (defined(INTRIN_COMPILER__GNUC) || defined(INTRIN_COMPILER__CLANG)) \
        && !defined(INTRIN_NO_POPCOUNT) 

        return (size_t) __builtin_popcountl((unsigned long) val);
    #else
        uint32_t i = (uint32_t) val;
        i = i - ((i >> 1) & 0x55555555);                    // add pairs of bits
        i = (i & 0x33333333) + ((i >> 2) & 0x33333333);     // quads
        i = (i + (i >> 4)) & 0x0F0F0F0F;                    // groups of 8
        return (size_t) (i * 0x01010101) >> 24;         // horizontal sum of bytes
    #endif
}

static size_t intrin__pop_count_64(uint64_t val)
{
    #if defined(INTRIN_COMPILER__MSVC) \
        && !defined(INTRIN_NO_POPCOUNT) 

        return (size_t) __popcnt64((unsigned long long) val);
    #elif (defined(INTRIN_COMPILER__GNUC) || defined(INTRIN_COMPILER__CLANG)) \
        && !defined(INTRIN_NO_POPCOUNT) 

        return (size_t) __builtin_popcountll((unsigned long long) val);
    #else
        uint32_t val1 = (uint32_t) val;
        uint32_t val2 = (uint32_t) (val >> 32);
        size_t count1 = intrin__pop_count_32(val1);
        size_t count2 = intrin__pop_count_32(val2);

        return count1 + count2;
    #endif
}

//casuses the program to fail signaling to a debuger it should stop here
static void intrin__trap(void)
{
    #if defined(INTRIN_COMPILER__MSVC) \
        && !defined(INTRIN_NO_TRAP) 

        #define intrin__trap() __debugbreak()
    #elif (defined(INTRIN_COMPILER__GNUC) || defined(INTRIN_COMPILER__CLANG)) \
        && !defined(INTRIN_NO_TRAP) 

        #define intrin__trap() __builtin_unreachable()
    #else
        //on most debuggers writing to null is a sure way to generate trap
        // we also provide write a string so that it shows in assembly
        #define intrin__trap() (*(const char* volatile*) 0 = "trap() called!")
    #endif
}

//declares a portion of code unreachable making it more likely to be optimized away
// (such as default case in a switch that cannot possibly happen)
static void intrin__unreachable(void)
{
    #if defined(INTRIN_COMPILER__MSVC) \
        && !defined(INTRIN_NO_UNREACHABLE) 

        #define intrin__unreachable() assert(false && "code declared as unreachable reached!"), __assume(0)
    #elif (defined(INTRIN_COMPILER__GNUC) || defined(INTRIN_COMPILER__CLANG)) \
        && !defined(INTRIN_NO_UNREACHABLE) 
    
        #define intrin__unreachable() assert(false && "code declared as unreachable reached!"), __builtin_unreachable()
    #else
        //undefined behaviour via integer overflow
        //This is as good of a try as any
        #define intrin__unreachable()               \
            do                                      \
            {                                       \
                assert(false && "code declared as unreachable reached!");                      \
                int32_t overflowing = 0x7FFFFFFF;   \
                overflowing += 0xF;                 \
            }                                       \
            while(false)                            \

    #endif
}

#ifdef INTRIN_EXAMPLE
#include <cstdio>

int main()
{
    size_t index = 0;
    bool was_found = intrin__find_first_set_32(&index, 0b010110000);
    printf("was_found: %d\n", (int) was_found); //1
    printf("index: %d\n", (int) index);         //4

    if(false) //will not get transleted into asembly
    {
        intrin__trap();
        intrin__unreachable();
    }

    return 0;
}
#endif

#ifdef min
    #undef min
#endif

#ifdef max
    #undef max
#endif

#endif


