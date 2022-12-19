#pragma once
#include "bits.h"
#include "endian.h"
#include "defines.h"

namespace jot
{
    #define bitsof(...) (sizeof(__VA_ARGS__) * CHAR_BIT)

    template <typename Val, typename Container>
    func get_bytefield_in_array(const Container containers[], size_t from_byte, size_t num_bytes, Val in base = Val()) noexcept -> Val
    {
        assert(num_bytes <= sizeof(Val));

        let placement = lower_bytes_offset(num_bytes, sizeof(Val));
        mut copy = base;
        copy_bytes(&copy, placement, containers, from_byte, num_bytes);
        return copy;
    }

    template <typename Val, typename Container>
    proc set_bytefield_in_array(Container containers[], size_t from_byte, size_t num_bytes, Val in val) noexcept -> void
    {
        assert(num_bytes <= sizeof(Val));

        let placement = lower_bytes_offset(num_bytes, sizeof(Val));
        copy_bytes(containers, from_byte, &val, placement, num_bytes);
    }

    template <typename Val, typename Container>
    func set_bytefield(Container in container, size_t from_byte, size_t num_bytes, Val in val) noexcept -> Container
    {
        assert(num_bytes <= sizeof(Val));
        assert(from_byte + num_bytes <= sizeof(Container));

        let placement = lower_bytes_offset(num_bytes, sizeof(Val));
        mut cont_copy = container;
        copy_bytes(&cont_copy, from_byte, &val, placement, num_bytes);
        return cont_copy;
    }

    template <typename Val, typename Container>
    func get_bytefield(Container in container, size_t from_byte, size_t num_bytes, Val in base = Val()) noexcept -> Val
    {
        assert(num_bytes <= sizeof(Val));
        assert(from_byte + num_bytes <= sizeof(Container));

        let placement = lower_bytes_offset(num_bytes, sizeof(Val));
        mut val = base;
        copy_bytes(&val, placement, &container, from_byte, num_bytes);
        return val;
    }

    template <typename Val, typename Container>
    func get_bytefield_in_array(const Container containers[], size_t from_byte) noexcept -> Val
    {
        return get_bytefield_in_array<Val>(containers, from_byte, sizeof(Val));
    }

    template <typename Val, typename Container>
    proc set_bytefield_in_array(Container containers[], size_t from_byte, Val in val) noexcept -> void
    {
        return set_bytefield_in_array(containers, from_byte, sizeof(Val), val);
    }

    template <typename Val, typename Container>
    func get_bitfield(Container in container, size_t from_bit, size_t num_bits, Val in base = Val()) noexcept -> Val
    {
        assert(num_bits <= bitsof(Val));
        assert(from_bit + num_bits <= bitsof(Container));

        let mask = low_mask<Max_Field>(num_bits);
        let promoted_field = cast(Max_Field) container;
        let promoted_base = cast(Max_Field) base;

        return cast(Val)(((promoted_field >> from_bit) & mask) | (promoted_base & ~mask));
    }

    template <typename Val, typename Container>
    func set_bitfield(Container in container, size_t from_bit, size_t num_bits, Val in to_value) noexcept -> Container
    {
        assert(num_bits <= bitsof(Val));
        assert(from_bit + num_bits <= bitsof(Container));

        let mask = range_mask<Max_Field>(from_bit, from_bit + num_bits);
        let promoted_field = cast(Max_Field) container;
        let promoted_value = cast(Max_Field) to_value;

        return cast(Container)(((promoted_value << from_bit) & mask) | (promoted_field & ~mask));
    }

    template <typename Val, typename Container>
    func get_bitfield_in_array(const Container containers[], size_t from_bit, size_t num_bits, Val in base = Val()) noexcept -> Val
    {
        assert(num_bits <= bitsof(Val));

        let from_cont = from_bit / bitsof(Container);
        let from_cont_offset = from_bit % bitsof(Container);
        let total_cont_size = div_round_up(from_cont_offset + num_bits, bitsof(Container)) * sizeof(Container);

        //Get the containing bytes then get the containing bits and retunr them
        let span = get_bytefield_in_array<Max_Field>(containers + from_cont, 0, total_cont_size);
        let temp = get_bitfield<Max_Field, Max_Field>(span, from_cont_offset, num_bits, base);
        return cast(Val) temp;
    }

