#pragma once

#include <cstdio>
#include <clocale>
#include <stdarg.h>

#include "string.h"
#include "defines.h"

namespace jot
{
    ///A set of extendible formatting functions that dont need the c type specification in format string.
    #ifdef _FMT_EXAMPLE
    void format_example()
    {
        int size = 20; int i = 12;
        println("hello world! size: {} i: {}", size, i);    //hello world! size: 20 i: 12
        println("size: ", size, " i: ", i, " ...");         //size: 20 i: 12 ...
        println("only one {} but two args! ", size, i);     //only one 20 but two args! 12
        println("two '{}' '{}' but only one arg!", i);      //two '12' '' but only one arg!

        String_Builder formatted = format("hello world! escape{{}}"); 
        println(formatted); //hello world! escape{}

        format_into(&formatted, " append to end {}! ", size, fmt_as_padded(i, 4));
        println(formatted); //hello world! escape{} append to end 20! 0012

        int vals[] = {1, 2, 3, 4};
        println(vals); //[1, 2, 3, 4]
    }
    #endif
    
    ///Since we need to know the type of argument passed in we cannot use c va_args. Thus we need to use
    ///templates in some extent. However we do not variadics even though this is exactly what they were intendend for.
    ///The problem with variadics is that for each combination of types we get template instantiation.
    ///When using fmt functions this adds up QUICK. As such we have one struct Format_Adaptor holding type 
    ///errased formating info that is used to interface with all formating functions
    
    ///The rules for formatting are as follows:
    /// (a) If first type is not String or const char*: 
    ///      Each of the arguments is formatted individually and the results are concatenated
    ///      into the format string
    /// (b) If first type is String or const char* it is interpreted as format string:
    ///      The format is: "hello world! size: {} i: {} escape {{}}" where a formated 
    ///      representation of the subsequent arguments is placed into each of the "{}" slot.
    ///      A single argument is placed into a single slot in order. If there are more arguments
    ///      than slots the remaining arguments get concatenated to the end. If there are more slots
    ///      than arguments the slots get removed from the string (places empty in each slot).
    /// 
    /// To escape a slot surround it with additional braces (format "{{}}" will get escaped to "{}")
    /// 
    /// There are no modifiers that can appear in the substitution 
    /// (for example c has modifier %.5f instead of the standard %f to set padding). 
    /// All modifiers are done by changing the type of the argument or formatting 
    /// outside the function and passing in a string. This is often much more readable and allows
    /// us to easily change the format programatically.

    ///Struct holding type erased representation of a format argument
    ///Has a templated constructor for all types that saves a pointer to the specific format function.
    struct Format_Adaptor
    {
        using Format = void(*)(String_Appender*, Format_Adaptor);
        using Format_String = String(*)(Format_Adaptor);
            
        Format _format = nullptr;
        Format_String _format_string = nullptr;
        void* _data = nullptr;

        Format_Adaptor() noexcept {};

        template<typename T>
        Format_Adaptor(T const& val) noexcept;
        
        //special case for string literals so that we dont instantiate any templates
        Format_Adaptor(const char* val) noexcept;
    };

    //Since we need to enforce that all types are type erased through Format_Adaptor we cannot use variadics. 
    // We allow at max 10 arguments if you need more just call the function twice or define your own version taking more.
    #define FORMAT_ADAPTOR_10_ARGS_DECL \
        Format_Adaptor const& a1      , Format_Adaptor const& a2 = {}, Format_Adaptor const& a3 = {}, \
        Format_Adaptor const& a4  = {}, Format_Adaptor const& a5 = {}, Format_Adaptor const& a6 = {}, \
        Format_Adaptor const& a7  = {}, Format_Adaptor const& a8 = {}, Format_Adaptor const& a9 = {}, \
        Format_Adaptor const& a10 = {} \

    ///Formats the provided arguments and returns String_Builder witht the result
    NO_INLINE nodisc static String_Builder format(FORMAT_ADAPTOR_10_ARGS_DECL);
    ///Formats the provided arguments and appends the result to String_Appender
    NO_INLINE static void format_into(String_Appender* into, FORMAT_ADAPTOR_10_ARGS_DECL);
    ///Formats the provided arguments and appends the result to String_Builder
    NO_INLINE static void format_into(String_Builder* into,  FORMAT_ADAPTOR_10_ARGS_DECL);

    ///Formats the provided arguments and appends the result to stream
    NO_INLINE static void print_into(FILE* stream, FORMAT_ADAPTOR_10_ARGS_DECL);
    ///Formats the provided arguments and appends the result to stream appending '\n' at the end
    NO_INLINE static void println_into(FILE* stream, FORMAT_ADAPTOR_10_ARGS_DECL);

