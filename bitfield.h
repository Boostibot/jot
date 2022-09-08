#pragma once
#include "bits.h"
#include "endian.h"
#include "defines.h"

namespace jot
{
    func div_round_up(let value, let to_multiple_of)
    {
        return (value + to_multiple_of - 1) / to_multiple_of;
    }

    
    template<typename T>
    func max() -> T
    {   
        return T();
    }

    template<typename T>
    func min() -> T
    {   
        return T();
    }

    template<typename T>
    func max(T first) -> T
    {   
        return first;
    }

    template<typename T>
    func min(T first) -> T
    {   
        return first;
    }

    template<typename T, typename ...Ts>
    func max(T first, Ts... values)
        requires (std::convertible_to<Ts, T> && ...)
    {
        let rest_max = max(values...);
        if(rest_max > first)
            return cast(T) rest_max;
        else
            return cast(T) first;
    }

    template<typename T, typename ...Ts>
    func min(T first, Ts... values...)
        requires (std::convertible_to<Ts, T> && ...)
    {
        let rest_max = max(values...);
        if(rest_max < first)
            return cast(T) rest_max;
        else
            return cast(T) first;
    }
    
    template <typename Val, typename Container>
    func get_array_bytefield(const Container containers[], let from_byte, let num_bytes, const Val& base = Val())
    {
        assert(num_bytes <= sizeof(Val));

        let placement = lower_bytes_offset(num_bytes, sizeof(Val));
        mut copy = base;
        copy_bytes(&copy, placement, containers, from_byte, num_bytes);
        return copy;
    }

    template <typename Val, typename Container>
    proc set_array_bytefield(Container containers[], let from_byte, let num_bytes, const Val& val)
    {
        assert(num_bytes <= sizeof(Val));

        let placement = lower_bytes_offset(num_bytes, sizeof(Val));
        copy_bytes(containers, from_byte, &val, placement, num_bytes);
    }

    template <typename Val, typename Container>
    func set_bytefield(const Container& container, let from_byte, let num_bytes, const Val& val)
    {
        assert(num_bytes <= sizeof(Val));
        assert(from_byte + num_bytes <= sizeof(Container));

        let placement = lower_bytes_offset(num_bytes, sizeof(Val));
        mut cont_copy = container;
        copy_bytes(&cont_copy, from_byte, &val, placement, num_bytes);
        return cont_copy;
    }

    template <typename Val, typename Container>
    func get_bytefield(const Container& container, let from_byte, let num_bytes, const Val& base = Val())
    {
        assert(num_bytes <= sizeof(Val));
        assert(from_byte + num_bytes <= sizeof(Container));

        let placement = lower_bytes_offset(num_bytes, sizeof(Val));
        mut val = base;
        copy_bytes(&val, placement, &container, from_byte, num_bytes);
        return val;
    }

    template <typename Val, typename Container>
    func get_array_bytefield(const Container containers[], let from_byte)
    {
        return get_array_bytefield<Val>(containers, from_byte, sizeof(Val));
    }

    template <typename Val, typename Container>
    proc set_array_bytefield(Container containers[], let from_byte, const Val& val)
    {
        return set_array_bytefield(containers, from_byte, sizeof(Val), val);
    }

    template <typename Val, typename Container, typename Max = u64>
    func get_bitfield(const Container& container, let from_bit, let num_bits, const Val& base = Val())
    {
        assert(num_bits <= bitsof(Val));
        assert(from_bit + num_bits <= bitsof(Container));

        let mask = bitmask_lower<Max, Max>(num_bits);
        let promoted_field = cast(Max) container;
        let promoted_base = cast(Max) base;

        return cast(Val)(((promoted_field >> from_bit) & mask) | (promoted_base & ~mask));
    }

    template <typename Val, typename Container, typename Max = u64>
    func set_bitfield(const Container& container, let from_bit, let num_bits, const Val& to_value)
    {
        assert(num_bits <= bitsof(Val));
        assert(from_bit + num_bits <= bitsof(Container));

        let mask = bitmask<Max, Max>(from_bit, num_bits);
        let promoted_field = cast(Max) container;
        let promoted_value = cast(Max) to_value;

        return cast(Container)(((promoted_value << from_bit) & mask) | (promoted_field & ~mask));
    }

    template <typename Val, typename Container, typename Max = u64>
    func get_array_bitfield(const Container containers[], let from_bit, let num_bits, const Val& base = Val())
    {
        assert(num_bits <= bitsof(Val));

        let from_cont = from_bit / bitsof(Container);
        let from_cont_offset = from_bit % bitsof(Container);
        let total_cont_size = div_round_up(from_cont_offset + num_bits, bitsof(Container)) * sizeof(Container);

        //Get the containing bytes then get the containing bits and retunr them
        let span = get_array_bytefield<Max>(containers + from_cont, 0, total_cont_size);
        let temp = get_bitfield<Max, Max, Max>(span, from_cont_offset, num_bits, base);
        return cast(Val) temp;
    }

