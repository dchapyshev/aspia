//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/desktop_session_client.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/desktop_session_client.h"
#include "host/power_injector.h"
#include "codec/video_encoder_zlib.h"
#include "codec/video_encoder_vpx.h"
#include "codec/video_helpers.h"
#include "protocol/message_serialization.h"

namespace aspia {

static const uint32_t kSupportedVideoEncodings =
    proto::VideoEncoding::VIDEO_ENCODING_ZLIB | proto::VideoEncoding::VIDEO_ENCODING_VP8 |
    proto::VideoEncoding::VIDEO_ENCODING_VP9;

static const uint32_t kSupportedAudioEncodings = 0;

static const uint32_t kSupportedFeatures =
    proto::DesktopSessionFeatures::FEATURE_CURSOR_SHAPE |
    proto::DesktopSessionFeatures::FEATURE_CLIPBOARD;

void DesktopSessionClient::Run(const std::wstring& input_channel_name,
                               const std::wstring& output_channel_name)
{
    ipc_channel_ = PipeChannel::CreateClient(input_channel_name,
                                             output_channel_name);
    if (!ipc_channel_)
        return;

    if (ipc_channel_->Connect(this))
    {
        ipc_channel_->Wait();
        clipboard_thread_.reset();
        screen_updater_.reset();
    }

    ipc_channel_.reset();
}

void DesktopSessionClient::OnPipeChannelConnect(ProcessId peer_pid)
{
    UNREF(peer_pid);
    SendConfigRequest();
}

void DesktopSessionClient::OnPipeChannelDisconnect()
{
    // Nothing
}

void DesktopSessionClient::OnPipeChannelMessage(const IOBuffer& buffer)
{
    proto::desktop::ClientToHost message;

    if (ParseMessage(buffer, message))
    {
        bool success = true;

        if (message.has_pointer_event())
        {
            ReadPointerEvent(message.pointer_event());
        }
        else if (message.has_key_event())
        {
            ReadKeyEvent(message.key_event());
        }
        else if (message.has_power_event())
        {
            ReadPowerEvent(message.power_event());
        }
        else if (message.has_clipboard_event())
        {
            std::shared_ptr<proto::ClipboardEvent> clipboard_event(
                message.release_clipboard_event());

            ReadClipboardEvent(clipboard_event);
        }
        else if (message.has_config())
        {
            success = ReadConfig(message.config());
        }
        else
        {
            // Unknown messages are ignored.
            DLOG(WARNING) << "Unhandled message from client";
        }

        if (success)
            return;
    }

    ipc_channel_->Close();
}

void DesktopSessionClient::OnScreenUpdate(const DesktopFrame* screen_frame)
{
    DCHECK(video_encoder_);

    std::unique_ptr<proto::VideoPacket> packet =
        video_encoder_->Encode(screen_frame);

    if (packet)
    {
        proto::desktop::HostToClient message;
        message.set_allocated_video_packet(packet.release());
        WriteMessage(message);
    }
}

void DesktopSessionClient::OnCursorUpdate(std::unique_ptr<MouseCursor> mouse_cursor)
{
    DCHECK(cursor_encoder_);

    std::unique_ptr<proto::CursorShape> cursor_shape =
        cursor_encoder_->Encode(std::move(mouse_cursor));

    if (cursor_shape)
    {
        proto::desktop::HostToClient message;
        message.set_allocated_cursor_shape(cursor_shape.release());
        WriteMessage(message);
    }
}

void DesktopSessionClient::OnScreenUpdateError()
{
    ipc_channel_->Close();
}

void DesktopSessionClient::WriteMessage(const proto::desktop::HostToClient& message)
{
    IOBuffer buffer(SerializeMessage(message));
    std::unique_lock<std::mutex> lock(outgoing_lock_);
    ipc_channel_->Send(buffer);
}

void DesktopSessionClient::ReadPointerEvent(const proto::PointerEvent& event)
{
    if (!input_injector_)
        input_injector_.reset(new InputInjector());

    input_injector_->InjectPointerEvent(event);
}

void DesktopSessionClient::ReadKeyEvent(const proto::KeyEvent& event)
{
    if (!input_injector_)
        input_injector_.reset(new InputInjector());

    input_injector_->InjectKeyEvent(event);
}

void DesktopSessionClient::ReadClipboardEvent(std::shared_ptr<proto::ClipboardEvent> clipboard_event)
{
    if (!clipboard_thread_)
        return;

    clipboard_thread_->InjectClipboardEvent(clipboard_event);
}

void DesktopSessionClient::ReadPowerEvent(const proto::PowerEvent& event)
{
    InjectPowerEvent(event);
}

void DesktopSessionClient::SendClipboardEvent(std::unique_ptr<proto::ClipboardEvent> clipboard_event)
{
    proto::desktop::HostToClient message;
    message.set_allocated_clipboard_event(clipboard_event.release());
    WriteMessage(message);
}

void DesktopSessionClient::SendConfigRequest()
{
    proto::desktop::HostToClient message;

    proto::DesktopSessionConfigRequest* request =
        message.mutable_config_request();

    request->set_video_encodings(kSupportedVideoEncodings);
    request->set_audio_encodings(kSupportedAudioEncodings);
    request->set_features(kSupportedFeatures);

    WriteMessage(message);
}

bool DesktopSessionClient::ReadConfig(const proto::DesktopSessionConfig& config)
{
    screen_updater_.reset(new ScreenUpdater());

    switch (config.video_encoding())
    {
        case proto::VIDEO_ENCODING_VP8:
            video_encoder_ = VideoEncoderVPX::CreateVP8();
            break;

        case proto::VIDEO_ENCODING_VP9:
            video_encoder_ = VideoEncoderVPX::CreateVP9();
            break;

        case proto::VIDEO_ENCODING_ZLIB:
            video_encoder_ =
                VideoEncoderZLIB::Create(ConvertFromVideoPixelFormat(config.pixel_format()),
                                                                     config.compress_ratio());
            break;

        default:
            LOG(ERROR) << "Unsupported video encoding: " << config.video_encoding();
            video_encoder_.reset();
            break;
    }

    if (!video_encoder_)
        return false;

    ScreenUpdater::Mode mode;

    if (config.flags() & proto::DesktopSessionConfig::ENABLE_CURSOR_SHAPE)
    {
        mode = ScreenUpdater::Mode::SCREEN_AND_CURSOR;
        cursor_encoder_.reset(new CursorEncoder());
    }
    else
    {
        mode = ScreenUpdater::Mode::SCREEN_ONLY;
        cursor_encoder_.reset();
    }

    if (!screen_updater_->StartUpdating(mode,
                                        std::chrono::milliseconds(config.update_interval()),
                                        this))
        return false;

    if (config.flags() & proto::DesktopSessionConfig::ENABLE_CLIPBOARD)
    {
        clipboard_thread_.reset(new ClipboardThread());

        Clipboard::ClipboardEventCallback clipboard_event_callback =
            std::bind(&DesktopSessionClient::SendClipboardEvent, this, std::placeholders::_1);

        clipboard_thread_->Start(std::move(clipboard_event_callback));
    }
    else
    {
        clipboard_thread_.reset();
    }

    return true;
}

} // namespace aspia
