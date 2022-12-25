#pragma once

#include "utils.h"
#include "type_id.h"
#include "types.h"
#include "slice.h"
#include "bits.h"
#include "defines.h"
namespace jot
{
    template<typename T>
    concept variant_compatible = 
        regular_type<T> &&
        std::is_trivially_copyable_v<T> && 
        std::is_trivially_destructible_v<T>;
    
    //Type erased POD variant using type_id as tag
    // Can store any type that fits inside its data and align
    template<isize byte_size_, isize align_>
    struct alignas(align_) Variant
    {
        static constexpr isize byte_size = byte_size_;
        static constexpr isize align = align_;

        type_id which = type_id_of(void);
        u8 bytes[byte_size];

        constexpr Variant() noexcept = default;

        template<typename T>
        constexpr Variant(T value) noexcept
        {
            static_assert(variant_compatible<T>, "must be variant comaptible");
            static_assert(sizeof(T) <= byte_size && alignof(T) <= align, "must fit");

            which = type_id_of(T);
            std::construct_at(bytes, value);
        }
    };

    template<typename... Ts>
    constexpr func max_size(Ts... sizes) noexcept -> isize
    {
        isize count = sizeof...(Ts);
        isize sizes[] = {cast(isize) sizes};
        isize max = 0;
        for(isize i = 0; i < count; i++)
            if(max < sizes[i])
                max = sizes[i];

        return max;
    }

    template<typename... Ts>
    using Variant_Of = Variant<
        max_size(sizeof(Ts)...), 
        max_size(alignof(Ts)...)>;

    template<isize prev_size, isize prev_align, typename... Added>
    using Expanded_Variant = Variant<
        max_size(prev_size, sizeof(Added)...), 
        max_size(prev_align, alignof(Added)...)>;

    template<typename First, typename... Ts>
    constexpr func make_variant(First in data) noexcept -> Variant_Of<First, Ts...>
    {
        static_assert(variant_compatible<First> && (variant_compatible<Ts> && ...), "all types must be variant compatible");

        Variant_Of<First, Ts...> variant;
        variant.which = type_id_of(First);
        std::construct_at(variant.bytes, data);

        return variant;
    }

    template<typename Added, isize prev_size, isize prev_align>
    constexpr func expand_variant(Variant<prev_size, prev_align> in variant) -> Expanded_Variant<prev_size, prev_align, Added>
    {
        static_assert(variant_compatible<Added>, "must be variant compatible");

        Expanded_Variant<prev_size, prev_align, Added> out;
        out.which = variant.which;
        for(isize i = 0; i < prev_size; i++)
            out.bytes[i] = variant.bytes[i];

        return out;
    }

    template<isize byte_size, isize align>
    constexpr func slice(Variant<byte_size, align>* variant) -> Slice<u8> {
        return Slice<u8>{variant->bytes, variant->byte_size};
    }

    template<isize byte_size, isize align>
    constexpr func slice(Variant<byte_size, align> in variant) -> Slice<const u8> {
        return Slice<const u8>{variant->bytes, variant->byte_size};
    }

    template<typename Which, isize byte_size, isize align>
    constexpr func has(Variant<byte_size, align> in variant) noexcept -> bool {
        return variant.which = type_id_of(Which);
    }

    template<typename Which, isize byte_size, isize align>
    constexpr func get(Variant<byte_size, align> variant) noexcept -> Which {
        assert(has<Which>(Variant));

        constexpr isize size = sizeof(Which);
        Array<u8, size> out_bytes;
        for(isize i = 0; i < size; i++)
            out_bytes[i] = variant.bytes[i];

        return bit_cast<Which>(out_bytes);
    }

    template<typename Which, isize byte_size, isize align>
    func get(Variant<byte_size, align>* variant) noexcept -> Which* {
        assert(has<Which>(Variant));
        return cast(Which*) cast(void*) variant->bytes;
    }

}

#include "undefs.h"