    ///Formats the provided arguments and appends the result to stdout
    inline void print(FORMAT_ADAPTOR_10_ARGS_DECL);
    ///Formats the provided arguments and appends the result to stdout appending '\n' at the end
    inline void println(FORMAT_ADAPTOR_10_ARGS_DECL);
    ///Prints a '\n' to stdout
    inline void println() noexcept;
    
    ///Formats the adaptors in adapted slice and appends the result to String_Appender
    static void format_adapted_into(String_Appender* appender, String format_str, Slice<Format_Adaptor> adapted);
    ///Formats the adaptors in adapted slice and appends the result to String_Appender
    static void format_adapted_into(String_Appender* appender, Slice<Format_Adaptor> adapted);
    
    ///Formats the provided arguments according to standard c format
    ///and returns String_Builder witht the result
    NO_INLINE nodisc static String_Builder cformat(const char* cformat, ...);
    ///Uses cformat to format varargs and appends the result string into 'into'
    NO_INLINE static void cformat_into(String_Appender* into, const char* cformat, ...);
    ///Uses cformat to format varargs and appends the result string into 'into'
    static void vcformat_into(String_Appender* appender, const char* format, va_list args);
    
    ///If you wish to extend the functionality of format to include custom
    ///types we do so by specializing this template and providing implementation
    ///for the format function
    template <typename T, typename Enable = void>
    struct Formattable
    {
        static void format(String_Appender* into, T const& value)
        {
            static_assert(sizeof(T) == 0, "This type does not have a format! (yet)");
            cast(void) into; cast(void) value;
        }
    };

    struct Format_Num_Info
    {
        int  pad_to = 0;
        char pad_with = '0';
        char positive_marker = 0; //0 means nothing will be inserted
        char negative_marker = '-';
        char digits[37] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        bool count_markers_towards_pad = true; //if - and + will contribute towards pad_to
        bool is_unsigned = false; //if the signed 64bit number should be interpreted as unsigned
    };

    ///Formats the number according to custom rules and appends it into 'into'
    static void format_number_into(String_Appender* into, int64_t num, uint8_t base = 10, Format_Num_Info const& info = {});
    
    ///Formats any valid c++ range into format [elem1, elem2, elem3] and append it into 'into'
    template<typename T> static
    void format_range_into(String_Appender* into, T const& range);
    
    ///Format modifier struct for integers 
    struct Padded_Int_Format
    {
        int64_t val;
        int pad_to;
        char pad_with = '0';
    };
    
    ///Format modifier struct for floats 
    struct Padded_Float_Format
    {
        double val;
        isize pad_total_size_to;
        isize pad_fraction_to = 0;
        char pad_with = '0';
    };

    inline Padded_Int_Format fmt_as_padded(int64_t val, int pad_to, char pad_with = '0')
    {
        return Padded_Int_Format{val, pad_to, pad_with};
    }
    
    inline Padded_Float_Format fmt_as_padded(double val, int pad_total_size_to, int pad_fraction_to, char pad_with = '0')
    {
        return Padded_Float_Format{val, pad_total_size_to, pad_fraction_to, pad_with};
    }
}

namespace jot
{
    static void format_number_into(String_Appender* appender, int64_t num, uint8_t base, Format_Num_Info const& info)
    {
        assert(base <= 36 && base >= 2);
        char buffer[64] = {0};
        int pad_to = info.pad_to;

        char marker = '\0';
        if(num < 0 && info.is_unsigned == false)
        {
            num *= -1;
            marker = info.negative_marker;
            if(info.count_markers_towards_pad && marker != 0)
                pad_to--;
        }
        else
        {
            marker = info.positive_marker;
            if(info.count_markers_towards_pad && marker != 0)
                pad_to--;
        }

        int used_size = 0;
        uint64_t last = cast(uint64_t) num;
        while(true)
        {
            uint64_t div = last / base;
            uint64_t last_digit = last % base;

            buffer[64 - 1 - used_size] = info.digits[last_digit];
            used_size ++;

            last = div;
            if(last == 0)
                break;
        }

        int max_size = used_size > pad_to ? used_size : pad_to;
        grow(appender, max_size + 1);

        if(marker != 0)
            push(appender, marker);

        int prepend_count = pad_to - used_size;
        if(prepend_count > 0)
            resize(appender, size(appender) + prepend_count, info.pad_with);

        String used = {buffer + 64 - used_size, used_size};
        push_multiple(appender, used);
    }
    
