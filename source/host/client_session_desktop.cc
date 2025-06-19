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

#include <QCoreApplication>

#include "base/logging.h"
#include "base/power_controller.h"
#include "base/codec/audio_encoder.h"
#include "base/codec/cursor_encoder.h"
#include "base/codec/scale_reducer.h"
#include "base/codec/video_encoder_vpx.h"
#include "base/codec/video_encoder_zstd.h"
#include "base/desktop/frame.h"
#include "base/desktop/screen_capturer.h"
#include "common/desktop_session_constants.h"
#include "host/desktop_session_manager.h"
#include "host/service_constants.h"
#include "host/host_storage.h"
#include "proto/desktop_internal.h"
#include "proto/task_manager.h"
#include "proto/text_chat.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/safe_mode_util.h"
#include "host/system_info.h"
#include "host/win/updater_launcher.h"
#endif // defined(Q_OS_WINDOWS)

namespace host {

//--------------------------------------------------------------------------------------------------
ClientSessionDesktop::ClientSessionDesktop(proto::peer::SessionType session_type,
                                           base::TcpChannel* channel,
                                           QObject* parent)
    : ClientSession(session_type, channel, parent),
      overflow_detection_timer_(new QTimer(this)),
      stat_counter_(id())
{
    LOG(INFO) << "Ctor";

    connect(overflow_detection_timer_, &QTimer::timeout,
            this, &ClientSessionDesktop::onOverflowDetectionTimer);
}

//--------------------------------------------------------------------------------------------------
ClientSessionDesktop::~ClientSessionDesktop()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::onStarted()
{
    max_fps_ = DesktopSessionManager::maxCaptureFps();

    if (!qEnvironmentVariableIsSet("ASPIA_NO_OVERFLOW_DETECTION"))
    {
        LOG(INFO) << "Overflow detection enabled (current FPS:"
                  << qApp->property("SCREEN_CAPTURE_FPS").toInt()
                  << ", max FPS:" << max_fps_ << ")";

        overflow_detection_timer_->start(std::chrono::milliseconds(1000));
    }
    else
    {
        LOG(INFO) << "Overflow detection disabled by environment variable (current FPS:"
                  << qApp->property("SCREEN_CAPTURE_FPS").toInt() << ")";
    }

    const char* extensions;

    // Supported extensions are different for managing and viewing the desktop.
    if (sessionType() == proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
    {
        extensions = common::kSupportedExtensionsForManage;
    }
    else
    {
        DCHECK_EQ(sessionType(), proto::peer::SESSION_TYPE_DESKTOP_VIEW);
        extensions = common::kSupportedExtensionsForView;
    }

    // Create a capabilities.
    proto::desktop::Capabilities* capabilities = outgoing_message_.newMessage().mutable_capabilities();

    // Add supported extensions and video encodings.
    capabilities->set_extensions(extensions);
    capabilities->set_video_encodings(common::kSupportedVideoEncodings);
    capabilities->set_audio_encodings(common::kSupportedAudioEncodings);

#if defined(Q_OS_WINDOWS)
    capabilities->set_os_type(proto::desktop::Capabilities::OS_TYPE_WINDOWS);
#elif defined(Q_OS_LINUX)
    capabilities->set_os_type(proto::desktop::Capabilities::OS_TYPE_LINUX);

    auto add_flag = [capabilities](const char* name, bool value)
    {
        proto::desktop::Capabilities::Flag* flag = capabilities->add_flag();
        flag->set_name(name);
        flag->set_value(value);
    };

    add_flag(common::kFlagDisablePasteAsKeystrokes, true);
    add_flag(common::kFlagDisableAudio, true);
    add_flag(common::kFlagDisableBlockInput, true);
    add_flag(common::kFlagDisableDesktopEffects, true);
    add_flag(common::kFlagDisableDesktopWallpaper, true);
    add_flag(common::kFlagDisableLockAtDisconnect, true);
    add_flag(common::kFlagDisableFontSmoothing, true);
#elif defined(Q_OS_MACOS)
    capabilities->set_os_type(proto::DesktopCapabilities::OS_TYPE_MACOSX);
#else
#warning Not implemented
#endif

    LOG(INFO) << "Sending config request";
    LOG(INFO) << "Supported extensions:" << capabilities->extensions();
    LOG(INFO) << "Supported video encodings:" << capabilities->video_encodings();
    LOG(INFO) << "Supported audio encodings:" << capabilities->audio_encodings();
    LOG(INFO) << "OS type:" << capabilities->os_type();

    // Send the request.
    sendMessage(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::onReceived(const QByteArray& buffer)
{
    if (!incoming_message_.parse(buffer))
    {
        LOG(ERROR) << "Invalid message from client";
        return;
    }

    if (incoming_message_->has_mouse_event())
    {
        if (sessionType() != proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
        {
            LOG(ERROR) << "Mouse event for non-desktop-manage session";
            return;
        }

        if (!scale_reducer_)
        {
            LOG(ERROR) << "Scale reducer NOT initialized";
            return;
        }

        const proto::desktop::MouseEvent& mouse_event = incoming_message_->mouse_event();

        int pos_x = static_cast<int>(
            static_cast<double>(mouse_event.x() * 100) / scale_reducer_->scaleFactorX());
        int pos_y = static_cast<int>(
            static_cast<double>(mouse_event.y() * 100) / scale_reducer_->scaleFactorY());

        proto::desktop::MouseEvent out_mouse_event;
        out_mouse_event.set_mask(mouse_event.mask());
        out_mouse_event.set_x(pos_x);
        out_mouse_event.set_y(pos_y);

        emit sig_injectMouseEvent(out_mouse_event);
        stat_counter_.addMouseEvent();
    }
    else if (incoming_message_->has_key_event())
    {
        if (sessionType() == proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
        {
            emit sig_injectKeyEvent(incoming_message_->key_event());
            stat_counter_.addKeyboardEvent();
        }
        else
        {
            LOG(ERROR) << "Key event for non-desktop-manage session";
        }
    }
    else if (incoming_message_->has_touch_event())
    {
        if (sessionType() == proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
        {
            emit sig_injectTouchEvent(incoming_message_->touch_event());
            stat_counter_.addTouchEvent();
        }
        else
        {
            LOG(ERROR) << "Touch event for non-desktop-manage session";
        }
    }
    else if (incoming_message_->has_text_event())
    {
        if (sessionType() == proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
        {
            emit sig_injectTextEvent(incoming_message_->text_event());
            stat_counter_.addTextEvent();
        }
        else
        {
            LOG(ERROR) << "Text event for non-desktop-manage session";
        }
    }
    else if (incoming_message_->has_clipboard_event())
    {
        if (sessionType() == proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
        {
            emit sig_injectClipboardEvent(incoming_message_->clipboard_event());
            stat_counter_.addIncomingClipboardEvent();
        }
        else
        {
            LOG(ERROR) << "Clipboard event for non-desktop-manage session";
        }
    }
    else if (incoming_message_->has_extension())
    {
        readExtension(incoming_message_->extension());
    }
    else if (incoming_message_->has_config())
    {
        readConfig(incoming_message_->config());
    }
    else
    {
        LOG(ERROR) << "Unhandled message from client";
        return;
    }
}

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::onTaskManagerMessage(const proto::task_manager::HostToClient& message)
{
    proto::desktop::Extension* extension = outgoing_message_.newMessage().mutable_extension();
    extension->set_name(common::kTaskManagerExtension);
    extension->set_data(message.SerializeAsString());

    sendMessage(outgoing_message_.serialize());
}
#endif // defined(Q_OS_WINDOWS)

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::encodeScreen(const base::Frame* frame, const base::MouseCursor* cursor)
{
    if (critical_overflow_)
        return;

    proto::desktop::HostToClient& message = outgoing_message_.newMessage();

    if (!is_video_paused_ && frame && video_encoder_)
    {
        DCHECK(scale_reducer_);

        if (source_size_ != frame->size())
        {
            // Every time we change the resolution, we have to reset the preferred size.
            source_size_ = frame->size();
            preferred_size_ = QSize(0, 0);
            forced_size_ = QSize(0, 0);
        }

        QSize current_size = preferred_size_;

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
            LOG(ERROR) << "No scaled frame";
            return;
        }

        proto::desktop::VideoPacket* packet = message.mutable_video_packet();

        // Encode the frame into a video packet.
        if (!video_encoder_->encode(scaled_frame, packet))
        {
            LOG(ERROR) << "Unable to encode video packet";
            return;
        }

        if (packet->has_format())
        {
            proto::desktop::VideoPacketFormat* format = packet->mutable_format();

            // In video packets that contain the format, we pass the screen capture type.
            format->set_capturer_type(frame->capturerType());

            // Real screen size.
            proto::desktop::Size* screen_size = format->mutable_screen_size();
            screen_size->set_width(frame->size().width());
            screen_size->set_height(frame->size().height());

            LOG(INFO) << "Video packet has format";
            LOG(INFO) << "Capturer type:" << static_cast<base::ScreenCapturer::Type>(frame->capturerType());
            LOG(INFO) << "Screen size:" << screen_size;
            LOG(INFO) << "Video size:" << format->video_rect();
        }
    }

    if (cursor && cursor_encoder_)
    {
        if (!cursor_encoder_->encode(*cursor, message.mutable_cursor_shape()))
            message.clear_cursor_shape();
    }

    if (message.has_video_packet() || message.has_cursor_shape())
    {
        sendMessage(outgoing_message_.serialize());

        if (message.has_video_packet())
        {
            video_encoder_->setEncodeBuffer(
                std::move(*message.mutable_video_packet()->mutable_data()));
            stat_counter_.addVideoPacket();
        }
    }
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::encodeAudio(const proto::desktop::AudioPacket& audio_packet)
{
    if (critical_overflow_)
        return;

    if (is_audio_paused_ || !audio_encoder_)
        return;

    if (!audio_encoder_->encode(audio_packet, outgoing_message_.newMessage().mutable_audio_packet()))
        return;

    sendMessage(outgoing_message_.serialize());
    stat_counter_.addAudioPacket();
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::setVideoErrorCode(proto::desktop::VideoErrorCode error_code)
{
    CHECK_NE(error_code, proto::desktop::VIDEO_ERROR_CODE_OK);

    outgoing_message_.newMessage().mutable_video_packet()->set_error_code(error_code);
    sendMessage(outgoing_message_.serialize());
    stat_counter_.addVideoError();
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::setCursorPosition(const proto::desktop::CursorPosition& cursor_position)
{
    if (!desktop_session_config_.cursor_position)
        return;

    int pos_x = static_cast<int>(
        static_cast<double>(cursor_position.x()) * scale_reducer_->scaleFactorX() / 100.0);
    int pos_y = static_cast<int>(
        static_cast<double>(cursor_position.y()) * scale_reducer_->scaleFactorY() / 100.0);

    proto::desktop::CursorPosition* position = outgoing_message_.newMessage().mutable_cursor_position();
    position->set_x(pos_x);
    position->set_y(pos_y);

    sendMessage(outgoing_message_.serialize());
    stat_counter_.addCursorPosition();
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::setScreenList(const proto::desktop::ScreenList& list)
{
    LOG(INFO) << "Send screen list to client";

    proto::desktop::Extension* extension = outgoing_message_.newMessage().mutable_extension();
    extension->set_name(common::kSelectScreenExtension);
    extension->set_data(list.SerializeAsString());

    sendMessage(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::setScreenType(const proto::desktop::ScreenType& type)
{
    LOG(INFO) << "Send screen type to client";

    proto::desktop::Extension* extension = outgoing_message_.newMessage().mutable_extension();
    extension->set_name(common::kScreenTypeExtension);
    extension->set_data(type.SerializeAsString());

    sendMessage(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::injectClipboardEvent(const proto::desktop::ClipboardEvent& event)
{
    if (sessionType() == proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
    {
        outgoing_message_.newMessage().mutable_clipboard_event()->CopyFrom(event);
        sendMessage(outgoing_message_.serialize());
        stat_counter_.addOutgoingClipboardEvent();
    }
    else
    {
        LOG(ERROR) << "Clipboard event can only be handled in a desktop manage session";
    }
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::readExtension(const proto::desktop::Extension& extension)
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
        LOG(ERROR) << "Unknown extension:" << extension.name();
    }
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::readConfig(const proto::desktop::Config& config)
{
    switch (config.video_encoding())
    {
        case proto::desktop::VIDEO_ENCODING_VP8:
            video_encoder_ = base::VideoEncoderVPX::createVP8();
            break;

        case proto::desktop::VIDEO_ENCODING_VP9:
            video_encoder_ = base::VideoEncoderVPX::createVP9();
            break;

        case proto::desktop::VIDEO_ENCODING_ZSTD:
            video_encoder_ = base::VideoEncoderZstd::create(
                base::PixelFormat::fromProto(config.pixel_format()), static_cast<int>(config.compress_ratio()));
            break;

        default:
        {
            // No supported video encoding.
            LOG(ERROR) << "Unsupported video encoding:" << config.video_encoding();
        }
        break;
    }

    if (!video_encoder_)
    {
        LOG(ERROR) << "Video encoder not initialized!";
        return;
    }

    switch (config.audio_encoding())
    {
        case proto::desktop::AUDIO_ENCODING_OPUS:
            audio_encoder_ = std::make_unique<base::AudioEncoder>();
            break;

        default:
        {
            LOG(ERROR) << "Unsupported audio encoding:" << config.audio_encoding();
            audio_encoder_.reset();
        }
        break;
    }

    cursor_encoder_.reset();
    if (config.flags() & proto::desktop::ENABLE_CURSOR_SHAPE)
    {
        LOG(INFO) << "Cursor shape enabled. Init cursor encoder";
        cursor_encoder_ = std::make_unique<base::CursorEncoder>();
    }

    scale_reducer_ = std::make_unique<base::ScaleReducer>();

    desktop_session_config_.disable_font_smoothing =
        (config.flags() & proto::desktop::DISABLE_FONT_SMOOTHING);
    desktop_session_config_.disable_effects =
        (config.flags() & proto::desktop::DISABLE_EFFECTS);
    desktop_session_config_.disable_wallpaper =
        (config.flags() & proto::desktop::DISABLE_WALLPAPER);
    desktop_session_config_.block_input =
        (config.flags() & proto::desktop::BLOCK_REMOTE_INPUT);
    desktop_session_config_.lock_at_disconnect =
        (config.flags() & proto::desktop::LOCK_AT_DISCONNECT);
    desktop_session_config_.clear_clipboard =
        (config.flags() & proto::desktop::CLEAR_CLIPBOARD);
    desktop_session_config_.cursor_position =
        (config.flags() & proto::desktop::CURSOR_POSITION);

    LOG(INFO) << "Client configuration changed";
    LOG(INFO) << "Video encoding:" << config.video_encoding();
    LOG(INFO) << "Enable cursor shape:" << (cursor_encoder_ != nullptr);
    LOG(INFO) << "Disable font smoothing:" << desktop_session_config_.disable_font_smoothing;
    LOG(INFO) << "Disable desktop effects:" << desktop_session_config_.disable_effects;
    LOG(INFO) << "Disable desktop wallpaper:" << desktop_session_config_.disable_wallpaper;
    LOG(INFO) << "Block input:" << desktop_session_config_.block_input;
    LOG(INFO) << "Lock at disconnect:" << desktop_session_config_.lock_at_disconnect;
    LOG(INFO) << "Clear clipboard:" << desktop_session_config_.clear_clipboard;
    LOG(INFO) << "Cursor position:" << desktop_session_config_.cursor_position;

    emit sig_clientSessionConfigured();
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::readSelectScreenExtension(const std::string& data)
{
    LOG(INFO) << "Select screen request";

    proto::desktop::Screen screen;

    if (!screen.ParseFromString(data))
    {
        LOG(ERROR) << "Unable to parse select screen extension data";
        return;
    }

    emit sig_selectScreen(screen);
    preferred_size_ = QSize(0, 0);
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::readPreferredSizeExtension(const std::string& data)
{
    proto::desktop::Size preferred_size;

    if (!preferred_size.ParseFromString(data))
    {
        LOG(ERROR) << "Unable to parse preferred size extension data";
        return;
    }

    static const int kMaxScreenSize = std::numeric_limits<qint16>::max();

    if (preferred_size.width() < 0 || preferred_size.width() > kMaxScreenSize ||
        preferred_size.height() < 0 || preferred_size.height() > kMaxScreenSize)
    {
        LOG(ERROR) << "Invalid preferred size:" << preferred_size;
        return;
    }

    LOG(INFO) << "Preferred size changed:" << preferred_size;

    preferred_size_ = base::parse(preferred_size);
    emit sig_captureScreen();
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::readVideoPauseExtension(const std::string& data)
{
    proto::desktop::Pause pause;

    if (!pause.ParseFromString(data))
    {
        LOG(ERROR) << "Unable to parse video pause extension data";
        return;
    }

    is_video_paused_ = pause.enable();
    LOG(INFO) << "Video paused:" << is_video_paused_;

    if (!is_video_paused_)
    {
        if (!video_encoder_)
        {
            LOG(ERROR) << "Video encoder not initialized";
            return;
        }

        video_encoder_->setKeyFrameRequired(true);
    }
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::readAudioPauseExtension(const std::string& data)
{
    proto::desktop::Pause pause;

    if (!pause.ParseFromString(data))
    {
        LOG(ERROR) << "Unable to parse pause extension data";
        return;
    }

    is_audio_paused_ = pause.enable();
    LOG(INFO) << "Audio paused:" << is_audio_paused_;
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::readPowerControlExtension(const std::string& data)
{
    if (sessionType() != proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
    {
        LOG(ERROR) << "Power management is only accessible from a desktop manage session";
        return;
    }

    proto::desktop::PowerControl power_control;

    if (!power_control.ParseFromString(data))
    {
        LOG(ERROR) << "Unable to parse power control extension data";
        return;
    }

    switch (power_control.action())
    {
        case proto::desktop::PowerControl::ACTION_SHUTDOWN:
        {
            LOG(INFO) << "SHUTDOWN command";

            if (!base::PowerController::shutdown())
            {
                LOG(ERROR) << "Unable to shutdown";
            }
        }
        break;

        case proto::desktop::PowerControl::ACTION_REBOOT:
        {
            LOG(INFO) << "REBOOT command";

            if (!base::PowerController::reboot())
            {
                LOG(ERROR) << "Unable to reboot";
            }
        }
        break;

        case proto::desktop::PowerControl::ACTION_REBOOT_SAFE_MODE:
        {
#if defined(Q_OS_WINDOWS)
            LOG(INFO) << "REBOOT_SAFE_MODE command";

            if (base::SafeModeUtil::setSafeModeService(kHostServiceName, true))
            {
                LOG(INFO) << "Service added successfully to start in safe mode";

                HostStorage storage;
                storage.setBootToSafeMode(true);

                if (base::SafeModeUtil::setSafeMode(true))
                {
                    LOG(INFO) << "Safe Mode boot enabled successfully";

                    if (!base::PowerController::reboot())
                    {
                        LOG(ERROR) << "Unable to reboot";
                    }
                }
                else
                {
                    LOG(ERROR) << "Failed to enable boot in Safe Mode";
                }
            }
            else
            {
                LOG(ERROR) << "Failed to add service to start in safe mode";
            }
#endif // defined(Q_OS_WINDOWS)
        }
        break;

        case proto::desktop::PowerControl::ACTION_LOGOFF:
        {
            LOG(INFO) << "LOGOFF command";
            emit sig_control(proto::internal::DesktopControl::LOGOFF);
        }
        break;

        case proto::desktop::PowerControl::ACTION_LOCK:
        {
            LOG(INFO) << "LOCK command";
            emit sig_control(proto::internal::DesktopControl::LOCK);
        }
        break;

        default:
            LOG(ERROR) << "Unhandled power control action:" << power_control.action();
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::readRemoteUpdateExtension(const std::string& /* data */)
{
#if defined(Q_OS_WINDOWS)
    LOG(INFO) << "Remote update requested";

    if (sessionType() == proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
    {
        launchUpdater(sessionId());
    }
    else
    {
        LOG(ERROR) << "Update can only be launched from a desktop manage session";
    }
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::readSystemInfoExtension(const std::string& data)
{
#if defined(Q_OS_WINDOWS)
    proto::system_info::SystemInfoRequest system_info_request;

    if (!data.empty())
    {
        if (!system_info_request.ParseFromString(data))
        {
            LOG(ERROR) << "Unable to parse system info request";
        }
    }

    proto::system_info::SystemInfo system_info;
    createSystemInfo(system_info_request, &system_info);

    proto::desktop::Extension* desktop_extension = outgoing_message_.newMessage().mutable_extension();
    desktop_extension->set_name(common::kSystemInfoExtension);
    desktop_extension->set_data(system_info.SerializeAsString());

    sendMessage(outgoing_message_.serialize());
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::readVideoRecordingExtension(const std::string& data)
{
    proto::desktop::VideoRecording video_recording;

    if (!video_recording.ParseFromString(data))
    {
        LOG(ERROR) << "Unable to parse video recording extension data";
        return;
    }

    bool started;

    switch (video_recording.action())
    {
        case proto::desktop::VideoRecording::ACTION_STARTED:
            started = true;
        break;

        case proto::desktop::VideoRecording::ACTION_STOPPED:
            started = false;
            break;

        default:
            LOG(ERROR) << "Unknown video recording action:" << video_recording.action();
            return;
    }

    emit sig_clientSessionVideoRecording(computerName(), userName(), started);
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::readTaskManagerExtension(const std::string& data)
{
#if defined(Q_OS_WINDOWS)
    proto::task_manager::ClientToHost message;

    if (!message.ParseFromString(data))
    {
        LOG(ERROR) << "Unable to parse task manager extension data";
        return;
    }

    if (!task_manager_)
    {
        task_manager_ = new TaskManager(this);

        connect(task_manager_, &TaskManager::sig_taskManagerMessage,
                this, &ClientSessionDesktop::onTaskManagerMessage);
    }

    task_manager_->readMessage(message);
#endif // defined(Q_OS_WINDOWS)
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

        LOG(INFO) << "Critical overflow:" << pending << "(" << write_overflow_count_ << ")";

        downStepOverflow();
    }
    else if (pending > kWarningPendingCount)
    {
        write_normal_count_ = 0;
        ++write_overflow_count_;

        LOG(INFO) << "Overflow:" << pending << "(" << write_overflow_count_ << ")";

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

        LOG(INFO) << "Overflow finished:" << pending;
    }
    else if (write_normal_count_ > 0)
    {
        ++write_normal_count_;

        // Trying to raise the maximum FPS every 30 seconds.
        if ((write_normal_count_ % 30) == 0)
        {
            int new_max_fps = std::min(max_fps_ + 1, DesktopSessionManager::maxCaptureFps());
            if (new_max_fps != max_fps_)
            {
                LOG(INFO) << "Max FPS:" << max_fps_ << "to" << new_max_fps;
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
    int fps = qApp->property("SCREEN_CAPTURE_FPS").toInt();
    int new_fps = fps;
    if (fps > DesktopSessionManager::minCaptureFps())
    {
        if (fps >= 25)
            new_fps = 20;
        else if (fps >= 20)
            new_fps = 15;
        else
            new_fps = fps - 1;

        if (critical_overflow_ && new_fps != max_fps_)
        {
            LOG(INFO) << "Max FPS:" << max_fps_ << "to" << new_fps;
            max_fps_ = new_fps;
        }

        if (new_fps != fps)
        {
            LOG(INFO) << "FPS:" << fps << "to" << new_fps;
            emit sig_captureFpsChanged(new_fps);
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
        QSize new_forced_size;

        if (new_fps < 10)
            new_forced_size = QSize(source_size_.width() * 70 / 100, source_size_.height() * 70 / 100);
        else if (new_fps < 15)
            new_forced_size = QSize(source_size_.width() * 80 / 100, source_size_.height() * 80 / 100);
        else
            new_forced_size = QSize(source_size_.width() * 90 / 100, source_size_.height() * 90 / 100);

        if (new_forced_size != forced_size_)
        {
            LOG(INFO) << "Forced size:" << forced_size_ << "to" << new_forced_size;
            forced_size_ = new_forced_size;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void ClientSessionDesktop::upStepOverflow()
{
    int fps = qApp->property("SCREEN_CAPTURE_FPS").toInt();
    int new_fps = fps;
    if (fps < max_fps_)
    {
        new_fps = fps + 1;

        LOG(INFO) << "FPS:" << fps << "to" << new_fps;
        emit sig_captureFpsChanged(new_fps);
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
        QSize new_forced_size;

        if (new_fps >= 25)
            new_forced_size = QSize(0, 0);
        else if (new_fps >= 20)
            new_forced_size = QSize(source_size_.width() * 90 / 100, source_size_.height() * 90 / 100);
        else if (new_fps >= 15)
            new_forced_size = QSize(source_size_.width() * 80 / 100, source_size_.height() * 80 / 100);
        else
            new_forced_size = forced_size_;

        if (new_forced_size != forced_size_)
        {
            LOG(INFO) << "Forced size:" << forced_size_ << "to" << new_forced_size;
            forced_size_ = new_forced_size;
        }
    }
}

} // namespace host
