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

#include "codec/video_encoder.h"

#include "codec/video_util.h"
#include "desktop_capture/desktop_frame.h"

namespace aspia {

// static
std::unique_ptr<proto::desktop::VideoPacket> VideoEncoder::createVideoPacket(
    proto::desktop::VideoEncoding encoding, const DesktopFrame* frame)
{
    std::unique_ptr<proto::desktop::VideoPacket> packet =
        std::make_unique<proto::desktop::VideoPacket>();

    packet->set_encoding(encoding);

    if (screen_size_ != frame->size() || top_left_ != frame->topLeft())
    {
        screen_size_ = frame->size();
        top_left_ = frame->topLeft();

        proto::desktop::VideoPacketFormat* format = packet->mutable_format();

        VideoUtil::toVideoSize(screen_size_, format->mutable_screen_size());
        VideoUtil::toVideoPoint(top_left_, format->mutable_top_left());
    }

    return packet;
}

} // namespace aspia
