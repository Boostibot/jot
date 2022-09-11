#pragma once

#include "endian.h"
#include "array.h"

#include "defines.h"

namespace jot::egnostic
{
    namespace shifts 
    {
        constexpr size_t MAX_SUPPORTED_SIZE = 8;
        using Row = Array<u8, MAX_SUPPORTED_SIZE>;
        using Table = Array<Row, MAX_SUPPORTED_SIZE + 1>;

        constexpr Table LITTLE = {
            Row{0},
            Row{0},
            Row{0, 8},
            Row{0, 8, 16},
            Row{0, 8, 16, 24},
            Row{0, 8, 16, 24, 32},
            Row{0, 8, 16, 24, 32, 40},
            Row{0, 8, 16, 24, 32, 40, 48},
            Row{0, 8, 16, 24, 32, 40, 48, 56},
        };

        constexpr Table BIG = {
            Row{0},
            Row{0},
            Row{8, 0},
            Row{16, 8, 0},
            Row{24, 16, 8, 0},
            Row{32, 24, 16, 8, 0},
            Row{40, 32, 24, 16, 8, 0},
            Row{48, 40, 32, 24, 16, 8, 0},
            Row{56, 48, 40, 32, 24, 16, 8, 0},
        };

        //! mark lenghts that are not supposed to be represented 
        //big endian words composed in little endian manner
        constexpr Table BIG_WORD = {
            Row{0},
            Row{0},
            Row{8, 0},
            Row{8, 0, 16}, //!
            Row{8, 0, 24, 16},
            Row{8, 0, 24, 16, 32}, //!
            Row{8, 0, 24, 16, 40, 32},
            Row{8, 0, 24, 16, 40, 32, 48}, //!
            Row{8, 0, 24, 16, 40, 32, 56, 48},
        };

        //little endian words composed in big endian manner
        constexpr Table LITTLE_WORD = {
            Row{0},
            Row{0},
            Row{0, 8},
            Row{16, 0, 8}, //!
            Row{16, 24, 0, 8},
            Row{32, 16, 24, 0, 8}, //!
            Row{32, 40, 16, 24, 0, 8},
            Row{48, 32, 40, 16, 24, 0, 8}, //!
            Row{48, 56, 32, 40, 16, 24, 0, 8},
        };

        constexpr let HONEYWELL = LITTLE_WORD;
        constexpr let PDP = BIG_WORD; 
    }

    template <typename RetT>
    proc from_shifted_bytes(mut&& bytes, let& shifts, let count) -> RetT
    {
        RetT ret = 0;
        for(mut i = count; i-- > 0;)
        {
            assert(sizeof(RetT) * 8 > shifts[i] && "Shift should never cause the value to copmpletely 'shift out' (undefined behaviour)");
            ret |= cast(RetT)(bytes[i]) << shifts[i];
        }

        return ret;
    }

    template <typename RetT>
    proc to_shifted_bytes(RetT&& num, mut&& bytes, let& shifts, let count)
    {
        using ByteType = typename std::remove_cvref<decltype(bytes[0])>::type; 

        for(mut i = count; i-- > 0;)
        {
            assert(sizeof(RetT) * 8 > shifts[i] && "Shift should never cause the value to copmpletely 'shift out' (undefined behaviour)");
            bytes[i] = cast(ByteType)(num >> shifts[i]);
        }
    }

    template <typename RetT = u64, typename BytesT>
    proc from_endian(BytesT&& bytes, Endian_Base endian)
    {
        using std::forward;
        let size = std::size(bytes);

        assert(size <= 8 && "Only sizes up to 8 bytes are supported");

        switch(endian)
        {
            break; case Endian::Little:
                return from_shifted_bytes<RetT>(forward<BytesT>(bytes), shifts::LITTLE[size], size);

            break; case Endian::Big:
                return from_shifted_bytes<RetT>(forward<BytesT>(bytes), shifts::BIG[size], size);

            break; case Endian::LittleWord:
                return from_shifted_bytes<RetT>(forward<BytesT>(bytes), shifts::LITTLE_WORD[size], size);

            break; case Endian::BigWord:
                return from_shifted_bytes<RetT>(forward<BytesT>(bytes), shifts::BIG_WORD[size], size);
        }

        return static_cast<RetT>(0);
    }

    template <typename BytesT>
    proc to_endian(let num, BytesT&& bytes, Endian_Base endian)
    {
        using std::forward;
        let size = std::size(bytes);

        assert(size <= 8 && "Only sizes up to 8 bytes are supported");

        switch(endian)
        {
            break; case Endian::Little:
                return to_shifted_bytes(num, forward<BytesT>(bytes), shifts::LITTLE[size], size);

            break; case Endian::Big:
                return to_shifted_bytes(num, forward<BytesT>(bytes), shifts::BIG[size], size);

            break; case Endian::LittleWord:
                return to_shifted_bytes(num, forward<BytesT>(bytes), shifts::LITTLE_WORD[size], size);

            break; case Endian::BigWord:
                return to_shifted_bytes(num, forward<BytesT>(bytes), shifts::BIG_WORD[size], size);
        }
    }
}

#include "undefs.h"