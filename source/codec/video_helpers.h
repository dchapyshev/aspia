//
// PROJECT:         Aspia
// FILE:            codec/video_helpers.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__VIDEO_HELPERS_H
#define _ASPIA_CODEC__VIDEO_HELPERS_H

#include <QRect>
#include <QSize>

#include "desktop_capture/pixel_format.h"
#include "proto/desktop_session.pb.h"

#include <memory>

namespace aspia {

std::unique_ptr<proto::desktop::VideoPacket> CreateVideoPacket(
    proto::desktop::VideoEncoding encoding);

QRect ConvertFromVideoRect(const proto::desktop::Rect& rect);

void ConvertToVideoRect(const QRect& from, proto::desktop::Rect* to);

QSize ConvertFromVideoSize(const proto::desktop::Size& size);

void ConvertToVideoSize(const QSize& from, proto::desktop::Size* to);

PixelFormat ConvertFromVideoPixelFormat(const proto::desktop::PixelFormat& format);

void ConvertToVideoPixelFormat(const PixelFormat& from, proto::desktop::PixelFormat* to);

} // namespace aspia

#endif // _ASPIA_CODEC__VIDEO_HELPERS_H
