#pragma once

#include <cstdio>
#include <charconv>

#include "meta.h"
#include "array.h"
#include "stack.h"
#include "defines.h"

namespace jot
{
    using String = Slice<const char>;
    using Mutable_String = Slice<char>;
    using String_Builder = Stack<char>;

    func first_index_of(String in_str, String search_for, isize from = 0) -> isize
    {
        if(search_for.size == 0)
            return 0;

        if(in_str.size < search_for.size)
            return -1;

        isize to = min(in_str.size - search_for.size + 1, in_str.size);
        for(isize i = from; i < to; i++)
        {
            let execute = [&](){
                for(isize j = 0; j < search_for.size; j++)
                {
                    if(in_str[i + j] != search_for[j])
                        return false;
                }

                return true;
            };

            if(execute())
                return i;
        };

        return -1;
    }

    constexpr Array<char, 37> LOWERCASE_NUM_CHAR_MAPPING = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
        'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
        'u', 'v', 'w', 'x', 'y', 'z',
        '-'
    };

    constexpr Array<char, 37> UPPERCASE_NUM_CHAR_MAPPING = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
        'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
        'U', 'V', 'W', 'X', 'Y', 'Z',
        '-'
    };

    proc print(std::FILE* stream, String_Builder in builder) -> void
    {
        fwrite(builder.data, sizeof(char), builder.size, stream);
    }

    func format_number(i64 val, isize base = 10, String prefix = "", isize pad_to = 0, Array<char, 37> const& converter = UPPERCASE_NUM_CHAR_MAPPING) -> String_Builder
    {
        String_Builder into;

        assert(2 <= base && base <= 36);

        Array<char, 128> buffer;
        i64 cast_base = cast(i64) base;

        bool is_negative = val < 0;
        isize used_size = 0;

        if(is_negative)
            val = -val;

        i64 last = val;
        while(true)
        {
            //12345
            //[------------]
            //        -12345

            i64 div = last / cast_base;
            i64 last_digit = last - div * cast_base;

            buffer[buffer.size - 1 - used_size] = converter[last_digit];
            used_size ++;

            last = div;
            if(last == 0 && pad_to <= used_size)
                break;
        }

        if(is_negative)
        {
            buffer[buffer.size - 1 - used_size] = '-';
            used_size ++;
        }

        Slice used = {buffer.data + buffer.size - used_size, used_size};

        force(push(&into, prefix));
        force(push(&into, used));

        return into;
    }

    template<std::integral T>
    func format(T in val)
    {
        return format_number(cast(i64) val);
    }

    template<std::floating_point T, typename... Formats>
    func format(T in value, Formats... format_args) -> String_Builder
    {
        String_Builder into;

        constexpr isize base_chars = 64; //64bit num in binary - nothing should be bigger than this 
        constexpr isize chars_grow = 64; //but just in case...
        constexpr isize max_tries = 1;

        isize old_size = into.size;
        isize current_try = 0;

        force(resize(&into, into.size + base_chars));
        mut res = std::to_chars(into.data + old_size, into.data + into.size, value, format_args...);

        for(; res.ec == std::errc::value_too_large && current_try < max_tries; current_try++)
        {
            force(resize(&into, into.size + chars_grow));
            res = std::to_chars(into.data + old_size, into.data + into.size, value, format_args...);
        }

        isize new_size = res.ptr - into.data;
        force(resize(&into, new_size));
        return into;
    }

    func format(String str) -> String_Builder
    {
        String_Builder into;
        force(push(&into, str));
        return into;
    }

    func format(cstring str) -> String_Builder
    {
        return format(String(str));
    }

    func format_multiple(String str, isize count = 1) -> String_Builder
    {
        String_Builder into;

        force(reserve(&into, into.size + str.size * count));
        for(isize i = 0; i < count; i++)
            force(push(&into, str));

        return into;
    }

    func format(String_Builder in builder) -> String_Builder
    {
        return format(slice(builder));
    }

    func format(char in c, isize count = 1) -> String_Builder
    {
        String_Builder into;
        force(resize(&into, count, c));
        return into;
    }

    func format(bool in val) -> String_Builder
    {
        return format(val ? String("true") : String("false"));
    }

    template <typename T>
    concept pointer = std::is_pointer_v<T>;

    template<pointer T>
    func format(T in val) -> String_Builder
    {
        return format_number(cast(isize) val, 16, "0x", 8);
    }

    func format(nullptr_t) -> String_Builder
    {
        return format<void*>(cast(void*) nullptr);
    }

    template <typename T>
    concept formattable = requires(T val)
    {
        { format(val) } -> std::convertible_to<String_Builder>;
    };

    template <typename T>
    concept formattable_forward_range = stdr::forward_range<T> && formattable<stdr::range_value_t<T>>;

    template<formattable_forward_range T>
    func format(T moved range) -> String_Builder
    {
        String_Builder into;
        force(push(&into, '['));

        mut it = stdr::begin(range);
        let end = stdr::end(range);
        if(it != end)
        {
            {
                mut first = format(*it);
                force(push(&into, first));
                ++it;
            }

            for(; it != end; ++it)
            {
                mut formatted = format(*it);
                force(push(&into, String(", ")));
                force(push(&into, formatted));
            }
        }

        force(push(&into, ']'));
        return into;
    }

    
    template <formattable T, isize N>
    func format(const T (&a)[N]) -> String_Builder
    {
        Slice<const T> slice = {a, N};
        return format(slice);
    }
    

    template<formattable T>
    func format_append(String_Builder* to, T in value) -> void {
        mut formatted = format(to);
        force(push(to, value));
    };

    namespace format_string 
    {
        template <typename T>
        proc copy_format_adaptor(void* content) -> String_Builder
        {
            T* casted = cast(T*) content;
            return format(*casted);
        }

        template <typename T, isize N>
        proc array_copy_format_adaptor(void* content) -> String_Builder
        {
            Slice<T> slice = {cast(T*) content, N};
            return format(slice);
        }

        struct Adapted
        {
            using Func = String_Builder(*)(void*);

            void* data;
            Func call;
        };

        template<typename T>
        func make_adapted(T in val) -> Adapted
        {
            return Adapted{
                .data = cast(void*) &val,
                .call = &copy_format_adaptor<T>
            };
        }

        template <class T, isize N>
        func make_adapted(const T (&a)[N]) -> Adapted
        {
            return Adapted{
                .data = cast(void*) a,
                .call = &array_copy_format_adaptor<T, N>
            };
        }

        template<typename... Ts>
        proc format_into(String_Builder* into, String format_str, Slice<Adapted> adapted) -> void
        {
            constexpr String sub_for = "{}";
            isize last = 0;
            isize new_found = 0;
            isize found_count = 0;

            for(;; last = new_found + sub_for.size)
            {
                new_found = first_index_of(format_str, sub_for, last);
                if(new_found == -1)
                {
                    format_append(into, slice(format_str, last));
                    assert(found_count == adapted.size && "number of arguments and holes must match");
                    return;
                }

                if(new_found != 0 && format_str[new_found - 1] == '\\')
                {
                    format_append(into, slice_range(format_str, last, new_found - 1));
                    format_append(into, "{}");
                    continue;
                }


                assert(found_count < adapted.size && "number of arguments and holes must match");

                format_append(into, slice_range(format_str, last, new_found));
                let& curr_adapted = adapted[found_count];
                mut formatted_current = curr_adapted.call(curr_adapted.data);
                force(push(into, formatted_current));

                found_count ++;
            }
        }
    }
    template<formattable... Ts>
    proc format_into(String_Builder* builder, String format_str, Ts in... args) -> void
    {
        format_string::Adapted adapted_storage[] = {format_string::make_adapted(args)...};
        Slice<format_string::Adapted> adapted_slice = {adapted_storage, sizeof...(args)};

        format_string::format_into(builder, format_str, adapted_slice);
    }

    template<formattable... Ts>
    proc format(String format_str, Ts in... args) -> String_Builder
    {
        String_Builder into;

        format_string::Adapted adapted_storage[] = {format_string::make_adapted(args)...};
        Slice<format_string::Adapted> adapted_slice = {adapted_storage, sizeof...(args)};

        format_string::format_into(&into, format_str, adapted_slice);
        return into;
    }

    template <formattable... Ts>
    proc print(std::FILE* stream, Ts in... types) -> void
    {
        String_Builder builder = format(types...);
        fwrite(builder.data, sizeof(char), builder.size, stream);
    }

    template <formattable... Ts>
    proc println(std::FILE* stream, Ts in... types) -> void
    {
        print(stream, types...);
        fputc('\n', stream);
    }

    template <formattable... Ts>
    proc print(Ts in... types) -> void
    {
        print(stdout, types...);
    }

    template <formattable... Ts>
    proc println(Ts in... types) -> void
    {
        println(stdout, types...);
    }
}

#include "undefs.h"