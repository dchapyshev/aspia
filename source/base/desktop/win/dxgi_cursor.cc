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

#include "base/desktop/win/dxgi_cursor.h"

namespace base {

namespace {

constexpr int kBytesPerPixel = 4;

} // namespace

//--------------------------------------------------------------------------------------------------
DxgiCursor::DxgiCursor()
{
    memset(&pointer_shape_info_, 0, sizeof(pointer_shape_info_));
}

//--------------------------------------------------------------------------------------------------
DxgiCursor::~DxgiCursor() = default;

//--------------------------------------------------------------------------------------------------
MouseCursor* DxgiCursor::mouseCursor()
{
    if (pointer_shape_.isEmpty())
        return nullptr;

    if (!pointer_shape_info_.Width || !pointer_shape_info_.Height)
        return nullptr;

    int width = static_cast<int>(pointer_shape_info_.Width);
    int height = static_cast<int>(pointer_shape_info_.Height);
    int pitch = static_cast<int>(pointer_shape_info_.Pitch);
    QByteArray image;

    if (pointer_shape_info_.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME)
    {
        // For non-color cursors, the mask contains both an AND and an XOR mask and the height
        // includes both. Thus, the width is correct, but we need to divide by 2 to get the correct
        // mask height.
        height /= 2;

        image.resize(static_cast<size_t>(width * height * kBytesPerPixel));
        memset(image.data(), 0, image.size());

        quint8* mask_and = reinterpret_cast<quint8*>(pointer_shape_.data());
        quint8* mask_xor = reinterpret_cast<quint8*>(pointer_shape_.data()) + (pitch * height);
        int width_bytes = ((width + 15) / 16) * 2;

        auto check_bit = [](quint8 byte, int index)
        {
            quint8 temp = 128 >> index;
            return (temp & byte) != 0;
        };

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                quint32* pixel = reinterpret_cast<quint32*>(
                    image.data() + (y * width + x) * kBytesPerPixel);
                int offset = y * width_bytes + x / 8;

                bool has_and = check_bit(mask_and[offset], x % 8);
                bool has_xor = check_bit(mask_xor[offset], x % 8);

                if (!has_and && !has_xor)
                    *pixel = 0xFF000000; // Black dot.
                else if (!has_and && has_xor)
                    *pixel = 0xFFFFFFFF; // White dot.
                else if (has_and && has_xor)
                    *pixel = 0xFF000000; // Inverted.
            }
        }
    }
    else if (pointer_shape_info_.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR)
    {
        image.resize(static_cast<size_t>(width * height * kBytesPerPixel));
        memcpy(image.data(), pointer_shape_.data(), image.size());
    }
    else
    {
        return nullptr;
    }

    mouse_cursor_ = std::make_unique<MouseCursor>(
        std::move(image),
        QSize(width, height),
        QPoint(pointer_shape_info_.HotSpot.x, pointer_shape_info_.HotSpot.y));

    pointer_shape_.clear();

    return mouse_cursor_.get();
}

//--------------------------------------------------------------------------------------------------
QPoint DxgiCursor::position() const
{
    return pointer_position_;
}

//--------------------------------------------------------------------------------------------------
void DxgiCursor::setPosition(const QPoint& pointer_position)
{
    pointer_position_ = pointer_position;
}

//--------------------------------------------------------------------------------------------------
QPoint DxgiCursor::nativePosition() const
{
    return native_pointer_position_;
}

//--------------------------------------------------------------------------------------------------
void DxgiCursor::setNativePosition(const QPoint& native_pointer_position)
{
    native_pointer_position_ = native_pointer_position;
}

//--------------------------------------------------------------------------------------------------
bool DxgiCursor::isVisible() const
{
    return is_visible_;
}

//--------------------------------------------------------------------------------------------------
void DxgiCursor::setVisible(bool visible)
{
    is_visible_ = visible;
}

//--------------------------------------------------------------------------------------------------
DXGI_OUTDUPL_POINTER_SHAPE_INFO* DxgiCursor::pointerShapeInfo()
{
    return &pointer_shape_info_;
}

//--------------------------------------------------------------------------------------------------
QByteArray* DxgiCursor::pointerShapeBuffer()
{
    return &pointer_shape_;
}

} // namespace base
