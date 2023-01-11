#define let const auto
#define mut auto
#define nodisc [[nodiscard]]
#define proc nodisc auto
#define func nodisc  auto
#define in const&
#define moved &&
#define no_alias __restrict
#define cast(...) (__VA_ARGS__)

#if defined(_MSC_VER)
    #define address_alias [[msvc::no_unique_address]]
#else
    #define address_alias [[no_unique_address]]
#endif

#ifndef MOVE_DEFINED
#define MOVE_DEFINED

    template <typename T>
    constexpr func move(T* val) noexcept -> T && {
        return cast(T &&) *val;
    };

#endif

#define forward(T, ...) (T &&) (__VA_ARGS__)