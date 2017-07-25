//
// PROJECT:         Aspia Remote Desktop
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

ClientSessionDesktopView::ClientSessionDesktopView(const ClientConfig& config,
                                                   ClientSession::Delegate* delegate) :
    ClientSession(config, delegate)
{
    viewer_.reset(new UiViewerWindow(&config_, this));
}

ClientSessionDesktopView::~ClientSessionDesktopView()
{
    viewer_.reset();
}

static std::unique_ptr<VideoDecoder>
CreateVideoDecoder(proto::VideoEncoding encoding)
{
    switch (encoding)
    {
        case proto::VIDEO_ENCODING_VP8:
            return VideoDecoderVPX::CreateVP8();

        case proto::VIDEO_ENCODING_VP9:
            return VideoDecoderVPX::CreateVP9();

        case proto::VIDEO_ENCODING_ZLIB:
            return VideoDecoderZLIB::Create();

        default:
            LOG(ERROR) << "Unsupported encoding: " << encoding;
            return nullptr;
    }
}

bool ClientSessionDesktopView::ReadVideoPacket(const proto::VideoPacket& video_packet)
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

        if (video_encoding_ == proto::VideoEncoding::VIDEO_ENCODING_ZLIB)
        {
            pixel_format =
                ConvertFromVideoPixelFormat(video_packet.format().pixel_format());
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

        DesktopSize size =
            ConvertFromVideoSize(video_packet.format().screen_size());

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
    const proto::DesktopSessionConfigRequest& config_request)
{
    OnConfigChange(config_.desktop_session_config());
}

void ClientSessionDesktopView::Send(IOBuffer buffer)
{
    proto::desktop::HostToClient message;

    if (ParseMessage(buffer, message))
    {
        if (message.status() != proto::Status::STATUS_SUCCESS)
        {
            // TODO: Status processing.
            return;
        }

        bool success = true;

        if (message.has_video_packet())
        {
            success = ReadVideoPacket(message.video_packet());
        }
        else if (message.has_audio_packet())
        {
            DLOG(WARNING) << "Audio not implemented yet";
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
            return;
    }

    delegate_->OnSessionTerminate();
}

void ClientSessionDesktopView::WriteMessage(
    const proto::desktop::ClientToHost& message)
{
    IOBuffer buffer(SerializeMessage<IOBuffer>(message));

    if (!buffer.IsEmpty())
    {
        delegate_->OnSessionMessageAsync(std::move(buffer));
        return;
    }

    delegate_->OnSessionTerminate();
}

void ClientSessionDesktopView::OnWindowClose()
{
    delegate_->OnSessionTerminate();
}

void ClientSessionDesktopView::OnConfigChange(
    const proto::DesktopSessionConfig& config)
{
    proto::desktop::ClientToHost message;
    message.mutable_config()->CopyFrom(config);
    WriteMessage(message);
}

} // namespace aspia
