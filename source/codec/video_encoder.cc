//
// PROJECT:         Aspia
// FILE:            codec/video_encoder.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
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
