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

#include "base/desktop/mouse_cursor.h"
#include "base/desktop/win/cursor.h"
#include "base/desktop/win/cursor_unittest_resources.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_user_object.h"

#include <gtest/gtest.h>

namespace base {

namespace {

// Loads |left| from resources, converts it to a |MouseCursor| instance and
// compares pixels with |right|. Returns true of MouseCursor bits match |right|.
// |right| must be a 32bpp cursor with alpha channel.
bool convertToMouseShapeAndCompare(unsigned left, unsigned right)
{
    HMODULE instance = GetModuleHandleW(nullptr);

    // Load |left| from the EXE module's resources.
    ScopedHCURSOR cursor(reinterpret_cast<HCURSOR>(
        LoadImageW(instance, MAKEINTRESOURCEW(left), IMAGE_CURSOR, 0, 0, 0)));
    EXPECT_TRUE(cursor != nullptr);

    // Convert |cursor| to |mouse_shape|.
    HDC dc = GetDC(nullptr);
    std::unique_ptr<MouseCursor> mouse_shape(mouseCursorFromHCursor(dc, cursor));
    ReleaseDC(nullptr, dc);

    EXPECT_TRUE(mouse_shape.get());

    // Load |right|.
    cursor.reset(reinterpret_cast<HCURSOR>(
        LoadImageW(instance, MAKEINTRESOURCEW(right), IMAGE_CURSOR, 0, 0, 0)));

    ICONINFO iinfo;
    EXPECT_TRUE(GetIconInfo(cursor, &iinfo));
    EXPECT_TRUE(iinfo.hbmColor);

    // Make sure the bitmaps will be freed.
    ScopedHBITMAP scoped_mask(iinfo.hbmMask);
    ScopedHBITMAP scoped_color(iinfo.hbmColor);

    // Get |scoped_color| dimensions.
    BITMAP bitmap_info;
    EXPECT_TRUE(GetObjectW(scoped_color, sizeof(bitmap_info), &bitmap_info));

    int width = bitmap_info.bmWidth;
    int height = bitmap_info.bmHeight;
    EXPECT_TRUE(QSize(width, height) == mouse_shape->size());

    // Get the pixels from |scoped_color|.
    int size = width * height;
    std::unique_ptr<quint32[]> data(new quint32[size]);
    EXPECT_TRUE(GetBitmapBits(scoped_color, size * sizeof(quint32), data.get()));

    // Compare the 32bpp image in |mouse_shape| with the one loaded from |right|.
    return memcmp(data.get(), mouse_shape->constImage().data(), size * sizeof(quint32)) == 0;
}

} // namespace

TEST(CursorTest, MatchCursors)
{
    EXPECT_TRUE(convertToMouseShapeAndCompare(IDD_CURSOR1_24BPP, IDD_CURSOR1_32BPP));
    EXPECT_TRUE(convertToMouseShapeAndCompare(IDD_CURSOR1_8BPP, IDD_CURSOR1_32BPP));
    EXPECT_TRUE(convertToMouseShapeAndCompare(IDD_CURSOR2_1BPP, IDD_CURSOR2_32BPP));
    EXPECT_TRUE(convertToMouseShapeAndCompare(IDD_CURSOR3_4BPP, IDD_CURSOR3_32BPP));
}

} // namespace base
