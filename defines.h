#define let const auto
#define mut auto
#define nodisc [[nodiscard]]
#define proc nodisc constexpr auto
#define func nodisc constexpr auto
#define runtime_proc nodisc auto
#define runtime_func nodisc auto
#define in const&
#define moved &&
#define no_alias __restrict

#define cast(...) (__VA_ARGS__)
#define address_alias [[no_unique_address]]

