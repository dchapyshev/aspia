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

#ifndef BASE_DESKTOP_WIN_DXGI_CURSOR_H
#define BASE_DESKTOP_WIN_DXGI_CURSOR_H

#include "base/desktop/mouse_cursor.h"

#include <dxgi.h>
#include <dxgi1_2.h>

namespace base {

class DxgiCursor
{
public:
    DxgiCursor();
    ~DxgiCursor();

    MouseCursor* mouseCursor();

    Point position() const;
    void setPosition(const Point& pointer_position);

    Point nativePosition() const;
    void setNativePosition(const Point& native_pointer_position);

    bool isVisible() const;
    void setVisible(bool visible);

    DXGI_OUTDUPL_POINTER_SHAPE_INFO* pointerShapeInfo();
    QByteArray* pointerShapeBuffer();

private:
    DXGI_OUTDUPL_POINTER_SHAPE_INFO pointer_shape_info_;
    QByteArray pointer_shape_;
    Point pointer_position_;
    Point native_pointer_position_;
    bool is_visible_ = false;

    std::unique_ptr<MouseCursor> mouse_cursor_;

    Q_DISABLE_COPY(DxgiCursor)
};

} // namespace base

#endif // BASE_DESKTOP_WIN_DXGI_CURSOR_H
