#define nodisc [[nodiscard]]
#define cast(...) (__VA_ARGS__)

#if defined(__clang__)
    #define RESTRICT __restrict__
#elif defined(__GNUC__)
    #define RESTRICT __restrict__
#elif defined(_MSC_VER)
    #define RESTRICT __restrict
#else
    #define RESTRICT
#endif

#if defined(__GNUC__)
    #define FORCE_INLINE __attribute__((always_inline))
#elif defined(_MSC_VER) && !defined(__clang__)
    #define FORCE_INLINE __forceinline
#else
    #define FORCE_INLINE
#endif

#if defined(__GNUC__)
    #define NO_INLINE __attribute__((noinline))
#elif defined(_MSC_VER) && !defined(__clang__)
    #define NO_INLINE __declspec(noinline)
#else
    #define NO_INLINE
#endif

#if defined(_MSC_VER)
    #define NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
    #define NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif

#ifndef MOVE_DEFINED
#define MOVE_DEFINED

    template <typename T> nodisc constexpr 
    T && move(T* val) { return cast(T &&) *val; };

#endif

#define forward(T, ...) (T &&) (__VA_ARGS__)