    //va args cannot be inlined and if we use our own va args wrapper this results in
    // at least 2 function calls and 3 va args unpackings. We need to reduce this down
    // as mch as we can because we use it as default printing of doubles.
    template<typename T>
    static void cformat_into_single(String_Appender* appender, const char* format, T val)
    {
        isize count = snprintf(data(appender), size(appender), format, val);
        resize(appender, count);
        if(count < estimated_size)
            return;

        count = snprintf(data(appender), size(appender), format, val);
    }

    static void vcformat_into(String_Appender* appender, const char* format, va_list args)
    {
        //an attempt to estimate the needed size so we dont need to call vsnprintf twice
        isize format_size = strlen(format);
        isize estimated_size = format_size + 10 + format_size/4;
        resize(appender, estimated_size);

        va_list args_copy;
        va_copy(args_copy, args);
        isize count = vsnprintf(data(appender), size(appender), format, args);
        
        resize(appender, count);
        if(count < estimated_size)
            return;

        count = vsnprintf(data(appender), size(appender), format, args_copy);
    }

    NO_INLINE 
    static void cformat_into(String_Appender* appender, const char* format, ...)
    {
        va_list args;
        va_start(args, format);
        vcformat_into(appender, format, args);
        va_end(args);
    }
    
    NO_INLINE nodisc 
    static String_Builder cformat(const char* format, ...)
    {
        String_Builder builder;
        String_Appender appender(&builder);

        va_list args;
        va_start(args, format);
        vcformat_into(&appender, format, args);
        va_end(args);

        return builder;
    }

    template<typename T> static
    void format_into_single_infer(String_Appender* appender, T const& val)
    {
        Formattable<T>::format(appender, val);
    }

    template<typename T> static
    void format_range_into(String_Appender* appender, T const& range)
    {
        push(appender, '[');
        auto it = range.begin();
        const auto end = range.end();
        if(it != end)
        {
            format_into_single_infer(appender, *it);
            ++it;
        }

        for(; it != end; ++it)
        {
            push_multiple(appender, String(", "));
            format_into_single_infer(appender, *it);
        }

        push(appender, ']');
    }

    #define FORMATTABLE_INT_TYPE(TYPE)                              \
        template <> struct Formattable<TYPE>                        \
        {                                                           \
            static void format(String_Appender* into, TYPE val)     \
            {                                                       \
                return format_number_into(into, cast(int64_t) val); \
            }                                                       \
        }                                                           \
    
    #define FORMATTABLE_UINT_TYPE(TYPE)                             \
        template <> struct Formattable<TYPE>                        \
        {                                                           \
            static void format(String_Appender* into, TYPE val)     \
            {                                                       \
                Format_Num_Info info;                               \
                info.is_unsigned = true;                            \
                return format_number_into(into, val, 10, info);     \
            }                                                       \
        }                                                           \

    FORMATTABLE_INT_TYPE(signed char);
    FORMATTABLE_INT_TYPE(short);
    FORMATTABLE_INT_TYPE(int);
    FORMATTABLE_INT_TYPE(long);
    FORMATTABLE_INT_TYPE(long long);

    FORMATTABLE_UINT_TYPE(unsigned char);
    FORMATTABLE_UINT_TYPE(unsigned short);
    FORMATTABLE_UINT_TYPE(unsigned int);
    FORMATTABLE_UINT_TYPE(unsigned long);
    FORMATTABLE_UINT_TYPE(unsigned long long);

    template <> struct Formattable<float>
    {
        static void format(String_Appender* into, float val)
        {
            cformat_into_single(into, "%f", val);
        }
    };
    
    template <> struct Formattable<double>
    {
        static void format(String_Appender* into, double val)
        {
            cformat_into_single(into, "%lf", val);
        }
    };
    
    template <> struct Formattable<long double>
    {
        static void format(String_Appender* into, long double val)
        {
            cformat_into_single(into, "%llf", val);
        }
    };
    
    template <> struct Formattable<Padded_Int_Format>
    {
        static void format(String_Appender* into, Padded_Int_Format const& padded)
        {
            Format_Num_Info info;
            info.pad_to = padded.pad_to;
            info.pad_with = padded.pad_with;
            format_number_into(into, padded.val, 10, info);
        }
    };
    
    template <> struct Formattable<Padded_Float_Format>
    {
        static void format(String_Appender* into, Padded_Float_Format const& padded)
        {
            assert(false && "TODO"); 
            cast(void) into; cast(void) padded;
        }
    };

    template <typename T> struct Formattable<T*>
    {
        static void format(String_Appender* into, T* val)
        {
            Format_Num_Info info;
            info.pad_to = 8;
            info.is_unsigned = true;
            push_multiple(into, "0x");
            format_number_into(into, cast(int64_t) val, 16, info);
        }
    };
    
