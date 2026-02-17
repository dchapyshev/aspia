//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/win/scoped_object.h"

#include <gtest/gtest.h>

namespace base::win {

TEST(ScopedHandleTest, ScopedHandle)
{
    // Any illegal error code will do. We just need to test that it is preserved
    // by ScopedHandle to avoid bug 528394.
    const DWORD magic_error = 0x12345678;

    HANDLE handle = CreateMutexW(nullptr, false, nullptr);
    // Call SetLastError after creating the handle.
    SetLastError(magic_error);
    base::ScopedHandle handle_holder(handle);
    EXPECT_EQ(magic_error, GetLastError());

    // Create a new handle and then set LastError again.
    handle = CreateMutexW(nullptr, false, nullptr);
    SetLastError(magic_error);
    handle_holder.reset(handle);
    EXPECT_EQ(magic_error, GetLastError());

    // Create a new handle and then set LastError again.
    handle = CreateMutexW(nullptr, false, nullptr);
    base::ScopedHandle handle_source(handle);
    SetLastError(magic_error);
    handle_holder = std::move(handle_source);
    EXPECT_EQ(magic_error, GetLastError());
}

} // namespace base::win
