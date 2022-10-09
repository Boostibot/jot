#define let const auto
#define mut auto
#define pure [[nodiscard]]
#define proc constexpr auto
#define func pure constexpr auto
#define runtime_proc pure auto
#define runtime_func auto
#define discard cast(void)

#define cast(...) (__VA_ARGS__)
#define maybe_unused [[maybe_unused]] 		
#define address_overlap [[no_unique_address]]