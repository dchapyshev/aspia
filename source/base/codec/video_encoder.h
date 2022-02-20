//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_CODEC_VIDEO_ENCODER_H
#define BASE_CODEC_VIDEO_ENCODER_H

#include "base/desktop/geometry.h"
#include "proto/desktop.pb.h"

namespace base {

class Frame;

class VideoEncoder
{
public:
    explicit VideoEncoder(proto::VideoEncoding encoding);
    virtual ~VideoEncoder() = default;

    virtual void encode(const Frame* frame, proto::VideoPacket* packet) = 0;

    proto::VideoEncoding encoding() const { return encoding_; }

protected:
    void fillPacketInfo(const Frame* frame, proto::VideoPacket* packet);

private:
    const proto::VideoEncoding encoding_;
    Size last_size_;
};

} // namespace base

#endif // BASE_CODEC_VIDEO_ENCODER_H
