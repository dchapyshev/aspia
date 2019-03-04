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

#ifndef CODEC__VIDEO_UTIL_H
#define CODEC__VIDEO_UTIL_H

#include "base/macros_magic.h"
#include "desktop/pixel_format.h"
#include "proto/desktop.pb.h"

#include <QRect>

namespace codec {

class VideoUtil
{
public:
    static QRect fromVideoRect(const proto::desktop::Rect& rect);
    static void toVideoRect(const QRect& from, proto::desktop::Rect* to);

    static desktop::PixelFormat fromVideoPixelFormat(
        const proto::desktop::PixelFormat& format);

    static void toVideoPixelFormat(
        const desktop::PixelFormat& from, proto::desktop::PixelFormat* to);

private:
    DISALLOW_COPY_AND_ASSIGN(VideoUtil);
};

} // namespace codec

#endif // CODEC__VIDEO_UTIL_H