    template <> struct Formattable<nullptr_t>
    {
        static void format(String_Appender* into, nullptr_t)
        {
            return Formattable<void*>::format(into, cast(void*) nullptr);
        }
    };
    
    template <> struct Formattable<bool>
    {
        static void format(String_Appender* into, bool val)
        {
            push_multiple(into, val ? "true" : "false");
        }
    };
    
    //Inheriting tells the format function that this type acts as format string. 
    //All types inheriting this must have a format_string(T const&) -> String function
    struct Is_String_Format {};

    template <> struct Formattable<String> : Is_String_Format
    {
        static void format(String_Appender* into, String str) { push_multiple(into, str);}
        static String format_string(String str) noexcept      { return str;}
    };
    
    template <> struct Formattable<Mutable_String> : Is_String_Format
    {
        static void format(String_Appender* into, Mutable_String str) { push_multiple(into, str);}
        static String format_string(Mutable_String str) noexcept      { return str;}
    };
    
    template <> struct Formattable<const char*> : Is_String_Format
    {
        static void format(String_Appender* into, const char* str) { push_multiple(into, str);}
        static String format_string(const char* str) noexcept      { return str;}
    };

    template <isize N> struct Formattable<char [N]> : Is_String_Format
    {
        static
        void format(String_Appender* into, const char (&arr)[N])
        {
            String str = {arr, N};
            Formattable<String>::format(into, str);
        }

        static String format_string(const char* str) noexcept      { return str;}
    };

    template <> struct Formattable<char>
    {
        static void format(String_Appender* into, char c)
        {
            push(into, c);
        }
    };
    
    template <typename T> struct Formattable<Slice<T>>
    {
        static void format(String_Appender* into, Slice<T> slice)
        {
            format_range_into(into, slice);
        }
    };

    template <typename T> struct Formattable<Stack<T>>
    {
        static void format(String_Appender* into, Stack<T> const& stack)
        {
            Slice<const T> s = slice(stack);
            Formattable<Slice<const T>>::format(into, s);
        }
    };
    
    template <typename T, isize N>
    struct Formattable<T[N]>
    {
        static
        void format(String_Appender* into, const T (&arr)[N])
        {
            Slice<const T> slice = {arr, N};
            Formattable<Slice<const T>>::format(into, slice);
        }
    };

    namespace format_internal
    {
        template <typename T>
        static void format_adaptor(String_Appender* into, Format_Adaptor adaptor)
        {
            T* casted = cast(T*) adaptor._data;
            Formattable<T>::format(into, *casted);
        }
        
        template <typename T>
        static String format_string_adaptor(Format_Adaptor adaptor)
        {
            T* casted = cast(T*) adaptor._data;
            String format_str = Formattable<T>::format_string(*casted);
            return format_str;
        }

        static void cstring_format_adaptor(String_Appender* into, Format_Adaptor adaptor)
        {
            String str = cast(const char*) adaptor._data;
            Formattable<String>::format(into, str);
        }
        
        static String cstring_format_string_adaptor(Format_Adaptor adaptor)
        {
            String str = cast(const char*) adaptor._data;
            return str;
        }
    }
    
    template<typename T>
    Format_Adaptor::Format_Adaptor(T const& val) noexcept
    {
        _data = cast(void*) &val;
        _format = format_internal::format_adaptor<T>;
        constexpr bool is_fmt_string = __is_base_of(Is_String_Format, Formattable<T>);
        if constexpr(is_fmt_string)
            _format_string = format_internal::format_string_adaptor<T>;
    }
        
    Format_Adaptor::Format_Adaptor(const char* val) noexcept
    {
        _data = cast(void*) val;
        _format = format_internal::cstring_format_adaptor;
        _format_string = format_internal::cstring_format_string_adaptor;
    }

    static void concat_adapted_into(String_Appender* appender, Slice<Format_Adaptor> adapted)
    {
        String_Appender curr_appender = append_to(appender);
        for(Format_Adaptor const& curr_adapted : adapted)
        {
            if(curr_adapted._data == nullptr)
                break;
            
            curr_appender = append_to(&curr_appender);
            curr_adapted._format(&curr_appender, curr_adapted);
        }
    }

