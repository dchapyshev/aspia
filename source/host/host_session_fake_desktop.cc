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

#include "host/host_session_fake_desktop.h"

#include "codec/video_encoder_vpx.h"
#include "codec/video_encoder_zstd.h"
#include "codec/video_util.h"
#include "common/message_serialization.h"
#include "desktop_capture/desktop_frame_simple.h"

namespace aspia {

HostSessionFakeDesktop::HostSessionFakeDesktop(QObject* parent)
    : HostSessionFake(parent)
{
    // Nothing
}

void HostSessionFakeDesktop::startSession()
{
    proto::desktop::HostToClient message;
    message.mutable_config_request()->set_dummy(1);
    emit sendMessage(serializeMessage(message));
}

void HostSessionFakeDesktop::onMessageReceived(const QByteArray& buffer)
{
    proto::desktop::ClientToHost incoming_message;

    if (!parseMessage(buffer, incoming_message))
    {
        LOG(LS_WARNING) << "Unable to parse message";
        emit errorOccurred();
        return;
    }

    if (incoming_message.has_config())
    {
        std::unique_ptr<codec::VideoEncoder> video_encoder(createEncoder(incoming_message.config()));
        if (!video_encoder)
        {
            LOG(LS_WARNING) << "Unable to create video encoder";
            emit errorOccurred();
            return;
        }

        std::unique_ptr<DesktopFrame> frame = createFrame();
        if (!frame)
        {
            LOG(LS_WARNING) << "Unable to create video frame";
            emit errorOccurred();
            return;
        }

        proto::desktop::HostToClient outgoing_message;
        video_encoder->encode(frame.get(), outgoing_message.mutable_video_packet());
        emit sendMessage(serializeMessage(outgoing_message));
    }
    else
    {
        // Other messages are ignored.
    }
}

codec::VideoEncoder* HostSessionFakeDesktop::createEncoder(const proto::desktop::Config& config)
{
    switch (config.video_encoding())
    {
        case proto::desktop::VIDEO_ENCODING_VP8:
            return codec::VideoEncoderVPX::createVP8();

        case proto::desktop::VIDEO_ENCODING_VP9:
            return codec::VideoEncoderVPX::createVP9();

        case proto::desktop::VIDEO_ENCODING_ZSTD:
            return codec::VideoEncoderZstd::create(
                codec::VideoUtil::fromVideoPixelFormat(
                    config.pixel_format()), config.compress_ratio());

        default:
            LOG(LS_WARNING) << "Unsupported video encoding: " << config.video_encoding();
            return nullptr;
    }
}

std::unique_ptr<DesktopFrame> HostSessionFakeDesktop::createFrame()
{
    static const int kWidth = 800;
    static const int kHeight = 600;

    std::unique_ptr<DesktopFrameSimple> frame =
        DesktopFrameSimple::create(QSize(kWidth, kHeight), PixelFormat::ARGB());
    if (!frame)
        return nullptr;

    memset(frame->frameData(), 0, frame->stride() * frame->size().height());

    *frame->updatedRegion() += QRect(QPoint(), frame->size());
    return frame;
}

} // namespace aspia
