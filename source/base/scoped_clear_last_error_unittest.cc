//
// SmartCafe Project
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

#include "base/scoped_clear_last_error.h"

#include <gtest/gtest.h>

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#endif // defined(Q_OS_WINDOWS)

namespace base {

TEST(ScopedClearLastError, test_no_error)
{
    errno = 1;
    {
        ScopedClearLastError clear_error;
        EXPECT_EQ(0, errno);
    }
    EXPECT_EQ(1, errno);
}

TEST(ScopedClearLastError, test_error)
{
    errno = 1;
    {
        ScopedClearLastError clear_error;
        errno = 2;
    }
    EXPECT_EQ(1, errno);
}

#if defined(Q_OS_WINDOWS)

TEST(ScopedClearLastError, test_no_error_win)
{
    ::SetLastError(1);
    {
        ScopedClearLastError clear_error;
        EXPECT_EQ(0, ::GetLastError());
    }
    EXPECT_EQ(1, ::GetLastError());
}

TEST(ScopedClearLastError, test_error_win)
{
    ::SetLastError(1);
    {
        ScopedClearLastError clear_error;
        ::SetLastError(2);
    }
    EXPECT_EQ(1, ::GetLastError());
}

#endif  // defined(Q_OS_WINDOWS)

} // namespace base
