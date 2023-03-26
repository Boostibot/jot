#pragma once

#include <cstdio>
#include <charconv> //get rid of this!
#include <clocale>

#include "array.h"
#include "traits.h"
#include "string.h"
#include "defines.h"

namespace jot
{
    template <typename T, typename Enable = Enabled>
    struct Formattable : No_Default
    {
        static
        void format(String_Appender* into, T const& value)
        {
            push_multiple(into, "{NOFORMAT}");
        }
    };
    
    template <typename T>
    static constexpr bool is_formattable = !std::is_base_of_v<No_Default, Formattable<T>>;

    template<typename T> nodisc
    void format_into(String_Appender* into, T const& arg)
    {
        if constexpr(std::is_array_v<T>)
        {
            auto slice = Slice{arg, std::extent_v<T>};
            Formattable<decltype(slice)>::format(into, arg);
        }
        else
            Formattable<T>::format(into, arg);
    }

    template<typename T> nodisc
    String_Builder format(T const& arg)
    {
        String_Builder builder;
        String_Appender appender = {&builder};
        format_into(&appender, arg);

        return builder;
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
    void format_number_into(String_Appender* appender, i64 val, isize base = 10, String prefix = "", isize zero_pad_to = 0, Array<char, 37> const& converter = UPPERCASE_NUM_CHAR_MAPPING)
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

        String used = {buffer.data + buffer.size - used_size, used_size};
        push_multiple(appender, prefix);
        push_multiple(appender, used);
    }