    template <typename Val, typename Container, typename Max = u64>
    proc set_array_bitfield(Container containers[], let from_bit, let num_bits, const Val& to_value)
    {
        assert(num_bits <= bitsof(Val));

        let from_cont = from_bit / bitsof(Container);
        let from_cont_offset = from_bit % bitsof(Container);
        let total_cont_size = div_round_up(from_cont_offset + num_bits, bitsof(Container)) * sizeof(Container);

        //Get old bytes
        let read_span = get_array_bytefield<Max>(containers + from_cont, 0, total_cont_size);

        //Blend new values with old bytes
        let span = set_bitfield<Max, Max, Max>(read_span, from_cont_offset, num_bits, to_value);

        //Put new bytes back
        set_array_bytefield(containers + from_cont, 0, total_cont_size, span);
    }

    struct BitfieldTag {};

    template <class T, size_t bit_count_>
    struct Bitfield
    {
        using Tag = BitfieldTag;
        using type = T;
        static constexpr size_t bit_count = bit_count_;
    };

    //Class to facilitate the use of bitfields - declare a BitStorage as a list of Bitfields
    // and then simply get/set them using the appropriate types
    template <class... Fields> requires (Tagged<Fields, BitfieldTag> && ...)
    struct BitStorage
    {
        static_assert(sizeof...(Fields) > 0, "At least one field must be set");

        template <size_t field_count_>
        struct Info 
        {
            using SizeArray = Array<size_t, field_count_>;
            SizeArray bit_count;
            SizeArray from_bit;
            SizeArray to_bit;

            size_t field_count;
            size_t total_byte_size;
            size_t total_bit_count;
        };

        template <typename ...Sizes>
        static proc get_info(Sizes ...bitcounts)
        {
            assert(max(bitcounts...) > 0 && "At least one field has to have not 0");
            assert(max(bitcounts...) < BIT_COUNT<u64> && "All sizes must be less then 8 bytes");

            BitStorage::Info<sizeof...(bitcounts)> info;
            info.bit_count = {bitcounts...};
            info.field_count = sizeof...(bitcounts);

            size_t current_bit = 0;
            for(size_t i = 0; i < info.field_count; i++)
            {
                info.from_bit[i] = current_bit;
                current_bit += info.bit_count[i];
                info.to_bit[i] = current_bit;
            }

            info.total_bit_count = current_bit;
            info.total_byte_size = div_round_up(info.total_bit_count, CHAR_BIT);
            return info;
        }

        template <size_t index>
        using Field = jot::nth_type<index, Fields...>;

        template <size_t index>
        using FieldType = typename Field<index>::type;

        using Max = u64;

        static constexpr let info = BitStorage::get_info(Fields::bit_count...);
        static constexpr size_t field_count = info.field_count;
        static constexpr size_t bit_count = info.total_bit_count;
        static constexpr size_t byte_size = info.total_byte_size;

        using Data = byte[byte_size];
        Data data;

        //Is in its own struct so it can be typedefed and used exactly like the non array variants
        struct array
        {
            template <size_t field_i, typename Item = FieldType<field_i>, typename Container>
            pure static proc get(const Container containers[], const Item& base = Item())
            {
                static_assert(field_i < info.field_count);
                return get_array_bitfield<Item, Container, Max>(containers, info.from_bit[field_i], info.bit_count[field_i], base);
            }

            template <size_t field_i, typename Item = FieldType<field_i>, typename Container>
            static proc set(Container containers[], const Item& to_value)
            {
                static_assert(field_i < info.field_count);
                return set_array_bitfield<Item, Container, Max>(containers, info.from_bit[field_i], info.bit_count[field_i], to_value);
            }
        };

        template <size_t field_i, typename Item = FieldType<field_i>, typename Container>
        pure static proc get(const Container container, const Item& base = Item())
        {
            static_assert(field_i < info.field_count);
            return get_bitfield<Item, Container, Max>(container, info.from_bit[field_i], info.bit_count[field_i], base);
        }

        template <size_t field_i, typename Item = FieldType<field_i>, typename Container>
        static proc set(Container container, const Item& to_value)
        {
            static_assert(field_i < info.field_count);
            return set_bitfield<Item, Container, Max>(container, info.from_bit[field_i], info.bit_count[field_i], to_value);
        }

        template <size_t field_i>
        func get() const                             { return BitStorage::array::get<field_i, FieldType<field_i>, byte>(this->data); }

        template <size_t field_i>
        proc set(const FieldType<field_i>& to_value) { return BitStorage::array::set<field_i, FieldType<field_i>, byte>(this->data, to_value); }
    };
}

#include "undefs.h"