//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"

#include <gtest/gtest.h>

#include <QtGlobal>

#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <limits>

#include <inttypes.h>

#if !defined(PRIuS)
#if defined(OS_WIN)
#define PRIuS "Iu"
#elif defined(OS_POSIX)
#define PRIuS "zu"
#else
#warning Unsupported OS
#endif
#endif // !defined(PRIuS)

namespace base {

namespace {

template <typename INT>
struct NumberToStringTest
{
    INT num;
    const char* sexpected;
    const char* uexpected;
};

#if defined(OS_POSIX)
//--------------------------------------------------------------------------------------------------
int _vscprintf(const char* format, va_list pargs)
{
    va_list argcopy;
    va_copy(argcopy, pargs);
    int ret = vsnprintf(nullptr, 0, format, argcopy);
    va_end(argcopy);
    return ret;
}
#endif // defined(OS_POSIX)

//--------------------------------------------------------------------------------------------------
int vsnprintfT(char* buffer, size_t buffer_size, const char* format, va_list args)
{
#if defined(OS_WIN)
    return _vsnprintf_s(buffer, buffer_size, _TRUNCATE, format, args);
#elif (OS_POSIX)
    return vsnprintf(buffer, buffer_size, format, args);
#endif
}

//--------------------------------------------------------------------------------------------------
int vscprintfT(const char* format, va_list args)
{
    return _vscprintf(format, args);
}

//--------------------------------------------------------------------------------------------------
template<class StringType>
StringType stringPrintfVT(const typename StringType::value_type* format, va_list args)
{
    va_list args_copy;
    va_copy(args_copy, args);

    const int length = vscprintfT(format, args_copy);
    if (length <= 0)
    {
        va_end(args_copy);
        return StringType();
    }

    StringType result;
    result.resize(static_cast<size_t>(length));

    const int ret = vsnprintfT(result.data(), static_cast<size_t>(length + 1), format, args_copy);
    va_end(args_copy);

    if (ret < 0 || ret > length)
        return StringType();

    return result;
}

//--------------------------------------------------------------------------------------------------
std::string stringPrintfV(const char* format, va_list args)
{
    return stringPrintfVT<std::string>(format, args);
}

//--------------------------------------------------------------------------------------------------
std::string stringPrintf(const char* format, ...)
{
    va_list args;

    va_start(args, format);
    std::string result = stringPrintfV(format, args);
    va_end(args);

    return result;
}

}  // namespace

TEST(StringNumberConversionsTest, NumberToString)
{
    static const NumberToStringTest<int> int_tests[] =
    {
        { 0, "0", "0" },
        { -1, "-1", "4294967295" },
        { std::numeric_limits<int>::max(), "2147483647", "2147483647" },
        { std::numeric_limits<int>::min(), "-2147483648", "2147483648" },
    };

    static const NumberToStringTest<int64_t> int64_tests[] =
    {
        { 0, "0", "0" },
        { -1, "-1", "18446744073709551615" },
        { std::numeric_limits<int64_t>::max(), "9223372036854775807", "9223372036854775807", },
        { std::numeric_limits<int64_t>::min(), "-9223372036854775808", "9223372036854775808" },
    };

    for (const auto& test : int_tests)
    {
        EXPECT_EQ(numberToString(test.num), test.sexpected);
        EXPECT_EQ(numberToString(static_cast<unsigned>(test.num)), test.uexpected);
    }
    for (const auto& test : int64_tests)
    {
        EXPECT_EQ(numberToString(test.num), test.sexpected);
        EXPECT_EQ(numberToString(static_cast<quint64>(test.num)), test.uexpected);
    }
}

TEST(StringNumberConversionsTest, Uint64ToString)
{
    static const struct
    {
        quint64 input;
        std::string output;
    } cases[] =
    {
        { 0, "0" },
        { 42, "42" },
        { INT_MAX, "2147483647" },
        { std::numeric_limits<quint64>::max(), "18446744073709551615" },
    };

    for (const auto& i : cases)
        EXPECT_EQ(i.output, numberToString(i.input));
}

TEST(StringNumberConversionsTest, SizeTToString)
{
    size_t size_t_max = std::numeric_limits<size_t>::max();
    std::string size_t_max_string = stringPrintf("%" PRIuS, size_t_max);

    static const struct
    {
        size_t input;
        std::string output;
    } cases[] =
    {
        { 0, "0" },
        { 9, "9" },
        { 42, "42" },
        { INT_MAX, "2147483647" },
        { 2147483648U, "2147483648" },
#if SIZE_MAX > 4294967295U
        { 99999999999U, "99999999999" },
#endif
        { size_t_max, size_t_max_string },
    };

    for (const auto& i : cases)
        EXPECT_EQ(i.output, numberToString(i.input));
}

TEST(StringNumberConversionsTest, StringToInt)
{
    static const struct
    {
        std::string input;
        int output;
        bool success;
    } cases[] =
    {
        { "0", 0, true },
        { "42", 42, true },
        { "42\x99", 42, false },
        { "\x99" "42\x99", 0, false },
        { "-2147483648", INT_MIN, true },
        { "2147483647", INT_MAX, true },
        { "", 0, false },
        { " 42", 42, false },
        { "42 ", 42, false },
        { "\t\n\v\f\r 42", 42, false },
        { "blah42", 0, false },
        { "42blah", 42, false },
        { "blah42blah", 0, false },
        { "-273.15", -273, false },
        { "+98.6", 98, false },
        { "--123", 0, false },
        { "++123", 0, false },
        { "-+123", 0, false },
        { "+-123", 0, false },
        { "-", 0, false },
        { "-2147483649", INT_MIN, false },
        { "-99999999999", INT_MIN, false },
        { "2147483648", INT_MAX, false },
        { "99999999999", INT_MAX, false },
    };

    for (const auto& i : cases)
    {
        int output = i.output ^ 1;  // Ensure stringToInt wrote something.

        bool ret = stringToInt(i.input, &output);
        EXPECT_EQ(i.success, ret);

        if (ret)
        {
            EXPECT_EQ(i.output, output);
        }
    }

    // One additional test to verify that conversion of numbers in strings with
    // embedded NUL characters. The NUL and extra data after it should be
    // interpreted as junk after the number.
    const char input[] = "6\06";
    std::string input_string(input, std::size(input) - 1);
    int output;
    EXPECT_FALSE(stringToInt(input_string, &output));
}

TEST(StringNumberConversionsTest, StringToUint)
{
    static const struct
    {
        std::string input;
        unsigned output;
        bool success;
    } cases[] =
    {
        { "0", 0, true },
        { "42", 42, true },
        { "42\x99", 42, false },
        { "\x99" "42\x99", 0, false },
        { "-2147483648", 0, false },
        { "2147483647", INT_MAX, true },
        { "", 0, false },
        { " 42", 42, false },
        { "42 ", 42, false },
        { "\t\n\v\f\r 42", 42, false },
        { "blah42", 0, false },
        { "42blah", 42, false },
        { "blah42blah", 0, false },
        { "-273.15", 0, false },
        { "+98.6", 98, false },
        { "--123", 0, false },
        { "++123", 0, false },
        { "-+123", 0, false },
        { "+-123", 0, false },
        { "-", 0, false },
        { "-2147483649", 0, false },
        { "-99999999999", 0, false },
        { "4294967295", UINT_MAX, true },
        { "4294967296", UINT_MAX, false },
        { "99999999999", UINT_MAX, false },
    };

    for (const auto& i : cases)
    {
        unsigned output = i.output ^ 1;  // Ensure StringToUint wrote something.

        bool ret = stringToUint(i.input, &output);
        EXPECT_EQ(i.success, ret);

        if (ret)
        {
            EXPECT_EQ(i.output, output);
        }
    }

    // One additional test to verify that conversion of numbers in strings with
    // embedded NUL characters.  The NUL and extra data after it should be
    // interpreted as junk after the number.
    const char input[] = "6\06";
    std::string input_string(input, std::size(input) - 1);
    unsigned output;
    EXPECT_FALSE(stringToUint(input_string, &output));
}

TEST(StringNumberConversionsTest, StringToInt64)
{
    static const struct
    {
        std::string input;
        int64_t output;
        bool success;
    } cases[] =
    {
        { "0", 0, true },
        { "42", 42, true },
        { "-2147483648", INT_MIN, true },
        { "2147483647", INT_MAX, true },
        { "-2147483649", INT64_C(-2147483649), true },
        { "-99999999999", INT64_C(-99999999999), true },
        { "2147483648", INT64_C(2147483648), true },
        { "99999999999", INT64_C(99999999999), true },
        { "9223372036854775807", std::numeric_limits<int64_t>::max(), true },
        { "-9223372036854775808", std::numeric_limits<int64_t>::min(), true },
        { "09", 9, true },
        { "-09", -9, true },
        { "", 0, false },
        { " 42", 42, false },
        { "42 ", 42, false },
        { "0x42", 0, false },
        { "\t\n\v\f\r 42", 42, false },
        { "blah42", 0, false },
        { "42blah", 42, false },
        { "blah42blah", 0, false },
        { "-273.15", -273, false },
        { "+98.6", 98, false },
        { "--123", 0, false },
        { "++123", 0, false },
        { "-+123", 0, false },
        { "+-123", 0, false },
        { "-", 0, false },
        { "-9223372036854775809", std::numeric_limits<int64_t>::min(), false },
        { "-99999999999999999999", std::numeric_limits<int64_t>::min(), false },
        { "9223372036854775808", std::numeric_limits<int64_t>::max(), false },
        { "99999999999999999999", std::numeric_limits<int64_t>::max(), false },
    };

    for (const auto& i : cases)
    {
        int64_t output = 0;

        int ret = stringToInt64(i.input, &output);
        EXPECT_EQ(i.success, ret);

        if (ret)
        {
            EXPECT_EQ(i.output, output);
        }
    }

    // One additional test to verify that conversion of numbers in strings with
    // embedded NUL characters.  The NUL and extra data after it should be
    // interpreted as junk after the number.
    const char input[] = "6\06";
    std::string input_string(input, std::size(input) - 1);
    int64_t output;
    EXPECT_FALSE(stringToInt64(input_string, &output));
}

TEST(StringNumberConversionsTest, StringToUint64)
{
    static const struct
    {
        std::string input;
        unsigned long long output;
        bool success;
    } cases[] =
    {
        { "0", 0, true },
        { "42", 42, true },
        { "-2147483648", 0, false },
        { "2147483647", INT_MAX, true },
        { "-2147483649", 0, false },
        { "-99999999999", 0, false },
        { "2147483648", UINT64_C(2147483648), true },
        { "99999999999", UINT64_C(99999999999), true },
        { "9223372036854775807", std::numeric_limits<int64_t>::max(), true },
        { "-9223372036854775808", 0, false },
        { "09", 9, true },
        { "-09", 0, false },
        { "", 0, false },
        { " 42", 42, false },
        { "42 ", 42, false },
        { "0x42", 0, false },
        { "\t\n\v\f\r 42", 42, false },
        { "blah42", 0, false },
        { "42blah", 42, false },
        { "blah42blah", 0, false },
        { "-273.15", 0, false },
        { "+98.6", 98, false },
        { "--123", 0, false },
        { "++123", 0, false },
        { "-+123", 0, false },
        { "+-123", 0, false },
        { "-", 0, false },
        { "-9223372036854775809", 0, false },
        { "-99999999999999999999", 0, false },
        { "9223372036854775808", UINT64_C(9223372036854775808), true },
        { "99999999999999999999", std::numeric_limits<quint64>::max(), false },
        { "18446744073709551615", std::numeric_limits<quint64>::max(), true },
        { "18446744073709551616", std::numeric_limits<quint64>::max(), false },
    };

    for (const auto& i : cases)
    {
        unsigned long long output = 0;

        bool ret = stringToULong64(i.input, &output);
        EXPECT_EQ(i.success, ret);

        if (ret)
        {
            EXPECT_EQ(i.output, output);
        }
    }

    // One additional test to verify that conversion of numbers in strings with
    // embedded NUL characters.  The NUL and extra data after it should be
    // interpreted as junk after the number.
    const char input[] = "6\06";
    std::string input_string(input, std::size(input) - 1);
    unsigned long long output;
    EXPECT_FALSE(stringToULong64(input_string, &output));
}

} // namespace base
