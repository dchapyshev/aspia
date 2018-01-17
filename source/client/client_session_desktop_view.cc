//
// PROJECT:         Aspia
// FILE:            client/client_session_desktop_view.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_session_desktop_view.h"
#include "base/logging.h"
#include "codec/video_decoder_zlib.h"
#include "codec/video_decoder_vpx.h"
#include "codec/video_helpers.h"
#include "protocol/message_serialization.h"

namespace aspia {

namespace {

std::unique_ptr<VideoDecoder> CreateVideoDecoder(proto::desktop::VideoEncoding encoding)
{
    switch (encoding)
    {
        case proto::desktop::VIDEO_ENCODING_VP8:
            return VideoDecoderVPX::CreateVP8();

        case proto::desktop::VIDEO_ENCODING_VP9:
            return VideoDecoderVPX::CreateVP9();

        case proto::desktop::VIDEO_ENCODING_ZLIB:
            return VideoDecoderZLIB::Create();

        default:
            LOG(ERROR) << "Unsupported encoding: " << encoding;
            return nullptr;
    }
}

} // namespace

ClientSessionDesktopView::ClientSessionDesktopView(
    const ClientConfig& config,
    std::shared_ptr<NetworkChannelProxy> channel_proxy)
    : ClientSession(config, channel_proxy)
{
    viewer_.reset(new ViewerWindow(&config_, this));

    channel_proxy_->Receive(std::bind(
        &ClientSessionDesktopView::OnMessageReceived, this, std::placeholders::_1));
}

ClientSessionDesktopView::~ClientSessionDesktopView()
{
    viewer_.reset();
}

bool ClientSessionDesktopView::ReadVideoPacket(const proto::desktop::VideoPacket& video_packet)
{
    if (video_encoding_ != video_packet.encoding())
    {
        video_encoding_ = video_packet.encoding();
        video_decoder_ = CreateVideoDecoder(video_encoding_);
    }

    if (!video_decoder_)
        return false;

    if (video_packet.has_format())
    {
        PixelFormat pixel_format;

        if (video_encoding_ == proto::desktop::VIDEO_ENCODING_ZLIB)
        {
            pixel_format = ConvertFromVideoPixelFormat(video_packet.format().pixel_format());
        }
        else
        {
            pixel_format = PixelFormat::ARGB();
        }

        if (!pixel_format.IsValid())
        {
            LOG(ERROR) << "Wrong pixel format for frame";
            return false;
        }

        DesktopSize size = ConvertFromVideoSize(video_packet.format().screen_size());

        if (size.IsEmpty())
        {
            LOG(ERROR) << "Wrong size for frame";
            return false;
        }

        viewer_->ResizeFrame(size, pixel_format);
    }

    DesktopFrame* frame = viewer_->Frame();
    if (!frame)
        return false;

    if (!video_decoder_->Decode(video_packet, frame))
        return false;

    viewer_->DrawFrame();

    return true;
}

void ClientSessionDesktopView::ReadConfigRequest(
    const proto::desktop::ConfigRequest& /* config_request */)
{
    OnConfigChange(config_.desktop_session_config());
}

void ClientSessionDesktopView::OnMessageReceived(const IOBuffer& buffer)
{
    proto::desktop::HostToClient message;

    if (ParseMessage(buffer, message))
    {
        bool success = true;

        if (message.has_video_packet())
        {
            success = ReadVideoPacket(message.video_packet());
        }
        else if (message.has_config_request())
        {
            ReadConfigRequest(message.config_request());
        }
        else
        {
            // Unknown messages are ignored.
            DLOG(WARNING) << "Unhandled message from host";
        }

        if (success)
        {
            channel_proxy_->Receive(std::bind(
                &ClientSessionDesktopView::OnMessageReceived, this, std::placeholders::_1));
            return;
        }
    }

    channel_proxy_->Disconnect();
}

void ClientSessionDesktopView::WriteMessage(const proto::desktop::ClientToHost& message)
{
    channel_proxy_->Send(SerializeMessage<IOBuffer>(message));
}

void ClientSessionDesktopView::OnWindowClose()
{
    channel_proxy_->Disconnect();
}

void ClientSessionDesktopView::OnConfigChange(const proto::desktop::Config& config)
{
    proto::desktop::ClientToHost message;
    message.mutable_config()->CopyFrom(config);
    WriteMessage(message);
}

void ClientSessionDesktopView::OnKeyEvent(uint32_t /* keycode */, uint32_t /* flags */)
{
    // Nothing
}

void ClientSessionDesktopView::OnPointerEvent(const DesktopPoint& /* pos */, uint32_t /* mask */)
{
    // Nothing
}

void ClientSessionDesktopView::OnClipboardEvent(
    proto::desktop::ClipboardEvent& /* clipboard_event */)
{
    // Nothing
}

} // namespace aspia
