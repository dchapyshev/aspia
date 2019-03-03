//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef DESKTOP__DESKTOP_FRAME_SIMPLE_H
#define DESKTOP__DESKTOP_FRAME_SIMPLE_H

#include "desktop/desktop_frame.h"

#include <memory>

namespace desktop {

class FrameSimple : public Frame
{
public:
    ~FrameSimple();

    static std::unique_ptr<FrameSimple> create(const QSize& size, const PixelFormat& format);

private:
    FrameSimple(const QSize& size, const PixelFormat& format, int stride, uint8_t* data);

    DISALLOW_COPY_AND_ASSIGN(FrameSimple);
};

} // namespace desktop

#endif // DESKTOP__DESKTOP_FRAME_SIMPLE_H
