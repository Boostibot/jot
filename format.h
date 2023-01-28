#pragma once

#include <cstdio>
#include <charconv> //get rid of this!

#include "array.h"
#include "traits.h"
#include "string.h"
#include "defines.h"

namespace jot
{
    template <typename T, typename Enable = True>
    struct Formattable : No_Default
    {
        nodisc static
        State format(String_Appender* into, T const& value) noexcept
        {
            //just for illustartion
            return OK_STATE;
        }
    };
    
    template <typename T>
    static constexpr bool is_formattable = !std::is_base_of_v<No_Default, Formattable<T>>;

    struct Format_Result
    {
        String_Builder builder;
        State state = OK_STATE;
    };

    void print_into(std::FILE* stream, String str)
    {
        fwrite(str.data, sizeof(char), cast(size_t) str.size, stream);
    }
    
    void println_into(std::FILE* stream, String str)
    {
        print_into(stream, str);
        fputc('\n', stream);
    }

    void print(String str)
    {
        print_into(stdout, str);
    }

    void println(String str)
    {
        println_into(stdout, str);
    }

    template<typename T> nodisc
    State format_into(String_Appender* into, T const& arg) noexcept
    {
        if constexpr(std::is_array_v<T>)
        {
            auto slice = Slice{arg, std::extent_v<T>};
            return Formattable<decltype(slice)>::format(into, arg);
        }
        else
            return Formattable<T>::format(into, arg);
    }

    template<typename T> nodisc
    Format_Result format(T const& arg) noexcept
    {
        Format_Result result;
        String_Appender appender = {&result.builder};
        result.state = format_into(&appender, arg);

        return result;
    }
    

    constexpr Array<char, 37> LOWERCASE_NUM_CHAR_MAPPING = {{
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
        'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
        'u', 'v', 'w', 'x', 'y', 'z',
        '-'
    }};

    constexpr Array<char, 37> UPPERCASE_NUM_CHAR_MAPPING = {{
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
        'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
        'U', 'V', 'W', 'X', 'Y', 'Z',
        '-'
    }};

    nodisc
    State format_number_into(String_Appender* appender, i64 val, isize base = 10, String prefix = "", isize zero_pad_to = 0, Array<char, 37> const& converter = UPPERCASE_NUM_CHAR_MAPPING)
    {
        assert(2 <= base && base <= 36);

        Array<char, 128> buffer = {{0}};
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
            if(last == 0 && zero_pad_to <= used_size)
                break;
        }

        if(is_negative)
        {
            buffer[buffer.size - 1 - used_size] = '-';
            used_size ++;
        }

        Slice used = {buffer.data + buffer.size - used_size, used_size};
        State state = push_multiple(appender, prefix);
        acumulate(&state, push_multiple(appender, used));

