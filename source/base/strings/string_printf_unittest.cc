//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/strings/string_printf.h"
#include "build/build_config.h"

#include <gtest/gtest.h>

namespace base {

TEST(string_printf_test, string_printf_empty)
{
    EXPECT_EQ("", stringPrintf("%s", ""));
}

TEST(string_printf_test, string_printf_misc)
{
    EXPECT_EQ("123hello w", stringPrintf("%3d%2s %1c", 123, "hello", 'w'));
#if defined(OS_WIN)
    EXPECT_EQ(L"123hello w", stringPrintf(L"%3d%2ls %1lc", 123, L"hello", 'w'));
#endif
}

// Make sure that lengths exactly around the initial buffer size are handled
// correctly.
TEST(string_printf_test, string_printf_bounds)
{
    const int kSrcLen = 1026;
    char src[kSrcLen];

    for (size_t i = 0; i < std::size(src); i++)
        src[i] = 'A';

    wchar_t srcw[kSrcLen];
    for (size_t i = 0; i < std::size(srcw); i++)
        srcw[i] = 'A';

    for (int i = 1; i < 3; i++)
    {
        src[kSrcLen - i] = 0;
        std::string out;
        sStringPrintf(&out, "%s", src);
        EXPECT_STREQ(src, out.c_str());

#if defined(OS_WIN)
        srcw[kSrcLen - i] = 0;
        std::wstring outw;
        sStringPrintf(&outw, L"%ls", srcw);
        EXPECT_STREQ(srcw, outw.c_str());
#endif
    }
}

// Test very large sprintfs that will cause the buffer to grow.
TEST(string_printf_test, grow)
{
    char src[1026];
    for (size_t i = 0; i < std::size(src); i++)
        src[i] = 'A';
    src[1025] = 0;

    const char fmt[] = "%sB%sB%sB%sB%sB%sB%s";

    std::string out;
    sStringPrintf(&out, fmt, src, src, src, src, src, src, src);

    const int kRefSize = 320000;
    char* ref = new char[kRefSize];
#if defined(OS_WIN)
    sprintf_s(ref, kRefSize, fmt, src, src, src, src, src, src, src);
#elif defined(OS_UNIX)
    snprintf(ref, kRefSize, fmt, src, src, src, src, src, src, src);
#endif

    EXPECT_STREQ(ref, out.c_str());
    delete[] ref;
}

// Test the boundary condition for the size of the string_util's
// internal buffer.
TEST(string_printf_test, grow_boundary)
{
    const int kStringUtilBufLen = 1024;
    // Our buffer should be one larger than the size of StringAppendVT's stack
    // buffer.
    // And need extra one for NULL-terminator.
    const int kBufLen = kStringUtilBufLen + 1 + 1;
    char src[kBufLen];
    for (int i = 0; i < kBufLen - 1; ++i)
        src[i] = 'a';
    src[kBufLen - 1] = 0;

    std::string out;
    sStringPrintf(&out, "%s", src);

    EXPECT_STREQ(src, out.c_str());
}

#if defined(OS_WIN)
// vswprintf in Visual Studio 2013 fails when given U+FFFF. This tests that the
// failure case is gracefuly handled. In Visual Studio 2015 the bad character
// is passed through.
TEST(string_printf_test, invalid)
{
    wchar_t invalid[2];
    invalid[0] = 0xffff;
    invalid[1] = 0;

    std::wstring out;
    sStringPrintf(&out, L"%ls", invalid);
#if _MSC_VER >= 1900
    EXPECT_STREQ(invalid, out.c_str());
#else
    EXPECT_STREQ(L"", out.c_str());
#endif
}
#endif

} // namespace base
