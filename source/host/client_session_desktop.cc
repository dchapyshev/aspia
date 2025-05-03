//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/client_session_desktop.h"

#include "base/logging.h"
#include "base/power_controller.h"
#include "base/serialization.h"
#include "base/version_constants.h"
#include "base/codec/audio_encoder_opus.h"
#include "base/codec/cursor_encoder.h"
#include "base/codec/scale_reducer.h"
#include "base/codec/video_encoder_vpx.h"
#include "base/codec/video_encoder_zstd.h"
#include "base/desktop/frame.h"
#include "base/desktop/screen_capturer.h"
#include "common/desktop_session_constants.h"
#include "host/desktop_session_proxy.h"
#include "host/service_constants.h"
#include "host/system_settings.h"
#include "proto/desktop_internal.pb.h"
#include "proto/task_manager.h"
#include "proto/text_chat.h"

#if defined(OS_WIN)
#include "base/win/safe_mode_util.h"
#include "host/system_info.h"
#include "host/win/updater_launcher.h"
#endif // defined(OS_WIN)

namespace host {

namespace {

//--------------------------------------------------------------------------------------------------
base::PixelFormat parsePixelFormat(const proto::PixelFormat& format)
{
    return base::PixelFormat(
        static_cast<quint8>(format.bits_per_pixel()),
        static_cast<quint16>(format.red_max()),
        static_cast<quint16>(format.green_max()),
        static_cast<quint16>(format.blue_max()),
        static_cast<quint8>(format.red_shift()),
        static_cast<quint8>(format.green_shift()),
        static_cast<quint8>(format.blue_shift()));
}

} // namespace

//--------------------------------------------------------------------------------------------------
ClientSessionDesktop::ClientSessionDesktop(proto::SessionType session_type,
                                           std::unique_ptr<base::TcpChannel> channel,
                                           QObject* parent)
    : ClientSession(session_type, std::move(channel), parent),
      overflow_detection_timer_(new QTimer(this)),
      stat_counter_(id())
{
    LOG(LS_INFO) << "Ctor";

    connect(overflow_detection_timer_, &QTimer::timeout,
            this, &ClientSessionDesktop::onOverflowDetectionTimer);
}

//--------------------------------------------------------------------------------------------------
ClientSessionDesktop::~ClientSessionDesktop()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::setDesktopSessionProxy(
    base::local_shared_ptr<DesktopSessionProxy> desktop_session_proxy)
{
    desktop_session_proxy_ = std::move(desktop_session_proxy);
    DCHECK(desktop_session_proxy_);
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::onStarted()
{
    max_fps_ = desktop_session_proxy_->maxScreenCaptureFps();

    if (!qEnvironmentVariableIsSet("ASPIA_NO_OVERFLOW_DETECTION"))
    {
        LOG(LS_INFO) << "Overflow detection enabled (current FPS: "
                     << desktop_session_proxy_->screenCaptureFps()
                     << ", max FPS: " << max_fps_ << ")";

        overflow_detection_timer_->start(std::chrono::milliseconds(1000));
    }
    else
    {
        LOG(LS_INFO) << "Overflow detection disabled by environment variable (current FPS: "
                     << desktop_session_proxy_->screenCaptureFps() << ")";
    }

    const char* extensions;

    // Supported extensions are different for managing and viewing the desktop.
    if (sessionType() == proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        extensions = common::kSupportedExtensionsForManage;
    }
    else
    {
        DCHECK_EQ(sessionType(), proto::SESSION_TYPE_DESKTOP_VIEW);
        extensions = common::kSupportedExtensionsForView;
    }

    // Create a capabilities.
    proto::DesktopCapabilities* capabilities = outgoing_message_.mutable_capabilities();

    // Add supported extensions and video encodings.
    capabilities->set_extensions(extensions);
    capabilities->set_video_encodings(common::kSupportedVideoEncodings);
    capabilities->set_audio_encodings(common::kSupportedAudioEncodings);

    auto add_flag = [capabilities](const char* name, bool value)
    {
        proto::DesktopCapabilities::Flag* flag = capabilities->add_flag();
        flag->set_name(name);
        flag->set_value(value);
    };

#if defined(OS_WIN)
    capabilities->set_os_type(proto::DesktopCapabilities::OS_TYPE_WINDOWS);
#elif defined(OS_LINUX)
    capabilities->set_os_type(proto::DesktopCapabilities::OS_TYPE_LINUX);

    add_flag(common::kFlagDisablePasteAsKeystrokes, true);
    add_flag(common::kFlagDisableAudio, true);
    add_flag(common::kFlagDisableBlockInput, true);
    add_flag(common::kFlagDisableDesktopEffects, true);
    add_flag(common::kFlagDisableDesktopWallpaper, true);
    add_flag(common::kFlagDisableLockAtDisconnect, true);
    add_flag(common::kFlagDisableFontSmoothing, true);
#elif defined(OS_MACOS)
    capabilities->set_os_type(proto::DesktopCapabilities::OS_TYPE_MACOSX);
#else
#warning Not implemented
#endif

    LOG(LS_INFO) << "Sending config request";
    LOG(LS_INFO) << "Supported extensions: " << capabilities->extensions();
    LOG(LS_INFO) << "Supported video encodings: " << capabilities->video_encodings();
    LOG(LS_INFO) << "Supported audio encodings: " << capabilities->audio_encodings();
    LOG(LS_INFO) << "OS type: " << capabilities->os_type();

    // Send the request.
    sendMessage(proto::HOST_CHANNEL_ID_SESSION, outgoing_message_);
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::onReceived(quint8 /* channel_id */, const QByteArray& buffer)
{
    incoming_message_.Clear();

    if (!base::parse(buffer, &incoming_message_))
    {
        LOG(LS_ERROR) << "Invalid message from client";
        return;
    }

    if (incoming_message_.has_mouse_event())
    {
        if (sessionType() != proto::SESSION_TYPE_DESKTOP_MANAGE)
        {
            LOG(LS_ERROR) << "Mouse event for non-desktop-manage session";
            return;
        }

        if (!scale_reducer_)
        {
            LOG(LS_ERROR) << "Scale reducer NOT initialized";
            return;
        }

        const proto::MouseEvent& mouse_event = incoming_message_.mouse_event();

        int pos_x = static_cast<int>(
            static_cast<double>(mouse_event.x() * 100) / scale_reducer_->scaleFactorX());
        int pos_y = static_cast<int>(
            static_cast<double>(mouse_event.y() * 100) / scale_reducer_->scaleFactorY());

        proto::MouseEvent out_mouse_event;
        out_mouse_event.set_mask(mouse_event.mask());
        out_mouse_event.set_x(pos_x);
        out_mouse_event.set_y(pos_y);

        desktop_session_proxy_->injectMouseEvent(out_mouse_event);
        stat_counter_.addMouseEvent();
    }
    else if (incoming_message_.has_key_event())
    {
        if (sessionType() == proto::SESSION_TYPE_DESKTOP_MANAGE)
        {
            desktop_session_proxy_->injectKeyEvent(incoming_message_.key_event());
            stat_counter_.addKeyboardEvent();
        }
        else
        {
            LOG(LS_ERROR) << "Key event for non-desktop-manage session";
        }
    }
    else if (incoming_message_.has_touch_event())
    {
        if (sessionType() == proto::SESSION_TYPE_DESKTOP_MANAGE)
        {
            desktop_session_proxy_->injectTouchEvent(incoming_message_.touch_event());
            stat_counter_.addTouchEvent();
        }
        else
        {
            LOG(LS_ERROR) << "Touch event for non-desktop-manage session";
        }
    }
    else if (incoming_message_.has_text_event())
    {
        if (sessionType() == proto::SESSION_TYPE_DESKTOP_MANAGE)
        {
            desktop_session_proxy_->injectTextEvent(incoming_message_.text_event());
            stat_counter_.addTextEvent();
        }
        else
        {
            LOG(LS_ERROR) << "Text event for non-desktop-manage session";
        }
    }
    else if (incoming_message_.has_clipboard_event())
    {
        if (sessionType() == proto::SESSION_TYPE_DESKTOP_MANAGE)
        {
            desktop_session_proxy_->injectClipboardEvent(incoming_message_.clipboard_event());
            stat_counter_.addIncomingClipboardEvent();
        }
        else
        {
            LOG(LS_ERROR) << "Clipboard event for non-desktop-manage session";
        }
    }
    else if (incoming_message_.has_extension())
    {
        readExtension(incoming_message_.extension());
    }
    else if (incoming_message_.has_config())
    {
        readConfig(incoming_message_.config());
    }
    else
    {
        LOG(LS_ERROR) << "Unhandled message from client";
        return;
    }
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::onWritten(quint8 /* channel_id */, size_t /* pending */)
{
    // Nothing
}

#if defined(OS_WIN)
//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::onTaskManagerMessage(const proto::task_manager::HostToClient& message)
{
    outgoing_message_.Clear();

    proto::DesktopExtension* extension = outgoing_message_.mutable_extension();
    extension->set_name(common::kTaskManagerExtension);
    extension->set_data(message.SerializeAsString());

    sendMessage(proto::HOST_CHANNEL_ID_SESSION, outgoing_message_);
}
#endif // defined(OS_WIN)

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::encodeScreen(const base::Frame* frame, const base::MouseCursor* cursor)
{
    if (critical_overflow_)
        return;

    outgoing_message_.Clear();

    if (!is_video_paused_ && frame && video_encoder_)
    {
        DCHECK(scale_reducer_);

        if (source_size_ != frame->size())
        {
            // Every time we change the resolution, we have to reset the preferred size.
            source_size_ = frame->size();
            preferred_size_ = base::Size();
            forced_size_ = base::Size();
        }

        base::Size current_size = preferred_size_;

        // If the preferred size is larger than the original, then we use the original size.
        if (current_size.width() > source_size_.width() ||
            current_size.height() > source_size_.height())
        {
            current_size = source_size_;
        }

        // If we don't have a preferred size, then we use the original frame size.
        if (current_size.isEmpty())
            current_size = source_size_;

        if (!forced_size_.isEmpty())
        {
            int forced = forced_size_.width() * forced_size_.height();
            int current = current_size.width() * current_size.height();

            if (forced < current)
                current_size = forced_size_;
        }

        const base::Frame* scaled_frame = scale_reducer_->scaleFrame(frame, current_size);
        if (!scaled_frame)
        {
            LOG(LS_ERROR) << "No scaled frame";
            return;
        }

        proto::VideoPacket* packet = outgoing_message_.mutable_video_packet();

        // Encode the frame into a video packet.
        if (!video_encoder_->encode(scaled_frame, packet))
        {
            LOG(LS_ERROR) << "Unable to encode video packet";
            return;
        }

        if (packet->has_format())
        {
            proto::VideoPacketFormat* format = packet->mutable_format();

            // In video packets that contain the format, we pass the screen capture type.
            format->set_capturer_type(frame->capturerType());

            // Real screen size.
            proto::Size* screen_size = format->mutable_screen_size();
            screen_size->set_width(frame->size().width());
            screen_size->set_height(frame->size().height());

            LOG(LS_INFO) << "Video packet has format";
            LOG(LS_INFO) << "Capturer type: " << base::ScreenCapturer::typeToString(
                static_cast<base::ScreenCapturer::Type>(frame->capturerType()));
            LOG(LS_INFO) << "Screen size: " << screen_size->width() << "x"
                         << screen_size->height();
            LOG(LS_INFO) << "Video size: " << format->video_rect().width() << "x"
                         << format->video_rect().height();
        }
    }

    if (cursor && cursor_encoder_)
    {
        if (!cursor_encoder_->encode(*cursor, outgoing_message_.mutable_cursor_shape()))
            outgoing_message_.clear_cursor_shape();
    }

    if (outgoing_message_.has_video_packet() || outgoing_message_.has_cursor_shape())
    {
        sendMessage(proto::HOST_CHANNEL_ID_SESSION, outgoing_message_);

        if (outgoing_message_.has_video_packet())
        {
            video_encoder_->setEncodeBuffer(
                std::move(*outgoing_message_.mutable_video_packet()->mutable_data()));
            stat_counter_.addVideoPacket();
        }
    }
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::encodeAudio(const proto::AudioPacket& audio_packet)
{
    if (critical_overflow_)
        return;

    if (is_audio_paused_ || !audio_encoder_)
        return;

    outgoing_message_.Clear();

    if (!audio_encoder_->encode(audio_packet, outgoing_message_.mutable_audio_packet()))
        return;

    sendMessage(proto::HOST_CHANNEL_ID_SESSION, outgoing_message_);
    stat_counter_.addAudioPacket();
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::setVideoErrorCode(proto::VideoErrorCode error_code)
{
    CHECK_NE(error_code, proto::VIDEO_ERROR_CODE_OK);

    if (clientVersion() < base::kVersion_2_6_0)
    {
        // Old version Client does not work with error codes.
        return;
    }

    outgoing_message_.Clear();
    outgoing_message_.mutable_video_packet()->set_error_code(error_code);
    sendMessage(proto::HOST_CHANNEL_ID_SESSION, outgoing_message_);
    stat_counter_.addVideoError();
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::setCursorPosition(const proto::CursorPosition& cursor_position)
{
    if (!desktop_session_config_.cursor_position)
        return;

    outgoing_message_.Clear();

    int pos_x = static_cast<int>(
        static_cast<double>(cursor_position.x()) * scale_reducer_->scaleFactorX() / 100.0);
    int pos_y = static_cast<int>(
        static_cast<double>(cursor_position.y()) * scale_reducer_->scaleFactorY() / 100.0);

    proto::CursorPosition* position = outgoing_message_.mutable_cursor_position();
    position->set_x(pos_x);
    position->set_y(pos_y);

    sendMessage(proto::HOST_CHANNEL_ID_SESSION, outgoing_message_);
    stat_counter_.addCursorPosition();
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::setScreenList(const proto::ScreenList& list)
{
    LOG(LS_INFO) << "Send screen list to client";

    outgoing_message_.Clear();
    proto::DesktopExtension* extension = outgoing_message_.mutable_extension();
    extension->set_name(common::kSelectScreenExtension);
    extension->set_data(list.SerializeAsString());

    sendMessage(proto::HOST_CHANNEL_ID_SESSION, outgoing_message_);
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::setScreenType(const proto::ScreenType& type)
{
    LOG(LS_INFO) << "Send screen type to client";

    outgoing_message_.Clear();
    proto::DesktopExtension* extension = outgoing_message_.mutable_extension();
    extension->set_name(common::kScreenTypeExtension);
    extension->set_data(type.SerializeAsString());

    sendMessage(proto::HOST_CHANNEL_ID_SESSION, outgoing_message_);
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::injectClipboardEvent(const proto::ClipboardEvent& event)
{
    if (sessionType() == proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        outgoing_message_.Clear();
        outgoing_message_.mutable_clipboard_event()->CopyFrom(event);
        sendMessage(proto::HOST_CHANNEL_ID_SESSION, outgoing_message_);
        stat_counter_.addOutgoingClipboardEvent();
    }
    else
    {
        LOG(LS_ERROR) << "Clipboard event can only be handled in a desktop manage session";
    }
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::readExtension(const proto::DesktopExtension& extension)
{
    if (extension.name() == common::kTaskManagerExtension)
    {
        readTaskManagerExtension(extension.data());
    }
    else if (extension.name() == common::kSelectScreenExtension)
    {
        readSelectScreenExtension(extension.data());
    }
    else if (extension.name() == common::kPreferredSizeExtension)
    {
        readPreferredSizeExtension(extension.data());
    }
    else if (extension.name() == common::kVideoPauseExtension)
    {
        readVideoPauseExtension(extension.data());
    }
    else if (extension.name() == common::kAudioPauseExtension)
    {
        readAudioPauseExtension(extension.data());
    }
    else if (extension.name() == common::kPowerControlExtension)
    {
        readPowerControlExtension(extension.data());
    }
    else if (extension.name() == common::kRemoteUpdateExtension)
    {
        readRemoteUpdateExtension(extension.data());
    }
    else if (extension.name() == common::kSystemInfoExtension)
    {
        readSystemInfoExtension(extension.data());
    }
    else if (extension.name() == common::kVideoRecordingExtension)
    {
        readVideoRecordingExtension(extension.data());
    }
    else
    {
        LOG(LS_ERROR) << "Unknown extension: " << extension.name();
    }
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::readConfig(const proto::DesktopConfig& config)
{
    switch (config.video_encoding())
    {
        case proto::VIDEO_ENCODING_VP8:
            video_encoder_ = base::VideoEncoderVPX::createVP8();
            break;

        case proto::VIDEO_ENCODING_VP9:
            video_encoder_ = base::VideoEncoderVPX::createVP9();
            break;

        case proto::VIDEO_ENCODING_ZSTD:
            video_encoder_ = base::VideoEncoderZstd::create(
                parsePixelFormat(config.pixel_format()), static_cast<int>(config.compress_ratio()));
            break;

        default:
        {
            // No supported video encoding.
            LOG(LS_ERROR) << "Unsupported video encoding: " << config.video_encoding();
        }
        break;
    }

    if (!video_encoder_)
    {
        LOG(LS_ERROR) << "Video encoder not initialized!";
        return;
    }

    switch (config.audio_encoding())
    {
        case proto::AUDIO_ENCODING_OPUS:
            audio_encoder_ = std::make_unique<base::AudioEncoderOpus>();
            break;

        default:
        {
            LOG(LS_ERROR) << "Unsupported audio encoding: " << config.audio_encoding();
            audio_encoder_.reset();
        }
        break;
    }

    cursor_encoder_.reset();
    if (config.flags() & proto::ENABLE_CURSOR_SHAPE)
    {
        LOG(LS_INFO) << "Cursor shape enabled. Init cursor encoder";
        cursor_encoder_ = std::make_unique<base::CursorEncoder>();
    }

    scale_reducer_ = std::make_unique<base::ScaleReducer>();

    desktop_session_config_.disable_font_smoothing =
        (config.flags() & proto::DISABLE_FONT_SMOOTHING);
    desktop_session_config_.disable_effects =
        (config.flags() & proto::DISABLE_DESKTOP_EFFECTS);
    desktop_session_config_.disable_wallpaper =
        (config.flags() & proto::DISABLE_DESKTOP_WALLPAPER);
    desktop_session_config_.block_input =
        (config.flags() & proto::BLOCK_REMOTE_INPUT);
    desktop_session_config_.lock_at_disconnect =
        (config.flags() & proto::LOCK_AT_DISCONNECT);
    desktop_session_config_.clear_clipboard =
        (config.flags() & proto::CLEAR_CLIPBOARD);
    desktop_session_config_.cursor_position =
        (config.flags() & proto::CURSOR_POSITION);

    LOG(LS_INFO) << "Client configuration changed";
    LOG(LS_INFO) << "Video encoding: " << config.video_encoding();
    LOG(LS_INFO) << "Enable cursor shape: " << (cursor_encoder_ != nullptr);
    LOG(LS_INFO) << "Disable font smoothing: " << desktop_session_config_.disable_font_smoothing;
    LOG(LS_INFO) << "Disable desktop effects: " << desktop_session_config_.disable_effects;
    LOG(LS_INFO) << "Disable desktop wallpaper: " << desktop_session_config_.disable_wallpaper;
    LOG(LS_INFO) << "Block input: " << desktop_session_config_.block_input;
    LOG(LS_INFO) << "Lock at disconnect: " << desktop_session_config_.lock_at_disconnect;
    LOG(LS_INFO) << "Clear clipboard: " << desktop_session_config_.clear_clipboard;
    LOG(LS_INFO) << "Cursor position: " << desktop_session_config_.cursor_position;

    delegate_->onClientSessionConfigured();
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::readSelectScreenExtension(const std::string& data)
{
    LOG(LS_INFO) << "Select screen request";

    proto::Screen screen;

    if (!screen.ParseFromString(data))
    {
        LOG(LS_ERROR) << "Unable to parse select screen extension data";
        return;
    }

    desktop_session_proxy_->selectScreen(screen);
    preferred_size_ = base::Size();
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::readPreferredSizeExtension(const std::string& data)
{
    proto::PreferredSize preferred_size;

    if (!preferred_size.ParseFromString(data))
    {
        LOG(LS_ERROR) << "Unable to parse preferred size extension data";
        return;
    }

    static const int kMaxScreenSize = std::numeric_limits<qint16>::max();

    if (preferred_size.width() < 0 || preferred_size.width() > kMaxScreenSize ||
        preferred_size.height() < 0 || preferred_size.height() > kMaxScreenSize)
    {
        LOG(LS_ERROR) << "Invalid preferred size: "
                      << preferred_size.width() << "x" << preferred_size.height();
        return;
    }

    LOG(LS_INFO) << "Preferred size changed: "
                 << preferred_size.width() << "x" << preferred_size.height();

    preferred_size_.set(preferred_size.width(), preferred_size.height());
    desktop_session_proxy_->captureScreen();
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::readVideoPauseExtension(const std::string& data)
{
    proto::Pause pause;

    if (!pause.ParseFromString(data))
    {
        LOG(LS_ERROR) << "Unable to parse video pause extension data";
        return;
    }

    is_video_paused_ = pause.enable();
    LOG(LS_INFO) << "Video paused: " << is_video_paused_;

    if (!is_video_paused_)
    {
        if (!video_encoder_)
        {
            LOG(LS_ERROR) << "Video encoder not initialized";
            return;
        }

        video_encoder_->setKeyFrameRequired(true);
    }
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::readAudioPauseExtension(const std::string& data)
{
    proto::Pause pause;

    if (!pause.ParseFromString(data))
    {
        LOG(LS_ERROR) << "Unable to parse pause extension data";
        return;
    }

    is_audio_paused_ = pause.enable();
    LOG(LS_INFO) << "Audio paused: " << is_audio_paused_;
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::readPowerControlExtension(const std::string& data)
{
    if (sessionType() != proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        LOG(LS_ERROR) << "Power management is only accessible from a desktop manage session";
        return;
    }

    proto::PowerControl power_control;

    if (!power_control.ParseFromString(data))
    {
        LOG(LS_ERROR) << "Unable to parse power control extension data";
        return;
    }

    switch (power_control.action())
    {
        case proto::PowerControl::ACTION_SHUTDOWN:
        {
            LOG(LS_INFO) << "SHUTDOWN command";

            if (!base::PowerController::shutdown())
            {
                LOG(LS_ERROR) << "Unable to shutdown";
            }
        }
        break;

        case proto::PowerControl::ACTION_REBOOT:
        {
            LOG(LS_INFO) << "REBOOT command";

            if (!base::PowerController::reboot())
            {
                LOG(LS_ERROR) << "Unable to reboot";
            }
        }
        break;

        case proto::PowerControl::ACTION_REBOOT_SAFE_MODE:
        {
#if defined(OS_WIN)
            LOG(LS_INFO) << "REBOOT_SAFE_MODE command";

            if (base::SafeModeUtil::setSafeModeService(kHostServiceName, true))
            {
                LOG(LS_INFO) << "Service added successfully to start in safe mode";

                SystemSettings settings;
                settings.setBootToSafeMode(true);

                if (base::SafeModeUtil::setSafeMode(true))
                {
                    LOG(LS_INFO) << "Safe Mode boot enabled successfully";

                    if (!base::PowerController::reboot())
                    {
                        LOG(LS_ERROR) << "Unable to reboot";
                    }
                }
                else
                {
                    LOG(LS_ERROR) << "Failed to enable boot in Safe Mode";
                }
            }
            else
            {
                LOG(LS_ERROR) << "Failed to add service to start in safe mode";
            }
#endif // defined(OS_WIN)
        }
        break;

        case proto::PowerControl::ACTION_LOGOFF:
        {
            LOG(LS_INFO) << "LOGOFF command";
            desktop_session_proxy_->control(proto::internal::DesktopControl::LOGOFF);
        }
        break;

        case proto::PowerControl::ACTION_LOCK:
        {
            LOG(LS_INFO) << "LOCK command";
            desktop_session_proxy_->control(proto::internal::DesktopControl::LOCK);
        }
        break;

        default:
            LOG(LS_ERROR) << "Unhandled power control action: " << power_control.action();
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::readRemoteUpdateExtension(const std::string& /* data */)
{
#if defined(OS_WIN)
    LOG(LS_INFO) << "Remote update requested";

    if (sessionType() == proto::SESSION_TYPE_DESKTOP_MANAGE)
    {
        launchUpdater(sessionId());
    }
    else
    {
        LOG(LS_ERROR) << "Update can only be launched from a desktop manage session";
    }
#endif // defined(OS_WIN)
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::readSystemInfoExtension(const std::string& data)
{
#if defined(OS_WIN)
    proto::system_info::SystemInfoRequest system_info_request;

    if (!data.empty())
    {
        if (!system_info_request.ParseFromString(data))
        {
            LOG(LS_ERROR) << "Unable to parse system info request";
        }
    }

    proto::system_info::SystemInfo system_info;
    createSystemInfo(system_info_request, &system_info);

    outgoing_message_.Clear();
    proto::DesktopExtension* desktop_extension = outgoing_message_.mutable_extension();
    desktop_extension->set_name(common::kSystemInfoExtension);
    desktop_extension->set_data(system_info.SerializeAsString());

    sendMessage(proto::HOST_CHANNEL_ID_SESSION, outgoing_message_);
#endif // defined(OS_WIN)
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::readVideoRecordingExtension(const std::string& data)
{
    proto::VideoRecording video_recording;

    if (!video_recording.ParseFromString(data))
    {
        LOG(LS_ERROR) << "Unable to parse video recording extension data";
        return;
    }

    bool started;

    switch (video_recording.action())
    {
        case proto::VideoRecording::ACTION_STARTED:
            started = true;
        break;

        case proto::VideoRecording::ACTION_STOPPED:
            started = false;
            break;

        default:
            LOG(LS_ERROR) << "Unknown video recording action: " << video_recording.action();
            return;
    }

    delegate_->onClientSessionVideoRecording(computerName(), userName(), started);
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::readTaskManagerExtension(const std::string& data)
{
#if defined(OS_WIN)
    proto::task_manager::ClientToHost message;

    if (!message.ParseFromString(data))
    {
        LOG(LS_ERROR) << "Unable to parse task manager extension data";
        return;
    }

    if (!task_manager_)
        task_manager_ = std::make_unique<TaskManager>(this);

    task_manager_->readMessage(message);
#endif // defined(OS_WIN)
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::onOverflowDetectionTimer()
{
    // Maximum value of messages in the queue for sending.
    static const size_t kCriticalPendingCount = 12;

    // Maximum average number of messages in the send queue.
    static const size_t kWarningPendingCount = 2;

    size_t pending = pendingMessages();
    if (pending > kCriticalPendingCount)
    {
        critical_overflow_ = true;
        write_normal_count_ = 0;
        ++write_overflow_count_;

        LOG(LS_INFO) << "Critical overflow: " << pending << " (" << write_overflow_count_ << ")";

        downStepOverflow();
    }
    else if (pending > kWarningPendingCount)
    {
        write_normal_count_ = 0;
        ++write_overflow_count_;

        LOG(LS_INFO) << "Overflow: " << pending << " (" << write_overflow_count_ << ")";

        if (pending > last_pending_count_ || write_overflow_count_ > 10)
            downStepOverflow();
    }
    else if (write_overflow_count_ > 0 && pending == 0)
    {
        if (critical_overflow_)
        {
            if (video_encoder_)
                video_encoder_->setKeyFrameRequired(true);
        }

        critical_overflow_ = false;
        write_normal_count_ = 1;
        write_overflow_count_ = 0;

        LOG(LS_INFO) << "Overflow finished: " << pending;
    }
    else if (write_normal_count_ > 0)
    {
        ++write_normal_count_;

        // Trying to raise the maximum FPS every 30 seconds.
        if ((write_normal_count_ % 30) == 0)
        {
            int new_max_fps = std::min(max_fps_ + 1, desktop_session_proxy_->maxScreenCaptureFps());
            if (new_max_fps != max_fps_)
            {
                LOG(LS_INFO) << "Max FPS: " << max_fps_ << " to " << new_max_fps;
                max_fps_ = new_max_fps;
            }
        }

        // Trying to raise the current FPS every 10 seconds.
        if ((write_normal_count_ % 10) == 0)
            upStepOverflow();
    }

    last_pending_count_ = pending;
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::downStepOverflow()
{
    int fps = desktop_session_proxy_->screenCaptureFps();
    int new_fps = fps;
    if (fps > desktop_session_proxy_->minScreenCaptureFps())
    {
        if (fps >= 25)
            new_fps = 20;
        else if (fps >= 20)
            new_fps = 15;
        else
            new_fps = fps - 1;

        if (critical_overflow_ && new_fps != max_fps_)
        {
            LOG(LS_INFO) << "Max FPS: " << max_fps_ << " to " << new_fps;
            max_fps_ = new_fps;
        }

        if (new_fps != fps)
        {
            LOG(LS_INFO) << "FPS: " << fps << " to " << new_fps;
            desktop_session_proxy_->setScreenCaptureFps(new_fps);
        }
    }

    if (new_fps < 25)
    {
        if (audio_encoder_)
        {
            static const int k96kbs = 96 * 1024;
            static const int k64kbs = 64 * 1024;
            static const int k32kbs = 32 * 1024;
            static const int k24kbs = 24 * 1024;

            int bitrate = audio_encoder_->bitrate();
            if (bitrate >= k96kbs)
                audio_encoder_->setBitrate(k64kbs);
            else if (bitrate >= k64kbs)
                audio_encoder_->setBitrate(k32kbs);
            else if (bitrate >= k32kbs)
                audio_encoder_->setBitrate(k24kbs);
        }
    }

    if (new_fps < 20)
    {
        base::Size new_forced_size;

        if (new_fps < 10)
            new_forced_size.set(source_size_.width() * 70 / 100, source_size_.height() * 70 / 100);
        else if (new_fps < 15)
            new_forced_size.set(source_size_.width() * 80 / 100, source_size_.height() * 80 / 100);
        else
            new_forced_size.set(source_size_.width() * 90 / 100, source_size_.height() * 90 / 100);

        if (new_forced_size != forced_size_)
        {
            LOG(LS_INFO) << "Forced size: " << forced_size_ << " to " << new_forced_size;
            forced_size_ = new_forced_size;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::upStepOverflow()
{
    int fps = desktop_session_proxy_->screenCaptureFps();
    int new_fps = fps;
    if (fps < max_fps_)
    {
        new_fps = fps + 1;

        LOG(LS_INFO) << "FPS: " << fps << " to " << new_fps;
        desktop_session_proxy_->setScreenCaptureFps(new_fps);
    }

    if (new_fps >= 25)
    {
        if (audio_encoder_)
        {
            static const int k96kbs = 96 * 1024;
            static const int k64kbs = 64 * 1024;
            static const int k32kbs = 32 * 1024;

            int bitrate = audio_encoder_->bitrate();
            if (bitrate < k32kbs)
                audio_encoder_->setBitrate(k32kbs);
            else if (bitrate < k64kbs)
                audio_encoder_->setBitrate(k64kbs);
            else if (bitrate < k96kbs)
                audio_encoder_->setBitrate(k96kbs);
        }
    }

    if (!forced_size_.isEmpty())
    {
        base::Size new_forced_size;

        if (new_fps >= 25)
            new_forced_size.set(0, 0);
        else if (new_fps >= 20)
            new_forced_size.set(source_size_.width() * 90 / 100, source_size_.height() * 90 / 100);
        else if (new_fps >= 15)
            new_forced_size.set(source_size_.width() * 80 / 100, source_size_.height() * 80 / 100);
        else
            new_forced_size = forced_size_;

        if (new_forced_size != forced_size_)
        {
            LOG(LS_INFO) << "Forced size: " << forced_size_ << " to " << new_forced_size;
            forced_size_ = new_forced_size;
        }
    }
}

} // namespace host