    template<typename T> nodisc 
    void format_float_into(String_Appender* appender, T const& value, std::chars_format format = std::chars_format::fixed, int precision = 8)
    {
        constexpr isize chars_grow = 64; //64bit num const& binary - nothing should be bigger than this 
        constexpr isize max_tries = 2; //but just in case
        
        isize size_before = slice(appender).size;

        std::to_chars_result res = {};
        for(isize current_try = 0; current_try < max_tries; current_try++)
        {
            resize(appender, slice(appender).size + chars_grow);
            Mutable_String into_string = tail(slice(appender), size_before);

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
    
    template <typename T> 
    struct Formattable_Range
    {
        static
        void format(String_Appender* appender, T const& range)
        {
            push(appender, '[');

            auto it = std::begin(range);
            const auto end = std::end(range);
            if(it != end)
            {
                format_into(appender, *it);
                ++it;

                for(; it != end; ++it)
                {
                    push_multiple(appender, String(", "));
                    format_into(appender, *it);
                }
            }

            push(appender, ']');
        }
    };

    template <typename T> struct Formattable<T, Enable_If<std::is_integral_v<T>>>
    {
        static
        void format(String_Appender* appender, T const& val)
        {
            return format_number_into(appender, cast(i64) val);
        }
    };

    template <typename T> struct Formattable<T, Enable_If<std::is_floating_point_v<T>>>
    {
        static
        void format(String_Appender* appender, T const& val)
        {
            return format_float_into(appender, val);
        }
    };

    template <typename T> struct Formattable<T, Enable_If<std::is_pointer_v<T>>>
    {
        static
        void format(String_Appender* appender, T const& val)
        {
            return format_number_into(appender, cast(isize) val, 16, "0x", 8);
        }
    };

    template <> struct Formattable<String>
    {
        static
        void format(String_Appender* appender, String str)
        {
            return push_multiple(appender, str);
        }
    };
    
    template <> struct Formattable<Mutable_String>
    {
        static
        void format(String_Appender* appender, Mutable_String str)
        {
            return format_into(appender, cast(String) str);
        }
    };

    template <> struct Formattable<cstring>
    {
        static
        void format(String_Appender* appender, cstring str)
        {
            return format_into(appender, String(str));
        }
    };
    
    template <typename T> struct Formattable<Stack<T>>
    {
        static
        void format(String_Appender* appender, Stack<T> const& stack)
        {
            return format_into(appender, slice(stack));
        }
    };

    template <> struct Formattable<char>
    {
        static
        void format(String_Appender* appender, char c)
        {
            return push(appender, c);
        }
    };
    
    template <> struct Formattable<bool>
    {
        static
        void format(String_Appender* appender, bool val)
        {
            return format_into(appender, val ? "true" : "false");
        }
    };

    template <> struct Formattable<nullptr_t>
    {
        static
        void format(String_Appender* appender, nullptr_t)
        {
            return format_into(appender, cast(void*) nullptr);
        }
    };
    
    template <typename T, isize N> struct Formattable<Array<T, N>>
    {
        static
        void format(String_Appender* appender, Array<T, N> const& arr)
        {
            //regardless of string or not string we always format Arrays as ranges
            Slice<const T> s = slice(arr);
            Formattable_Range<Slice<const T>>::format(appender, s);
        }
    };
    
    template <typename T> 
    struct Formattable<Slice<T>> : Formattable_Range<Slice<T>> {};

    namespace fmt_intern 
    {
        //We try to instantiate as little templates as possible
        // as such we first convert all arguments of generic format(String format...) builder
        // "Adapted" that know how to format the particular type. Then format simply calls
        // the adapted function and uses the result. This results in us only isnatntiating
        // one template for each argument type and not for each combination of argument types
        struct Adaptor
        {
            using Formatting_Fn = void(*)(String_Appender*, Adaptor);
            
            void* data = nullptr;
            Formatting_Fn function = nullptr;
            i32 array_size = 0;
            bool is_string = false;

            Adaptor() noexcept = default;

            template<typename T>
            Adaptor(T const& val) noexcept
            {
                data = cast(void*) &val;
                function = &format_adaptor<T>;
            }
            
            template <class T, i32 N> nodisc
            Adaptor(const T (&a)[N]) noexcept
            {
                data = cast(void*) a;
                function = &array_format_adaptor<T>;
                
                assert(N != 0 && "size must not be 0 shouldnt even be possible");
                bool is_null_terminated = a[N - 1] == 0;

                is_string = is_string_char<T> && is_null_terminated;
                array_size = N - cast(i32) is_string; //if is string we dont include the null termination char
            }

            template <typename T> nodisc
            static void array_format_adaptor(String_Appender* appender, Adaptor adaptor)
            {
                Slice<T> slice = {cast(T*) adaptor.data, adaptor.array_size};
                return format_into(appender, slice);
            }

            template <typename T> nodisc
            static void format_adaptor(String_Appender* appender, Adaptor adaptor)
            {
                T* casted = cast(T*) adaptor.data;
                return format_into(appender, *casted);
            }
        };
        
        static void concat_adapted_into(String_Appender* appender, Slice<Adaptor> adapted)
        {
            for(Adaptor const& curr_adapted : adapted)
            {
                if(curr_adapted.data == nullptr)
                    break;

                curr_adapted.function(appender, curr_adapted);
            }
        }
        
        static void format_adapted_into(String_Appender* appender, String format_str, Slice<Adaptor> adapted)
        {
            constexpr String sub_for = "{}";

            isize last = 0;
            isize new_found = 0;
            isize found_count = 0;

            for(found_count;; last = new_found + sub_for.size, found_count++)
            {
                new_found = first_index_of(format_str, sub_for, last);
                if(new_found == -1)
                    break;

                if(new_found != 0 && new_found > 0 && new_found + 2 < format_str.size)
                {
                    if (format_str[new_found - 1] == '{' && format_str[new_found + 2] == '}')
                    {
                        String in_between = slice_range(format_str, last, new_found - 1);
                        push_multiple(appender, in_between);
                        push_multiple(appender, sub_for);
                        
                        new_found += 1;
                        continue;
                    }
                }

                String in_between = slice_range(format_str, last, new_found);
                push_multiple(appender, in_between);

                if(found_count < adapted.size)
                {
                    Adaptor const& curr_adapted = adapted[found_count];
                    if(curr_adapted.data != nullptr)
                        curr_adapted.function(appender, curr_adapted);
                }
            }
            
            String till_end = tail(format_str, last);
            push_multiple(appender, till_end);
            concat_adapted_into(appender, tail(adapted, found_count));
        }

        NO_INLINE
        static void format_adapted_into(String_Appender* appender, Slice<Adaptor> adapted)
        {
            if(adapted.size == 0)
                return;

            if(adapted[0].is_string)
            {
                Slice<Adaptor> rest = tail(adapted);
                String format = {cast(cstring) adapted[0].data, adapted[0].array_size};
                format_adapted_into(appender, format, rest);
            }
            else
            {
                concat_adapted_into(appender, adapted);
            }
        }
    }
    
    inline void format_into(String_Appender* appender, String format_str)
    {
        return push_multiple(appender, format_str);
    }
    
    nodisc inline 
    String_Builder format(String str)
    {
        String_Builder builder;
        push_multiple(&builder, str);
        return builder;
    }

    //We dont use variadics here even though this is exactly what they were intendend for.
    //The problem with variadics is that for each combination of types we get template instantiation.
    //When using fmt functions this adds up QUICK so having instead a single templated constructor in any is much better idea 
    #define ADAPTOR_10_ARGS_DECL \
        fmt_intern::Adaptor const& a1     , fmt_intern::Adaptor const& a2 = {}, fmt_intern::Adaptor const& a3 = {}, fmt_intern::Adaptor const& a4 = {}, fmt_intern::Adaptor const& a5 = {}, \
        fmt_intern::Adaptor const& a6 = {}, fmt_intern::Adaptor const& a7 = {}, fmt_intern::Adaptor const& a8 = {}, fmt_intern::Adaptor const& a9 = {}, fmt_intern::Adaptor const& a10 = {} \
        
    #define ADAPTOR_10_ARGS a1, a2, a3, a4, a5, a6, a7, a8, a9, a10

    inline void format_into(String_Appender* appender, ADAPTOR_10_ARGS_DECL)
    {
        using namespace fmt_intern;
        
        Adaptor adapted_storage[] = {ADAPTOR_10_ARGS};
        Slice<Adaptor> adapted_slice = {adapted_storage, 10};

        return format_adapted_into(appender, adapted_slice);
    }
    
    inline void format_into(String_Builder* into, ADAPTOR_10_ARGS_DECL) noexcept
    {
        String_Appender appender(into);
        return format_into(&appender, ADAPTOR_10_ARGS);
    }

    inline nodisc
    String_Builder format(ADAPTOR_10_ARGS_DECL)
    {
        using namespace fmt_intern;
        String_Builder builder;
        String_Appender appender = {&builder};
        format_into(&appender, ADAPTOR_10_ARGS);
        return builder;
    }
    
    inline void print_into(std::FILE* stream, String str)
    {
        fwrite(str.data, sizeof(char), cast(size_t) str.size, stream);
    }
    
    inline void println_into(std::FILE* stream, String str)
    {
        print_into(stream, str);
        fputc('\n', stream);
    }

    inline void print_into(std::FILE* stream, ADAPTOR_10_ARGS_DECL)
    {
        String_Builder builder = format(ADAPTOR_10_ARGS);
        print_into(stream, slice(builder));
    }
    
    inline void println_into(std::FILE* stream, ADAPTOR_10_ARGS_DECL)
    {
        String_Builder builder = format(ADAPTOR_10_ARGS);
        push(&builder, '\n');
        print_into(stream, slice(builder));
    }

    inline void print(ADAPTOR_10_ARGS_DECL)
    {
        print_into(stdout, ADAPTOR_10_ARGS);
    }
    
    inline void println(ADAPTOR_10_ARGS_DECL)
    {
        println_into(stdout, ADAPTOR_10_ARGS);
    }

    //more specific overloads so that we dont waste time 
    inline void print(String str) noexcept
    {
        print_into(stdout, str);
    }

    inline void println(String str) noexcept
    {
        println_into(stdout, str);
    }

    inline void print(Mutable_String str) noexcept
    {
        print_into(stdout, cast(String) str);
    }

    inline void println(Mutable_String str) noexcept
    {
        println_into(stdout, cast(String) str);
    }

    inline void print(cstring str) noexcept
    {
        print_into(stdout, String(str));
    }

    inline void println(cstring str) noexcept
    {
        println_into(stdout, String(str));
    }

    inline void println() noexcept
    {
        fputc('\n', stdout);
    }
    
    #ifndef SET_UTF8_LOCALE
    #define SET_UTF8_LOCALE
    inline bool set_utf8_locale(bool english = false)
    {
        if(english)
            return setlocale(LC_ALL, "en_US.UTF-8") != NULL;
        else
            return setlocale(LC_ALL, ".UTF-8") != NULL;
    }

    namespace {
        const static bool _locale_setter = set_utf8_locale(true);
    }
    #endif // !SET_UTF8_LOCALE
}

#include "undefs.h"