#pragma once
#include "string.h"

namespace jot
{
namespace tests
{

    template <typename T>
    static void test_single_strlen(const T* str)
    {
        isize size = strlen(str);
        isize true_size = simple_strlen(str);

        TEST(size == true_size);
    }

    static void test_strlen()
    {
        test_single_strlen("");
        test_single_strlen("a");
        test_single_strlen("abc");
        test_single_strlen("Hello world!");
        test_single_strlen("Hello world! a");
        test_single_strlen("Hello world! ab");
        test_single_strlen("Hello world! abc");
        test_single_strlen("Bye");
        test_single_strlen(
            "Integer a quam sit amet nisl euismod porttitor. Vestibulum ante ipsum primis in faucibus orci luctus et ultrices posuere cubilia curae; "
            "Curabitur ex sem, venenatis porta dui efficitur, pretium faucibus lacus. Pellentesque commodo volutpat urna, ac laoreet felis sollicitudin quis. "
            "Morbi fringilla dolor quis tortor aliquam, eget egestas augue euismod. Donec tempor condimentum diam in ultricies. Pellentesque non fringilla nisl. "
            "Interdum et malesuada fames ac ante ipsum primis in faucibus. Sed pellentesque lorem nibh, et consequat elit laoreet eu. "
            "Suspendisse lobortis metus quis neque rhoncus cursus ut ut augue. In ac odio in turpis cursus congue vitae id sapien. "
            "Etiam pulvinar volutpat tincidunt. Morbi commodo sapien lacus, ac pellentesque purus scelerisque nec. "
            "Sed laoreet risus magna, sed pellentesque tellus dignissim a.");
            
        test_single_strlen(L"");
        test_single_strlen(L"a");
        test_single_strlen(L"abc");
        test_single_strlen(L"Hello world!");
        test_single_strlen(L"Hello world! a");
        test_single_strlen(L"Hello world! ab");
        test_single_strlen(L"Hello world! abc");
        test_single_strlen(L"Bye");
        test_single_strlen(
            L"Integer a quam sit amet nisl euismod porttitor. Vestibulum ante ipsum primis in faucibus orci luctus et ultrices posuere cubilia curae; "
            L"Curabitur ex sem, venenatis porta dui efficitur, pretium faucibus lacus. Pellentesque commodo volutpat urna, ac laoreet felis sollicitudin quis. "
            L"Morbi fringilla dolor quis tortor aliquam, eget egestas augue euismod. Donec tempor condimentum diam in ultricies. Pellentesque non fringilla nisl. "
            L"Interdum et malesuada fames ac ante ipsum primis in faucibus. Sed pellentesque lorem nibh, et consequat elit laoreet eu. "
            L"Suspendisse lobortis metus quis neque rhoncus cursus ut ut augue. In ac odio in turpis cursus congue vitae id sapien. "
            L"Etiam pulvinar volutpat tincidunt. Morbi commodo sapien lacus, ac pellentesque purus scelerisque nec. "
            L"Sed laoreet risus magna, sed pellentesque tellus dignissim a.");
    }
}
}
