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

    constexpr nodisc 
    isize first_index_of(String in_str, String search_for, isize from = 0)
    {
        if(search_for.size == 0)
            return 0;

        if(in_str.size < search_for.size)
            return -1;

        isize to = min(in_str.size - search_for.size + 1, in_str.size);
        for(isize i = from; i < to; i++)
        {
            const auto execute = [&](){
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

    void print(std::FILE* stream, String_Builder const& builder)
    {
        fwrite(builder.data, sizeof(char), builder.size, stream);
    }

    nodisc
    String_Builder format_number(i64 val, isize base = 10, String prefix = "", isize pad_to = 0, Array<char, 37> const& converter = UPPERCASE_NUM_CHAR_MAPPING)
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
            i64 last_digit = last % cast_base;

            buffer[buffer.size - 1 - used_size] = converter[cast(isize) last_digit];
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

        force(push_multiple(&into, prefix));
        force(push_multiple(&into, used));

        return into;
    }

    template<std::integral T> nodisc 
    String_Builder format(T const& val)
    {
        return format_number(cast(i64) val);
    }

    template<std::floating_point T, typename... Formats> nodisc 
    String_Builder format(T const& value, Formats... format_args)
    {
        String_Builder into;

        constexpr isize base_chars = 64; //64bit num const& binary - nothing should be bigger than this 
        constexpr isize chars_grow = 64; //but just const& case...
        constexpr isize max_tries = 1;

        isize old_size = into.size;
        isize current_try = 0;

        force(resize(&into, into.size + base_chars));
        auto res = std::to_chars(into.data + old_size, into.data + into.size, value, format_args...);

        for(; res.ec == std::errc::value_too_large && current_try < max_tries; current_try++)
        {
            force(resize(&into, into.size + chars_grow));
            res = std::to_chars(into.data + old_size, into.data + into.size, value, format_args...);
        }

        isize new_size = res.ptr - into.data;
        force(resize(&into, new_size));
        return into;
    }

    nodisc 
    String_Builder format(String str)
    {
        String_Builder into;
        force(push_multiple(&into, str));
        return into;
    }

    nodisc 
    String_Builder format(cstring str)
    {
        return format(String(str));
    }

    nodisc
    String_Builder format_multiple(String str, isize count = 1)
    {
        String_Builder into;

        force(reserve(&into, into.size + str.size * count));
        for(isize i = 0; i < count; i++)
            force(push_multiple(&into, str));

        return into;
    }

    nodisc 
    String_Builder format(String_Builder const& builder)
    {
        return format(slice(builder));
    }

    nodisc 
    String_Builder format(char const& c, isize count = 1)
    {
        String_Builder into;
        force(resize(&into, count, c));
        return into;
    }

    nodisc 
    String_Builder format(bool const& val)
    {
        return format(val ? String("true") : String("false"));
    }

    template <typename T>
    concept pointer = std::is_pointer_v<T>;

    template<pointer T> nodisc 
    String_Builder format(T const& val)
    {
        return format_number(cast(isize) val, 16, "0x", 8);
    }

    nodisc 
    String_Builder format(nullptr_t)
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

    template<formattable_forward_range T> nodisc 
    String_Builder format(T && range)
    {
        String_Builder into;
        force(push(&into, '['));

        auto it = stdr::begin(range);
        const auto end = stdr::end(range);
        if(it != end)
        {
            {
                auto first = format(*it);
                force(push(&into, first));
                ++it;
            }

            for(; it != end; ++it)
            {
                auto formatted = format(*it);
                force(push_multiple(&into, String(", ")));
                force(push_multiple(&into, formatted));
            }
        }

        force(push(&into, ']'));
        return into;
    }

    
    template <formattable T, isize N> nodisc 
    String_Builder format(const T (&a)[N])
    {
        Slice<const T> slice = {a, N};
        return format(slice);
    }
    

    template<formattable T>
    void format_append(String_Builder* to, T const& value) 
    {
        String_Builder formatted = format(to);
        force(push_multiple(to, value));
    };

    namespace format_string 
    {
        template <typename T> nodisc
        String_Builder copy_format_adaptor(void* content)
        {
            T* casted = cast(T*) content;
            return format(*casted);
        }

        template <typename T, isize N> nodisc
        String_Builder array_copy_format_adaptor(void* content)
        {
            Slice<T> slice = {cast(T*) content, N};
            return format(slice);
        }

        struct Adapted
        {
            using Func = String_Builder(*)(void*);

            void* data = nullptr;
            Func call = nullptr;
        };

        template<typename T> nodisc
        Adapted make_adapted(T const& val)
        {
            Adapted adapted;
            adapted.data = cast(void*) &val;
            adapted.call = &copy_format_adaptor<T>;
            return adapted;
        }

        template <class T, isize N> nodisc
        Adapted make_adapted(const T (&a)[N])
        {
            Adapted adapted;
            adapted.data = cast(void*) a;
            adapted.call = &array_copy_format_adaptor<T, N>;
            return adapted;
        }

        template<typename... Ts>
        void format_into(String_Builder* into, String format_str, Slice<Adapted> adapted) 
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
                const auto& curr_adapted = adapted[found_count];
                auto formatted_current = curr_adapted.call(curr_adapted.data);
                force(push_multiple(into, formatted_current));

                found_count ++;
            }
        }
    }
    template<formattable... Ts>
    void format_into(String_Builder* builder, String format_str, Ts const&... args)
    {
        format_string::Adapted adapted_storage[] = {format_string::make_adapted(args)...};
        Slice<format_string::Adapted> adapted_slice = {adapted_storage, sizeof...(args)};

        format_string::format_into(builder, format_str, adapted_slice);
    }

    template<formattable... Ts> nodisc
    String_Builder format(String format_str, Ts const&... args)
    {
        String_Builder into;

        format_string::Adapted adapted_storage[] = {format_string::make_adapted(args)...};
        Slice<format_string::Adapted> adapted_slice = {adapted_storage, sizeof...(args)};

        format_string::format_into(&into, format_str, adapted_slice);
        return into;
    }

    template <formattable... Ts>
    void print(std::FILE* stream, Ts const&... types)
    {
        String_Builder builder = format(types...);
        fwrite(builder.data, sizeof(char), builder.size, stream);
    }

    template <formattable... Ts>
    void println(std::FILE* stream, Ts const&... types)
    {
        print(stream, types...);
        fputc('\n', stream);
    }

    template <formattable... Ts>
    void print(Ts const&... types)
    {
        print(stdout, types...);
    }

    template <formattable... Ts>
    void println(Ts const&... types)
    {
        println(stdout, types...);
    }
}

#include "undefs.h"