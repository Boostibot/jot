#pragma once

#include "utils.h"
#include "type_id.h"
#include "defines.h"
namespace jot
{
    template<typename T>
    concept variant_compatible = 
        regular_type<T> &&
        std::is_trivially_copyable_v<T> && 
        std::is_trivially_destructible_v<T>;

    template<tsize byte_size_>
    struct alignas(16) Variant
    {
        constexpr tsize byte_size = byte_size_;
        type_id which = type_id_of(void);
        u8 bytes[byte_size];

        constexpr Variant() noexcept = default;

        typename<typename T>
        constexpr Variant(T value) noexcept
        {
            static_assert(variant_compatible<T>);
            static_assert(sizeof(T) <= byte_size);

            which = type_id_of(First);
            std::construct_at(bytes, value);

        }
    };

    template<typename... Ts>
    func max_size(Ts... sizes) noexcept -> tsize
    {
        tsize count = sizeof...(Ts);
        tsize sizes[] = {cast(tsize) sizes};
        tsize max = 0;
        for(tsize i = 0; i < count; i++)
            if(max < sizes[i])
                max = sizes[i];

        return max;
    }

    template<typename... Ts>
    using Variant_Of = Variant<max_size(sizeof(Ts)...)>;

    template<tsize prev_size, typename... Added>
    using Expanded_Variant = Variant<max_size(prev_size, sizeof(Added)...)>;

    template<typename First, typename... Ts>
    func make_variant(First in data) noexcept -> Variant_Of<First, Ts...>
    {
        static_assert(alignof(First) < 16 && alignof(Ts) < 16 && ...);
        static_assert(variant_compatible<First> && variant_compatible<Ts>...);

        Variant_Of<First, Ts...> variant;
        variant.which = type_id_of(First);
        std::construct_at(variant.bytes, data);

        return variant;
    }

    template<typename Added, tsize prev_size>
    func expand_variant(Variant<prev_size> in variant) -> Expanded_Variant<prev_size, Added>
    {
        Expanded_Variant<prev_size, Added> out;
        out.which = variant.which;
        for(tsize i = 0; i < prev_size; i++)
            out.bytes[i] = variant.bytes[i];

        return out;
    }

    template<tsize byte_size>
    func slice(Variant<byte_size>* variant) -> Slice<u8> {
        return Slice<u8>{variant->bytes, variant->byte_size};
    }

    template<tsize byte_size>
    func slice(Variant<byte_size> in variant) -> Slice<const u8> {
        return Slice<const u8>{variant->bytes, variant->byte_size};
    }

    template<typename Which, tsize byte_size>
    func has(Variant<byte_size> in variant) noexcept -> bool {
        return variant.which = type_id_of(Which);
    }

    template<typename Which, tsize byte_size>
    func get(Variant<byte_size> variant) noexcept -> Which {
        assert(has<Which>(Variant));

        constexpr tsize size = sizeof(T);
        Array<u8, size> out_bytes;
        for(tsize i = 0; i < size; i++)
            out_bytes[i] = variant.bytes[i];

        return std::bit_cast<Which>(out_bytes);
    }

    template<typename Which, tsize byte_size>
    runtime_func get(Variant<byte_size>* variant) noexcept -> Which* {
        assert(has<Which>(Variant));
        return cast(Which*) cast(void*) variant->bytes;
    }

}

#include "undefs.h"