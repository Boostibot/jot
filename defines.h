#if defined(__GNUC__) || defined(__clang__)
    #define RESTRICT __restrict__
    #define FORCE_INLINE __attribute__((always_inline))
    #define NO_INLINE __attribute__((noinline))
    
#elif defined(_MSC_VER)
    #define RESTRICT __restrict
    #define FORCE_INLINE __forceinline
    #define NO_INLINE __declspec(noinline)

#else
    #define RESTRICT
    #define FORCE_INLINE
    #define NO_INLINE

#endif

#if defined(_MSC_VER)
    #define NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
    #define NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif