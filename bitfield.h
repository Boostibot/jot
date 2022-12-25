#pragma once
#include "bits.h"
#include "endian.h"
#include "defines.h"

namespace jot
{
    #define bitsof(...) (sizeof(__VA_ARGS__) * CHAR_BIT)

    #undef func
    #undef proc
    #define func constexpr nodisc auto
    #define proc constexpr auto


    template <typename Val, typename Container>
    func get_bytefield_in_array(const Container containers[], isize from_byte, isize num_bytes, Val in base = Val()) noexcept -> Val
    {
        assert(num_bytes <= sizeof(Val));

        isize placement = offset_from_low_bytes(num_bytes, sizeof(Val));
        Val copy = base;
        copy_bytes(&copy, placement, containers, from_byte, num_bytes);
        return copy;
    }

    template <typename Val, typename Container>
    proc set_bytefield_in_array(Container containers[], isize from_byte, isize num_bytes, Val in val) noexcept -> void
    {
        assert(num_bytes <= sizeof(Val));

        isize placement = offset_from_low_bytes(num_bytes, sizeof(Val));
        copy_bytes(containers, from_byte, &val, placement, num_bytes);
    }

    template <typename Val, typename Container>
    func set_bytefield(Container in container, isize from_byte, isize num_bytes, Val in val) noexcept -> Container
    {
        assert(num_bytes <= sizeof(Val));
        assert(from_byte + num_bytes <= sizeof(Container));

        isize placement = offset_from_low_bytes(num_bytes, sizeof(Val));
        Container cont_copy = container;
        copy_bytes(&cont_copy, from_byte, &val, placement, num_bytes);
        return cont_copy;
    }

    template <typename Val, typename Container>
    func get_bytefield(Container in container, isize from_byte, isize num_bytes, Val in base = Val()) noexcept -> Val
    {
        assert(num_bytes <= sizeof(Val));
        assert(from_byte + num_bytes <= sizeof(Container));

        isize placement = offset_from_low_bytes(num_bytes, sizeof(Val));
        Val val = base;
        copy_bytes(&val, placement, &container, from_byte, num_bytes);
        return val;
    }

    template <typename Val, typename Container>
    func get_bytefield_in_array(const Container containers[], isize from_byte) noexcept -> Val
    {
        return get_bytefield_in_array<Val>(containers, from_byte, sizeof(Val));
    }

    template <typename Val, typename Container>
    proc set_bytefield_in_array(Container containers[], isize from_byte, Val in val) noexcept -> void
    {
        return set_bytefield_in_array(containers, from_byte, sizeof(Val), val);
    }

    template <typename Val, typename Container>
    func get_bitfield(Container in container, isize from_bit, isize num_bits, Val in base = Val()) noexcept -> Val
    {
        assert(num_bits <= bitsof(Val));
        assert(from_bit + num_bits <= bitsof(Container));

        Max_Field mask = low_mask<Max_Field>(num_bits);
        Max_Field promoted_field = cast(Max_Field) container;
        Max_Field promoted_base = cast(Max_Field) base;

        return cast(Val)(((promoted_field >> from_bit) & mask) | (promoted_base & ~mask));
    }

    template <typename Val, typename Container>
    func set_bitfield(Container in container, isize from_bit, isize num_bits, Val in to_value) noexcept -> Container
    {
        assert(num_bits <= bitsof(Val));
        assert(from_bit + num_bits <= bitsof(Container));

        Max_Field mask = range_mask<Max_Field>(from_bit, from_bit + num_bits);
        Max_Field promoted_field = cast(Max_Field) container;
        Max_Field promoted_value = cast(Max_Field) to_value;

        return cast(Container)(((promoted_value << from_bit) & mask) | (promoted_field & ~mask));
    }

    template <typename Val, typename Container>
    func get_bitfield_in_array(const Container containers[], isize from_bit, isize num_bits, Val in base = Val()) noexcept -> Val
    {
        assert(num_bits <= bitsof(Val));

        isize from_cont = from_bit / bitsof(Container);
        isize from_cont_offset = from_bit % bitsof(Container);
        isize total_cont_size = div_round_up(from_cont_offset + num_bits, bitsof(Container)) * sizeof(Container);

        //Get the containing bytes then get the containing bits and retunr them
        Max_Field span = get_bytefield_in_array<Max_Field>(containers + from_cont, 0, total_cont_size);
        Max_Field temp = get_bitfield<Max_Field, Max_Field>(span, from_cont_offset, num_bits, base);
        return cast(Val) temp;
    }

    template <typename Val, typename Container>
    proc set_bitfield_in_array(Container containers[], isize from_bit, isize num_bits, Val in to_value) noexcept -> void
    {
        assert(num_bits <= bitsof(Val));

        isize from_cont = from_bit / bitsof(Container);
        isize from_cont_offset = from_bit % bitsof(Container);
        isize total_cont_size = div_round_up(from_cont_offset + num_bits, bitsof(Container)) * sizeof(Container);

        //Get old bytes
        Max_Field read_span = get_bytefield_in_array<Max_Field>(containers + from_cont, 0, total_cont_size);

        //Blend new values with old bytes
        Max_Field span = set_bitfield<Max_Field, Max_Field>(read_span, from_cont_offset, num_bits, to_value);

        //Put new bytes back
        set_bytefield_in_array(containers + from_cont, 0, total_cont_size, span);
    }

    struct Bitfield_Tag {};
    struct Bit_Storage_Tag {};

    template <class T, isize bit_count_>
    struct Bitfield : Bitfield_Tag
    {
        using type = T;
        static constexpr isize bit_count = bit_count_;
    };

    //Class to facilitate the use of bitfields - declare a Bit_Storage as a list of Bitfields
    // and then simply get/set them using the appropriate types
    template <class... Fields> 
    struct Bit_Storage : Bit_Storage_Tag
    {
        static_assert(std::is_base_of_v<Bitfield_Tag, Fields> && ..., "fields must be bitfields!");
        static_assert(sizeof...(Fields) > 0, "At least one field must be set");

        static constexpr isize field_count = sizeof...(Fields);

        struct Info 
        {
            using SizeArray = Array_<isize, field_count>;
            SizeArray bit_count;
            SizeArray from_bit;
            SizeArray to_bit;

            isize field_count;
            isize total_byte_size;
            isize total_bit_count;
        };

        template <typename ...Sizes>
        static func get_info(Sizes ...bitcounts) -> Info
        {
            assert(max(bitcounts...) > 0 && "At least one field has to have not 0");
            assert(max(bitcounts...) < BIT_COUNT<u64> && "All sizes must be less then 8 bytes");

            Info info;
            info.bit_count = {bitcounts...};
            info.field_count = field_count;

            isize current_bit = 0;
            for(isize i = 0; i < info.field_count; i++)
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
        static constexpr isize bit_count = info.total_bit_count;
        static constexpr isize byte_size = info.total_byte_size;

        template <isize index>
        using Field = jot::nth_type<index, Fields...>;

        template <isize index>
        using FieldType = typename Field<index>::type;

        using Data = byte[byte_size];
        Data data;

        //Is in its own struct so it can be typedefed and used exactly like the non array variants
        struct array
        {
            template <isize field_i, typename Item = FieldType<field_i>, typename Container = int>
            static func get(const Container containers[], Item in base = Item()) noexcept -> Item
            {
                static_assert(field_i < info.field_count, "index must be in range");
                return get_bitfield_in_array<Item, Container>(containers, info.from_bit[field_i], info.bit_count[field_i], base);
            }

            template <isize field_i, typename Item = FieldType<field_i>, typename Container = int>
            static proc set(Container containers[], Item in to_value) noexcept -> void
            {
                static_assert(field_i < info.field_count, "index must be in range");
                return set_bitfield_in_array<Item, Container>(containers, info.from_bit[field_i], info.bit_count[field_i], to_value);
            }
        };

        template <isize field_i, typename Item = FieldType<field_i>, typename Container = int>
        static func get(Container in container, Item in base = Item()) noexcept -> Item
        {
            static_assert(field_i < info.field_count, "index must be in range");
            return get_bitfield<Item, Container>(container, info.from_bit[field_i], info.bit_count[field_i], base);
        }

        template <isize field_i, typename Item = FieldType<field_i>, typename Container = int>
        static func set(Container in container, Item in to_value) noexcept -> Container
        {
            static_assert(field_i < info.field_count, "index must be in range");
            return set_bitfield<Item, Container>(container, info.from_bit[field_i], info.bit_count[field_i], to_value);
        }

        //Would be too hard to implement as a free function
        template <isize field_i>
        func get() noexcept const -> FieldType<field_i>        
        { 
            return Bit_Storage::array::get<field_i, FieldType<field_i>, byte>(this->data); 
        }

        template <isize field_i>
        proc set(const FieldType<field_i>& to_value) noexcept -> void
        { 
            return Bit_Storage::array::set<field_i, FieldType<field_i>, byte>(this->data, to_value); 
        }
    };

    #undef bitsof
}

#include "undefs.h"