//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "codec/video_encoder.h"

#include "desktop/frame.h"

namespace codec {

void VideoEncoder::fillPacketInfo(proto::VideoEncoding encoding,
                                  const desktop::Frame* frame,
                                  proto::VideoPacket* packet)
{
    packet->set_encoding(encoding);

    if (last_size_ != frame->size())
    {
        last_size_ = frame->size();

        proto::Rect* rect = packet->mutable_format()->mutable_screen_rect();
        rect->set_width(last_size_.width());
        rect->set_height(last_size_.height());
    }
}

} // namespace codec
