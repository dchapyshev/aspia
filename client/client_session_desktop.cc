//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_session_desktop.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_session_desktop.h"
#include "base/logging.h"
#include "codec/video_decoder_zlib.h"
#include "codec/video_decoder_vpx.h"
#include "codec/video_helpers.h"
#include "protocol/message_serialization.h"

namespace aspia {

ClientSessionDesktop::ClientSessionDesktop(const ClientConfig& config,
                                           ClientSession::Delegate* delegate) :
    ClientSession(config, delegate)
{
    viewer_.reset(new ViewerWindow(config, this));
}

ClientSessionDesktop::~ClientSessionDesktop()
{
    viewer_.reset();
}

bool ClientSessionDesktop::ReadVideoPacket(const proto::VideoPacket& video_packet)
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

void ClientSessionDesktop::ReadCursorShape(const proto::CursorShape& cursor_shape)
{
    if (!cursor_decoder_)
        cursor_decoder_.reset(new CursorDecoder());

    std::shared_ptr<MouseCursor> mouse_cursor = cursor_decoder_->Decode(cursor_shape);

    if (mouse_cursor)
        viewer_->InjectMouseCursor(mouse_cursor);
}

void ClientSessionDesktop::ReadClipboardEvent(std::shared_ptr<proto::ClipboardEvent> clipboard_event)
{
    viewer_->InjectClipboardEvent(clipboard_event);
}

void ClientSessionDesktop::ReadConfigRequest(const proto::DesktopSessionConfigRequest& config_request)
{
    OnConfigChange(config_.desktop_session_config());
}

void ClientSessionDesktop::Send(const IOBuffer& buffer)
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
            LOG(WARNING) << "Audio not implemented yet";
        }
        else if (message.has_cursor_shape())
        {
            ReadCursorShape(message.cursor_shape());
        }
        else if (message.has_clipboard_event())
        {
            std::shared_ptr<proto::ClipboardEvent> clipboard_event(
                message.release_clipboard_event());

            ReadClipboardEvent(clipboard_event);
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

void ClientSessionDesktop::WriteMessage(const proto::desktop::ClientToHost& message)
{
    IOBuffer buffer = SerializeMessage(message);

    if (!buffer.IsEmpty())
    {
        delegate_->OnSessionMessage(std::move(buffer));
        return;
    }

    delegate_->OnSessionTerminate();
}

void ClientSessionDesktop::OnWindowClose()
{
    delegate_->OnSessionTerminate();
}

void ClientSessionDesktop::OnConfigChange(const proto::DesktopSessionConfig& config)
{
    proto::desktop::ClientToHost message;
    message.mutable_config()->CopyFrom(config);
    WriteMessage(message);
}

void ClientSessionDesktop::OnKeyEvent(uint32_t keycode, uint32_t flags)
{
    proto::desktop::ClientToHost message;

    proto::KeyEvent* event = message.mutable_key_event();
    event->set_keycode(keycode);
    event->set_flags(flags);

    WriteMessage(message);
}

void ClientSessionDesktop::OnPointerEvent(int x, int y, uint32_t mask)
{
    proto::desktop::ClientToHost message;

    proto::PointerEvent* event = message.mutable_pointer_event();
    event->set_x(x);
    event->set_y(y);
    event->set_mask(mask);

    WriteMessage(message);
}

void ClientSessionDesktop::OnPowerEvent(proto::PowerEvent::Action action)
{
    proto::desktop::ClientToHost message;
    message.mutable_power_event()->set_action(action);
    WriteMessage(message);
}

void ClientSessionDesktop::OnClipboardEvent(std::unique_ptr<proto::ClipboardEvent> clipboard_event)
{
    proto::desktop::ClientToHost message;
    message.set_allocated_clipboard_event(clipboard_event.release());
    WriteMessage(message);
}

} // namespace aspia
