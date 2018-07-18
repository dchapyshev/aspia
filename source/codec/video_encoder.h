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

#ifndef _ASPIA_CODEC__VIDEO_ENCODER_H
#define _ASPIA_CODEC__VIDEO_ENCODER_H

#include <QRect>
#include <QSize>

#include <memory>

#include "protocol/desktop_session.pb.h"

namespace aspia {

class DesktopFrame;

class VideoEncoder
{
public:
    virtual ~VideoEncoder() = default;

    virtual std::unique_ptr<proto::desktop::VideoPacket> encode(const DesktopFrame* frame) = 0;

protected:
    std::unique_ptr<proto::desktop::VideoPacket> createVideoPacket(
        proto::desktop::VideoEncoding encoding, const DesktopFrame* frame);

private:
    QSize screen_size_;
    QPoint top_left_;
};

} // namespace aspia

#endif // _ASPIA_CODEC__VIDEO_ENCODER_H