        return state;
    }

    template<typename T> nodisc 
    State format_float_into(String_Appender* appender, T const& value, std::chars_format format = std::chars_format::fixed, int precision = 8)
    {
        constexpr isize chars_grow = 64; //64bit num const& binary - nothing should be bigger than this 
        constexpr isize max_tries = 2; //but just in case
        
        isize size_before = slice(appender).size;

        std::to_chars_result res = {};
        State state = OK_STATE;
        for(isize current_try = 0; current_try < max_tries; current_try++)
        {
            state = resize(appender, slice(appender).size + chars_grow);
            if(state == ERROR)
                return state;

            Mutable_String into_string = slice(slice(appender), size_before);

            if(precision == -1)
                res = std::to_chars(into_string.data, into_string.data + into_string.size, value, format);
            else
                res = std::to_chars(into_string.data, into_string.data + into_string.size, value, format, precision);

            if(res.ec == std::errc())
                break;
        }
        
        assert(res.ec == std::errc() && "shouldnt fail!");
        isize new_size = res.ptr - slice(appender).data;
        return resize(appender, new_size);
    }
    
    
    struct Not_Integral {};
    struct Not_Floating_Point {};
    struct Not_Pointer {};
    struct Not_Formattable_Range {};
    struct Not_Formattable_Array {};

    template <typename T> struct Formattable<T, Enable_If<std::is_integral_v<T>, Not_Integral>>
    {
        nodisc static
        State format(String_Appender* appender, T const& val) noexcept
        {
            return format_number_into(appender, cast(i64) val);
        }
    };

    template <typename T> struct Formattable<T, Enable_If<std::is_floating_point_v<T>, Not_Floating_Point>>
    {
        nodisc static
        State format(String_Appender* appender, T const& val) noexcept
        {
            return format_float_into(appender, val);
        }
    };

    template <typename T> struct Formattable<T, Enable_If<std::is_pointer_v<T>, Not_Pointer>>
    {
        nodisc static
        State format(String_Appender* appender, T const& val) noexcept
        {
            return format_number_into(appender, cast(isize) val, 16, "0x", 8);
        }
    };

    template <> struct Formattable<String>
    {
        nodisc static
        State format(String_Appender* appender, String str) noexcept
        {
            return push_multiple(appender, str);
        }
    };
    
    template <> struct Formattable<Mutable_String>
    {
        nodisc static
        State format(String_Appender* appender, Mutable_String str) noexcept
        {
            return format_into(appender, cast(String) str);
        }
    };

    template <> struct Formattable<cstring>
    {
        nodisc static
        State format(String_Appender* appender, cstring str) noexcept
        {
            return format_into(appender, String(str));
        }
    };
    
    template <> struct Formattable<String_Builder>
    {
        nodisc static
        State format(String_Appender* appender, String_Builder const& builder_) noexcept
        {
            return format_into(appender, slice(builder_));
        }
    };

    template <> struct Formattable<char>
    {
        nodisc static
        State format(String_Appender* appender, char c) noexcept
        {
            return push(appender, c);
        }
    };
    
    template <> struct Formattable<bool>
    {
        nodisc static
        State format(String_Appender* appender, bool val) noexcept
        {
            return format_into(appender, val ? "true" : "false");
        }
    };

    template <> struct Formattable<nullptr_t>
    {
        nodisc static
        State format(String_Appender* appender, nullptr_t) noexcept
        {
            return format_into(appender, cast(void*) nullptr);
        }
    };
    
    template <typename T> 
    struct Formattable_Range
    {
        nodisc static
        State format(String_Appender* appender, T const& range) noexcept
        {
            State state = push(appender, '[');

            auto it = std::begin(range);
            const auto end = std::end(range);
            if(it != end)
            {
                acumulate(&state, format_into(appender, *it));
                ++it;

                for(; it != end; ++it)
                {
                    acumulate(&state, push_multiple(appender, String(", ")));
                    acumulate(&state, format_into(appender, *it));
                }
            }

            acumulate(&state, push(appender, ']'));
            return state;
        }
    };

    
    template <typename T> 
    struct Formattable<Slice<T>> : Formattable_Range<Slice<T>> {};

    open_enum Format_State
    {
        OPEN_ENUM_DECLARE("jot::Format_State");
        OPEN_ENUM_ENTRY(FORMAT_ARGUMENTS_DONT_MATCH);
        OPEN_ENUM_ENTRY(INVALID_FORMAT_SYNTAX);
    }

    namespace format_type_erasure 
    {
        //We try to instantiate as little templates as possible
        // as such we first convert all arguments of generic format(String format...) builder
        // "Adapted" that know how to format the particular type. Then format simply calls
        // the adapted function and uses the result. This results in us only isnatntiating
        // one template for each argument type and not for each combination of argument types
        struct Format_Adaptor
        {
            using Formatting_Fn = State(*)(String_Appender*, Format_Adaptor);

            void* data = nullptr;
            Formatting_Fn function = nullptr;
            isize array_size = -1;
        };

        template <typename T> nodisc
        State array_format_adaptor(String_Appender* appender, Format_Adaptor adaptor) noexcept
        {
            Slice<T> slice = {cast(T*) adaptor.data, adaptor.array_size};
            return format_into(appender, slice);
        }

        template <typename T> nodisc
        State format_adaptor(String_Appender* appender, Format_Adaptor adaptor) noexcept
        {
            T* casted = cast(T*) adaptor.data;
            return format_into(appender, *casted);
        }

        template<typename T> nodisc
        Format_Adaptor make_adapted(T const& val) noexcept
        {
            Format_Adaptor adapted;
            adapted.data = cast(void*) &val;
            adapted.function = &format_adaptor<T>;
            adapted.array_size = 0;
            return adapted;
        }

        template <class T, isize N> nodisc
        Format_Adaptor make_adapted(const T (&a)[N]) noexcept
        {
            Format_Adaptor adapted;
            adapted.data = cast(void*) a;
            adapted.function = &array_format_adaptor<T>;
            adapted.array_size = N;
            return adapted;
        }

        NO_INLINE
        State format_adapted_into(String_Appender* appender, String format_str, Slice<Format_Adaptor> adapted) noexcept
        {
            constexpr String sub_for = "{}";

            isize last = 0;
            isize new_found = 0;
            isize found_count = 0;

            State state = OK_STATE;
            for(;; last = new_found + sub_for.size)
            {
                new_found = first_index_of(format_str, sub_for, last);
                if(new_found == -1)
                {
                    String till_end = slice(format_str, last);
                    acumulate(&state, push_multiple(appender, till_end));

                    if(found_count != adapted.size)
                    {
                        assert(false && "number of arguments and holes must match");
                        acumulate(&state, Format_State::FORMAT_ARGUMENTS_DONT_MATCH);
                    }

                    break;
                }

                if(new_found != 0 && format_str[new_found - 1] == '\\')
                {
                    String in_between = slice_range(format_str, last, new_found - 1);
                    acumulate(&state, push_multiple(appender, in_between));
                    acumulate(&state, push_multiple(appender, sub_for));

                    continue;
                }

                assert(found_count < adapted.size && "number of arguments and holes must match");

                String in_between = slice_range(format_str, last, new_found);
                acumulate(&state, push_multiple(appender, in_between));

                Format_Adaptor const& curr_adapted = adapted[found_count];
                acumulate(&state, curr_adapted.function(appender, curr_adapted));

                found_count ++;
            }

            return state;
        }
    }
    
    nodisc
    State format_into(String_Appender* appender, String format_str)
    {
        return push_multiple(appender, format_str);
    }
    
    nodisc
    Format_Result format(String format_str)
    {
        Format_Result result;
        result.state = push_multiple(&result.builder, format_str);
        return result;
    }

    template<typename... Ts> nodisc
    State format_into(String_Appender* appender, String format_str, Ts const&... args)
    {
        using namespace format_type_erasure;

        Format_Adaptor adapted_storage[] = {make_adapted(args)...};
        Slice<Format_Adaptor> adapted_slice = {adapted_storage, sizeof...(args)};

        return format_adapted_into(appender, format_str, adapted_slice);
    }
    
    template<typename... Ts> nodisc
    Format_Result format(String format_str, Ts const&... args)
    {
        using namespace format_type_erasure;
        Format_Result result;
        String_Appender appender = {&result.builder};

        Format_Adaptor adapted_storage[] = {make_adapted(args)...};
        Slice<Format_Adaptor> adapted_slice = {adapted_storage, sizeof...(args)};

        result.state = format_adapted_into(&appender, format_str, adapted_slice);
        return result;
    }
    
    //@NOTE: Code duplication here since we want to ensure fastest possible 
    // compilation times and as such we imit the call stack depth of functions with
    // variadic arguments to two
    template<typename... Ts> 
    State print_into(std::FILE* stream, Ts const&... types)
    {
        Format_Result result = format(types...);
        print_into(stream, slice(result.builder));
        return result.state;
    }
    
    template<typename... Ts> 
    State println_into(std::FILE* stream, Ts const&... types)
    {
        Format_Result result = format(types...);
        println_into(stream, slice(result.builder));
        return result.state;
    }
    
    template<typename... Ts>
    State print(Ts const&... types)
    {
        Format_Result result = format(types...);
        print(slice(result.builder));
        return result.state;
    }

    template <typename... Ts>
    State println(Ts const&... types)
    {
        Format_Result result = format(types...);
        println(slice(result.builder));
        return result.state;
    }
}

#include "undefs.h"