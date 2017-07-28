//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/desktop_session_client.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/desktop_session_client.h"
#include "codec/video_encoder_zlib.h"
#include "codec/video_encoder_vpx.h"
#include "codec/video_helpers.h"
#include "ipc/pipe_channel_proxy.h"
#include "protocol/message_serialization.h"

namespace aspia {

static const uint32_t kSupportedVideoEncodings =
    proto::VideoEncoding::VIDEO_ENCODING_ZLIB |
    proto::VideoEncoding::VIDEO_ENCODING_VP8 |
    proto::VideoEncoding::VIDEO_ENCODING_VP9;

static const uint32_t kSupportedAudioEncodings = 0;

static const uint32_t kSupportedFeatures =
    proto::DesktopSessionFeatures::FEATURE_CURSOR_SHAPE |
    proto::DesktopSessionFeatures::FEATURE_CLIPBOARD;

void DesktopSessionClient::Run(const std::wstring& channel_id)
{
    ipc_channel_ = PipeChannel::CreateClient(channel_id);
    if (!ipc_channel_)
        return;

    ipc_channel_proxy_ = ipc_channel_->pipe_channel_proxy();

    uint32_t user_data = GetCurrentProcessId();

    if (ipc_channel_->Connect(user_data))
    {
        OnPipeChannelConnect(user_data);

        // Waiting for the connection to close.
        ipc_channel_proxy_->WaitForDisconnect();

        // Stop the threads.
        clipboard_thread_.reset();
        screen_updater_.reset();
    }
}

void DesktopSessionClient::OnPipeChannelConnect(uint32_t user_data)
{
    // The server sends the session type in user_data.
    session_type_ = static_cast<proto::SessionType>(user_data);

    switch (session_type_)
    {
        case proto::SessionType::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::SessionType::SESSION_TYPE_DESKTOP_VIEW:
            break;

        default:
            LOG(FATAL) << "Invalid session type passed: " << session_type_;
            return;
    }

    SendConfigRequest();

    ipc_channel_proxy_->Receive(std::bind(
        &DesktopSessionClient::OnPipeChannelMessage, this,
        std::placeholders::_1));
}

void DesktopSessionClient::OnPipeChannelMessage(
    std::unique_ptr<IOBuffer> buffer)
{
    proto::desktop::ClientToHost message;

    if (ParseMessage(*buffer, message))
    {
        bool success = true;

        if (message.has_pointer_event())
        {
            success = ReadPointerEvent(message.pointer_event());
        }
        else if (message.has_key_event())
        {
            success = ReadKeyEvent(message.key_event());
        }
        else if (message.has_clipboard_event())
        {
            std::shared_ptr<proto::ClipboardEvent> clipboard_event(
                message.release_clipboard_event());

            success = ReadClipboardEvent(clipboard_event);
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
        {
            ipc_channel_proxy_->Receive(std::bind(
                &DesktopSessionClient::OnPipeChannelMessage, this,
                std::placeholders::_1));

            return;
        }
    }

    ipc_channel_.reset();
}

void DesktopSessionClient::OnScreenUpdate(
    const DesktopFrame* screen_frame,
    std::unique_ptr<MouseCursor> mouse_cursor)
{
    DCHECK(screen_frame || mouse_cursor);
    DCHECK(video_encoder_);

    proto::desktop::HostToClient message;

    // If the screen image has changes.
    if (screen_frame)
    {
        std::unique_ptr<proto::VideoPacket> packet =
            video_encoder_->Encode(screen_frame);

        message.set_allocated_video_packet(packet.release());
    }

    // If the mouse cursor has changes.
    if (mouse_cursor)
    {
        DCHECK_EQ(session_type_, proto::SessionType::SESSION_TYPE_DESKTOP_MANAGE);
        DCHECK(cursor_encoder_);

        std::unique_ptr<proto::CursorShape> cursor_shape =
            cursor_encoder_->Encode(std::move(mouse_cursor));

        message.set_allocated_cursor_shape(cursor_shape.release());
    }

    WriteMessage(message,
                 std::bind(&DesktopSessionClient::OnScreenUpdated, this));
}

void DesktopSessionClient::OnScreenUpdated()
{
    if (!screen_updater_)
        return;

    screen_updater_->PostUpdateRequest();
}

void DesktopSessionClient::WriteMessage(
    const proto::desktop::HostToClient& message,
    PipeChannel::SendCompleteHandler handler)
{
    std::unique_ptr<IOBuffer> buffer = SerializeMessage<IOBuffer>(message);
    ipc_channel_proxy_->Send(std::move(buffer), std::move(handler));
}

void DesktopSessionClient::WriteMessage(
    const proto::desktop::HostToClient& message)
{
    WriteMessage(message, nullptr);
}

void DesktopSessionClient::WriteStatus(proto::Status status)
{
    DCHECK(status != proto::Status::STATUS_SUCCESS);

    proto::desktop::HostToClient message;
    message.set_status(status);
    WriteMessage(message);
}

bool DesktopSessionClient::ReadPointerEvent(const proto::PointerEvent& event)
{
    if (session_type_ != proto::SessionType::SESSION_TYPE_DESKTOP_MANAGE)
        return false;

    if (!input_injector_)
        input_injector_ = std::make_unique<InputInjector>();

    input_injector_->InjectPointerEvent(event);
    return true;
}

bool DesktopSessionClient::ReadKeyEvent(const proto::KeyEvent& event)
{
    if (session_type_ != proto::SessionType::SESSION_TYPE_DESKTOP_MANAGE)
        return false;

    if (!input_injector_)
        input_injector_ = std::make_unique<InputInjector>();

    input_injector_->InjectKeyEvent(event);
    return true;
}

bool DesktopSessionClient::ReadClipboardEvent(
    std::shared_ptr<proto::ClipboardEvent> clipboard_event)
{
    if (session_type_ != proto::SessionType::SESSION_TYPE_DESKTOP_MANAGE)
        return false;

    if (!clipboard_thread_)
        return false;

    clipboard_thread_->InjectClipboardEvent(clipboard_event);
    return true;
}

void DesktopSessionClient::SendClipboardEvent(
    std::unique_ptr<proto::ClipboardEvent> clipboard_event)
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

    if (session_type_ == proto::SessionType::SESSION_TYPE_DESKTOP_MANAGE)
        request->set_features(kSupportedFeatures);
    else
        request->set_features(0);

    WriteMessage(message);
}

bool DesktopSessionClient::ReadConfig(const proto::DesktopSessionConfig& config)
{
    screen_updater_.reset();

    bool enable_cursor_shape = false;

    if (session_type_ == proto::SessionType::SESSION_TYPE_DESKTOP_MANAGE)
    {
        if (config.flags() & proto::DesktopSessionConfig::ENABLE_CURSOR_SHAPE)
            enable_cursor_shape = true;

        if (enable_cursor_shape)
        {
            cursor_encoder_ = std::make_unique<CursorEncoder>();
        }
        else
        {
            cursor_encoder_.reset();
        }

        if (config.flags() & proto::DesktopSessionConfig::ENABLE_CLIPBOARD)
        {
            clipboard_thread_ = std::make_unique<ClipboardThread>();

            clipboard_thread_->Start(std::bind(
                &DesktopSessionClient::SendClipboardEvent, this,
                std::placeholders::_1));
        }
        else
        {
            clipboard_thread_.reset();
        }
    }

    switch (config.video_encoding())
    {
        case proto::VIDEO_ENCODING_VP8:
            video_encoder_ = VideoEncoderVPX::CreateVP8();
            break;

        case proto::VIDEO_ENCODING_VP9:
            video_encoder_ = VideoEncoderVPX::CreateVP9();
            break;

        case proto::VIDEO_ENCODING_ZLIB:
            video_encoder_ = VideoEncoderZLIB::Create(
                ConvertFromVideoPixelFormat(config.pixel_format()),
                                            config.compress_ratio());
            break;

        default:
            LOG(ERROR) << "Unsupported video encoding: "
                       << config.video_encoding();
            video_encoder_.reset();
            break;
    }

    if (!video_encoder_)
        return false;

    ScreenUpdater::ScreenUpdateCallback screen_update_callback =
        std::bind(&DesktopSessionClient::OnScreenUpdate,
                  this,
                  std::placeholders::_1,
                  std::placeholders::_2);

    ScreenUpdater::Mode update_mode = enable_cursor_shape ?
        ScreenUpdater::Mode::SCREEN_AND_CURSOR : ScreenUpdater::Mode::SCREEN;

    screen_updater_ = std::make_unique<ScreenUpdater>(
        update_mode,
        std::chrono::milliseconds(config.update_interval()),
        std::move(screen_update_callback));

    screen_updater_->PostUpdateRequest();

    return true;
}

} // namespace aspia
