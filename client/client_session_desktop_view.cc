//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session_desktop_view.cc
// LICENSE:         See top-level directory
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
    viewer_.reset(new ViewerWindow(config, this));
}

ClientSessionDesktopView::~ClientSessionDesktopView()
{
    viewer_.reset();
}

bool ClientSessionDesktopView::ReadVideoPacket(const proto::VideoPacket& video_packet)
{
    if (video_encoding_ != video_packet.encoding())
    {
        video_encoding_ = video_packet.encoding();

        switch (video_encoding_)
        {
            case proto::VIDEO_ENCODING_VP8:
                video_decoder_ = VideoDecoderVPX::CreateVP8();
                break;

            case proto::VIDEO_ENCODING_VP9:
                video_decoder_ = VideoDecoderVPX::CreateVP9();
                break;

            case proto::VIDEO_ENCODING_ZLIB:
                video_decoder_ = VideoDecoderZLIB::Create();
                break;

            default:
                LOG(ERROR) << "Unsupported encoding: " << video_encoding_;
                return false;
        }
    }

    if (video_packet.has_screen_size() || video_packet.has_pixel_format())
    {
        PixelFormat pixel_format;

        if (video_encoding_ == proto::VideoEncoding::VIDEO_ENCODING_ZLIB)
        {
            pixel_format = ConvertFromVideoPixelFormat(video_packet.pixel_format());
        }
        else
        {
            pixel_format = PixelFormat::ARGB();
        }

        if (!pixel_format.IsValid())
        {
            LOG(ERROR) << "Wrong pixel format";
            return false;
        }

        viewer_->ResizeFrame(ConvertFromVideoSize(video_packet.screen_size()), pixel_format);
    }

    DesktopFrame* frame = viewer_->Frame();
    if (!frame)
        return false;

    if (!video_decoder_->Decode(video_packet, frame))
        return false;

    viewer_->DrawFrame();

    return true;
}

void ClientSessionDesktopView::ReadConfigRequest(const proto::DesktopSessionConfigRequest& config_request)
{
    OnConfigChange(config_.desktop_session_config());
}

void ClientSessionDesktopView::Send(const IOBuffer& buffer)
{
    proto::desktop::HostToClient message;

    if (ParseMessage(buffer, message))
    {
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

void ClientSessionDesktopView::WriteMessage(const proto::desktop::ClientToHost& message)
{
    IOBuffer buffer = SerializeMessage(message);

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

void ClientSessionDesktopView::OnConfigChange(const proto::DesktopSessionConfig& config)
{
    proto::desktop::ClientToHost message;
    message.mutable_config()->CopyFrom(config);
    WriteMessage(message);
}

} // namespace aspia
