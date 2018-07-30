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

#ifndef ASPIA_CODEC__VIDEO_UTIL_H_
#define ASPIA_CODEC__VIDEO_UTIL_H_

#include <QRect>

#include "base/macros_magic.h"
#include "desktop_capture/pixel_format.h"
#include "protocol/desktop_session.pb.h"

namespace aspia {

class VideoUtil
{
public:
    static QRect fromVideoRect(const proto::desktop::Rect& rect);
    static void toVideoRect(const QRect& from, proto::desktop::Rect* to);

    static QSize fromVideoSize(const proto::desktop::Size& size);
    static void toVideoSize(const QSize& from, proto::desktop::Size* to);

    static QPoint fromVideoPoint(const proto::desktop::Point& point);
    static void toVideoPoint(const QPoint& from, proto::desktop::Point* to);

    static PixelFormat fromVideoPixelFormat(const proto::desktop::PixelFormat& format);
    static void toVideoPixelFormat(const PixelFormat& from, proto::desktop::PixelFormat* to);

private:
    DISALLOW_COPY_AND_ASSIGN(VideoUtil);
};

} // namespace aspia

#endif // ASPIA_CODEC__VIDEO_UTIL_H_