    template <typename Val, typename Container>
    proc set_bitfield_in_array(Container containers[], size_t from_bit, size_t num_bits, Val in to_value) noexcept -> void
    {
        assert(num_bits <= bitsof(Val));

        let from_cont = from_bit / bitsof(Container);
        let from_cont_offset = from_bit % bitsof(Container);
        let total_cont_size = div_round_up(from_cont_offset + num_bits, bitsof(Container)) * sizeof(Container);

        //Get old bytes
        let read_span = get_bytefield_in_array<Max_Field>(containers + from_cont, 0, total_cont_size);

        //Blend new values with old bytes
        let span = set_bitfield<Max_Field, Max_Field>(read_span, from_cont_offset, num_bits, to_value);

        //Put new bytes back
        set_bytefield_in_array(containers + from_cont, 0, total_cont_size, span);
    }

    struct Bitfield_Tag {};
    struct Bit_Storage_Tag {};

    template <class T, size_t bit_count_>
    struct Bitfield : Bitfield_Tag
    {
        using type = T;
        static constexpr size_t bit_count = bit_count_;
    };

    //Class to facilitate the use of bitfields - declare a Bit_Storage as a list of Bitfields
    // and then simply get/set them using the appropriate types
    template <class... Fields> 
    struct Bit_Storage : Bit_Storage_Tag
    {
        static_assert(std::is_base_of_v<Bitfield_Tag, Fields> && ..., "fields must be bitfields!");
        static_assert(sizeof...(Fields) > 0, "At least one field must be set");

        static constexpr size_t field_count = sizeof...(Fields);

        struct Info 
        {
            using SizeArray = Array_<size_t, field_count>;
            SizeArray bit_count;
            SizeArray from_bit;
            SizeArray to_bit;

            size_t field_count;
            size_t total_byte_size;
            size_t total_bit_count;
        };

        template <typename ...Sizes>
        static func get_info(Sizes ...bitcounts) -> Info
        {
            assert(max(bitcounts...) > 0 && "At least one field has to have not 0");
            assert(max(bitcounts...) < BIT_COUNT<u64> && "All sizes must be less then 8 bytes");

            Info info;
            info.bit_count = {bitcounts...};
            info.field_count = field_count;

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

        static constexpr Info info = Bit_Storage::get_info(Fields::bit_count...);
        static constexpr size_t bit_count = info.total_bit_count;
        static constexpr size_t byte_size = info.total_byte_size;

        template <size_t index>
        using Field = jot::nth_type<index, Fields...>;

        template <size_t index>
        using FieldType = typename Field<index>::type;

        using Data = byte[byte_size];
        Data data;

        //Is in its own struct so it can be typedefed and used exactly like the non array variants
        struct array
        {
            template <size_t field_i, typename Item = FieldType<field_i>, typename Container>
            pure static proc get(const Container containers[], Item in base = Item()) noexcept -> Item
            {
                static_assert(field_i < info.field_count);
                return get_bitfield_in_array<Item, Container>(containers, info.from_bit[field_i], info.bit_count[field_i], base);
            }

            template <size_t field_i, typename Item = FieldType<field_i>, typename Container>
            static proc set(Container containers[], Item in to_value) noexcept -> void
            {
                static_assert(field_i < info.field_count);
                return set_bitfield_in_array<Item, Container>(containers, info.from_bit[field_i], info.bit_count[field_i], to_value);
            }
        };

        template <size_t field_i, typename Item = FieldType<field_i>, typename Container>
        static func get(Container in container, Item in base = Item()) noexcept -> Item
        {
            static_assert(field_i < info.field_count);
            return get_bitfield<Item, Container>(container, info.from_bit[field_i], info.bit_count[field_i], base);
        }

        template <size_t field_i, typename Item = FieldType<field_i>, typename Container>
        static func set(Container in container, Item in to_value) noexcept -> Container
        {
            static_assert(field_i < info.field_count);
            return set_bitfield<Item, Container>(container, info.from_bit[field_i], info.bit_count[field_i], to_value);
        }

        //Would be too hard to implement as a free function
        template <size_t field_i>
        func get() noexcept const -> FieldType<field_i>        
        { 
            return Bit_Storage::array::get<field_i, FieldType<field_i>, byte>(this->data); 
        }

        template <size_t field_i>
        proc set(const FieldType<field_i>& to_value) noexcept -> void
        { 
            return Bit_Storage::array::set<field_i, FieldType<field_i>, byte>(this->data, to_value); 
        }
    };

    #undef bitsof
}

#include "undefs.h"