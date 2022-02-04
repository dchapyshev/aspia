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

#include "base/codec/video_encoder.h"

#include "base/desktop/frame.h"

namespace base {

VideoEncoder::VideoEncoder(proto::VideoEncoding encoding)
    : encoding_(encoding)
{
    // Nothing
}

void VideoEncoder::fillPacketInfo(const Frame* frame, proto::VideoPacket* packet)
{
    packet->set_encoding(encoding_);

    if (last_size_ != frame->size() || last_dpi_ != frame->dpi())
    {
        last_size_ = frame->size();
        last_dpi_ = frame->dpi();

        proto::VideoPacketFormat* format = packet->mutable_format();

        proto::Rect* video_rect = format->mutable_video_rect();
        video_rect->set_width(last_size_.width());
        video_rect->set_height(last_size_.height());

        proto::Dpi* screen_dpi = format->mutable_screen_dpi();
        screen_dpi->set_dpi_x(last_dpi_.x());
        screen_dpi->set_dpi_y(last_dpi_.y());
    }
}

} // namespace base
