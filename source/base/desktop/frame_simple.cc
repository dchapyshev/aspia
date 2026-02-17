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

#include "base/desktop/frame_simple.h"

namespace base {

//--------------------------------------------------------------------------------------------------
FrameSimple::FrameSimple(const QSize& size, const PixelFormat& format, quint8* data)
    : Frame(size, format, size.width() * format.bytesPerPixel(), data, nullptr)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
FrameSimple::~FrameSimple()
{
    free(data_);
}

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<FrameSimple> FrameSimple::create(const QSize& size, const PixelFormat& format)
{
    quint8* data = reinterpret_cast<quint8*>(malloc(calcMemorySize(size, format.bytesPerPixel())));
    if (!data)
        return nullptr;

    return std::unique_ptr<FrameSimple>(new FrameSimple(size, format, data));
}

} // namespace base