    static void format_adapted_into(String_Appender* appender, String format_str, Slice<Format_Adaptor> adapted)
    {
        //estimate the needed size so we dont need to reallocate so much
        grow(appender, format_str.size + 5*adapted.size);
        constexpr String sub_for = "{}";

        String_Appender curr_appender = append_to(appender);

        isize last = 0;
        isize new_found = 0;
        isize found_count = 0;

        for(found_count = 0;; last = new_found + sub_for.size, found_count++)
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
                Format_Adaptor const& curr_adapted = adapted[found_count];
                curr_appender = append_to(&curr_appender);
                if(curr_adapted._data != nullptr)
                    curr_adapted._format(&curr_appender, curr_adapted);
            }
        }
            
        String till_end = tail(format_str, last);
        push_multiple(appender, till_end);
        concat_adapted_into(appender, tail(adapted, found_count));
    }

    static void format_adapted_into(String_Appender* appender, Slice<Format_Adaptor> adapted)
    {
        if(adapted.size == 0)
            return;

        if(adapted[0]._format_string != nullptr)
        {
            Slice<Format_Adaptor> rest = tail(adapted);
            String format_str = adapted[0]._format_string(adapted[0]);
            format_adapted_into(appender, format_str, rest);
        }
        else
        {
            concat_adapted_into(appender, adapted);
        }
    }
    #define FORMAT_ADAPTOR_10_ARGS_DECL_NO_DEF \
        Format_Adaptor const& a1, Format_Adaptor const& a2, Format_Adaptor const& a3, \
        Format_Adaptor const& a4, Format_Adaptor const& a5, Format_Adaptor const& a6, \
        Format_Adaptor const& a7, Format_Adaptor const& a8, Format_Adaptor const& a9, \
        Format_Adaptor const& a10                                                     \

    #define FORMAT_ADAPTOR_10_ARGS a1, a2, a3, a4, a5, a6, a7, a8, a9, a10

    static void _format_into(String_Appender* appender, FORMAT_ADAPTOR_10_ARGS_DECL_NO_DEF)
    {
        Format_Adaptor adapted_storage[] = {FORMAT_ADAPTOR_10_ARGS};
        Slice<Format_Adaptor> adapted_slice = {adapted_storage, 10};

        return format_adapted_into(appender, adapted_slice);
    }
    
    static String_Builder _format(FORMAT_ADAPTOR_10_ARGS_DECL_NO_DEF)
    {
        String_Builder builder;
        String_Appender appender = {&builder};
        format_into(&appender, FORMAT_ADAPTOR_10_ARGS);
        return builder;
    }

    NO_INLINE
    static void format_into(String_Appender* appender, FORMAT_ADAPTOR_10_ARGS_DECL_NO_DEF)
    {
        _format_into(appender, FORMAT_ADAPTOR_10_ARGS);
    }

    NO_INLINE
    static void format_into(String_Builder* into, FORMAT_ADAPTOR_10_ARGS_DECL_NO_DEF)
    {
        String_Appender appender(into);
        return _format_into(&appender, FORMAT_ADAPTOR_10_ARGS);
    }

    NO_INLINE nodisc 
    static String_Builder format(FORMAT_ADAPTOR_10_ARGS_DECL_NO_DEF)
    {
        return _format(FORMAT_ADAPTOR_10_ARGS);
    }

    inline void print_into(FILE* stream, String str)
    {
        fwrite(str.data, sizeof(char), cast(size_t) str.size, stream);
    }
    
    inline void println_into(FILE* stream, String str)
    {
        print_into(stream, str);
        fputc('\n', stream);
    }

    NO_INLINE
    inline void print_into(FILE* stream, FORMAT_ADAPTOR_10_ARGS_DECL_NO_DEF)
    {
        String_Builder builder = _format(FORMAT_ADAPTOR_10_ARGS);
        print_into(stream, slice(builder));
    }
    
    NO_INLINE
    inline void println_into(FILE* stream, FORMAT_ADAPTOR_10_ARGS_DECL_NO_DEF)
    {
        String_Builder builder = _format(FORMAT_ADAPTOR_10_ARGS);
        push(&builder, '\n');
        print_into(stream, slice(builder));
    }

    inline void print(FORMAT_ADAPTOR_10_ARGS_DECL_NO_DEF)
    {
        print_into(stdout, FORMAT_ADAPTOR_10_ARGS);
    }
    
    inline void println(FORMAT_ADAPTOR_10_ARGS_DECL_NO_DEF)
    {
        println_into(stdout, FORMAT_ADAPTOR_10_ARGS);
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
        inline const static bool _locale_setter = set_utf8_locale(true);
    }
    #endif // !SET_UTF8_LOCALE
    
    //inline String_Builder_Panic make_panic(Line_Info line_info, FORMAT_ADAPTOR_10_ARGS_DECL) noexcept
    //{
    //    return String_Builder_Panic(line_info, format(FORMAT_ADAPTOR_10_ARGS));
    //}
    
}

#include "undefs.h"