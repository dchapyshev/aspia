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

#ifndef BASE_DESKTOP_FRAME_ALIGNED_H
#define BASE_DESKTOP_FRAME_ALIGNED_H

#include "base/desktop/frame.h"

#include <memory>

namespace base {

class FrameAligned final : public Frame
{
public:
    ~FrameAligned() final;

    static std::unique_ptr<FrameAligned> create(
        const QSize& size, const PixelFormat& format, size_t alignment);

private:
    FrameAligned(const QSize& size, const PixelFormat& format, quint8* data);

    Q_DISABLE_COPY(FrameAligned)
};

} // namespace base

#endif // BASE_DESKTOP_FRAME_ALIGNED_H
