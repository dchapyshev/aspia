//
// PROJECT:         Aspia
// FILE:            codec/video_util.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__VIDEO_UTIL_H
#define _ASPIA_CODEC__VIDEO_UTIL_H

#include <QRect>
#include <QSize>

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

    static PixelFormat fromVideoPixelFormat(const proto::desktop::PixelFormat& format);
    static void toVideoPixelFormat(const PixelFormat& from, proto::desktop::PixelFormat* to);

private:
    Q_DISABLE_COPY(VideoUtil)
};

} // namespace aspia

#endif // _ASPIA_CODEC__VIDEO_UTIL_H
