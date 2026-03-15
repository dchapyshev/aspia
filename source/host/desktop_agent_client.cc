//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/desktop_agent_client.h"

#include <QThread>

#include "base/logging.h"
#include "base/numeric_utils.h"
#include "base/power_controller.h"
#include "base/codec/audio_encoder.h"
#include "base/codec/cursor_encoder.h"
#include "base/codec/scale_reducer.h"
#include "base/codec/video_encoder_vpx.h"
#include "base/codec/video_encoder_zstd.h"
#include "base/desktop/frame.h"
#include "base/desktop/screen_capturer.h"
#include "base/ipc/ipc_channel.h"
#include "common/desktop_session_constants.h"
#include "host/host_storage.h"
#include "host/service.h"
#include "proto/peer.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/safe_mode_util.h"
#include "host/win/system_info.h"
#include "host/win/task_manager.h"
#include "host/win/updater_launcher.h"
#endif // defined(Q_OS_WINDOWS)

namespace host {

//--------------------------------------------------------------------------------------------------
DesktopAgentClient::DesktopAgentClient(QObject* parent)
    : QObject(parent),
      ipc_channel_(new base::IpcChannel(this))
{
    LOG(INFO) << "Ctor";

    connect(ipc_channel_, &base::IpcChannel::sig_connected, this, &DesktopAgentClient::onIpcConnected);
    connect(ipc_channel_, &base::IpcChannel::sig_disconnected, this, &DesktopAgentClient::onIpcDisconnected);
    connect(ipc_channel_, &base::IpcChannel::sig_errorOccurred, this, &DesktopAgentClient::onIpcErrorOccurred);
    connect(ipc_channel_, &base::IpcChannel::sig_messageReceived, this, &DesktopAgentClient::onIpcMessageReceived);
}

//--------------------------------------------------------------------------------------------------
DesktopAgentClient::~DesktopAgentClient()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::onScreenCaptureData(const base::Frame* frame)
{
    if (is_video_paused_ || !video_encoder_ || !frame)
        return;

    if (frame->constUpdatedRegion().isEmpty() && frame_count_ > 0)
        return;

    ++frame_count_;

    if (source_size_ != frame->size())
    {
        // Every time we change the resolution, we have to reset the preferred size.
        source_size_ = frame->size();
        preferred_size_ = QSize(0, 0);
        forced_size_ = QSize(0, 0);
    }

    QSize current_size = preferred_size_;

    // If the preferred size is larger than the original, then we use the original size.
    if (current_size.width() > source_size_.width() || current_size.height() > source_size_.height())
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

    proto::desktop::SessionToClient& message = outgoing_message_.newMessage();
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

        LOG(INFO) << "Video packet has format ("
                  << "capturer:" << static_cast<base::ScreenCapturer::Type>(frame->capturerType())
                  << "screen:" << *screen_size
                  << "video:" << format->video_rect() << ")";
    }

    sendSessionMessage();
    video_encoder_->setEncodeBuffer(std::move(*message.mutable_video_packet()->mutable_data()));
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::onScreenCaptureError(proto::desktop::VideoErrorCode error_code)
{
    proto::desktop::VideoPacket* video_packet = outgoing_message_.newMessage().mutable_video_packet();
    video_packet->set_error_code(error_code);
    sendSessionMessage();
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::onScreenListChanged(
    const base::ScreenCapturer::ScreenList& list, base::ScreenCapturer::ScreenId current)
{
    proto::desktop::ScreenList screen_list;
    screen_list.set_current_screen(current);

    for (const auto& resolition_item : list.resolutions)
    {
        proto::desktop::Size* resolution = screen_list.add_resolution();
        resolution->set_width(resolition_item.width());
        resolution->set_height(resolition_item.height());
    }

    for (const auto& screen_item : list.screens)
    {
        proto::desktop::Screen* screen = screen_list.add_screen();
        screen->set_id(screen_item.id);
        screen->set_title(screen_item.title.toStdString());

        proto::desktop::Point* position = screen->mutable_position();
        position->set_x(screen_item.position.x());
        position->set_y(screen_item.position.y());

        proto::desktop::Size* resolution = screen->mutable_resolution();
        resolution->set_width(screen_item.resolution.width());
        resolution->set_height(screen_item.resolution.height());

        proto::desktop::Point* dpi = screen->mutable_dpi();
        dpi->set_x(screen_item.dpi.x());
        dpi->set_y(screen_item.dpi.y());

        if (screen_item.is_primary)
            screen_list.set_primary_screen(screen_item.id);
    }

    proto::desktop::Extension* extension = outgoing_message_.newMessage().mutable_extension();
    extension->set_name(common::kSelectScreenExtension);
    extension->set_data(screen_list.SerializeAsString());
    sendSessionMessage();
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::onScreenTypeChanged(base::ScreenCapturer::ScreenType type, const QString& name)
{
    proto::desktop::ScreenType screen_type;
    screen_type.set_name(name.toStdString());

    switch (type)
    {
        case base::ScreenCapturer::ScreenType::DESKTOP:
            screen_type.set_type(proto::desktop::ScreenType::TYPE_DESKTOP);
            break;

        case base::ScreenCapturer::ScreenType::LOCK:
            screen_type.set_type(proto::desktop::ScreenType::TYPE_LOCK);
            break;

        case base::ScreenCapturer::ScreenType::LOGIN:
            screen_type.set_type(proto::desktop::ScreenType::TYPE_LOGIN);
            break;

        case base::ScreenCapturer::ScreenType::OTHER:
            screen_type.set_type(proto::desktop::ScreenType::TYPE_OTHER);
            break;

        default:
            screen_type.set_type(proto::desktop::ScreenType::TYPE_UNKNOWN);
            break;
    }

    proto::desktop::Extension* extension = outgoing_message_.newMessage().mutable_extension();
    extension->set_name(common::kScreenTypeExtension);
    extension->set_data(screen_type.SerializeAsString());
    sendSessionMessage();
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::onCursorPositionChanged(const QPoint& cursor_position)
{
    if (!scale_reducer_)
    {
        LOG(ERROR) << "Scale reducer is not initialized!";
        return;
    }

    if (!config_.cursor_position)
        return;

    int pos_x = static_cast<int>(
        static_cast<double>(cursor_position.x()) * scale_reducer_->scaleFactorX() / 100.0);
    int pos_y = static_cast<int>(
        static_cast<double>(cursor_position.y()) * scale_reducer_->scaleFactorY() / 100.0);

    proto::desktop::CursorPosition* position = outgoing_message_.newMessage().mutable_cursor_position();
    position->set_x(pos_x);
    position->set_y(pos_y);

    sendSessionMessage();
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::onClipboardEvent(const proto::desktop::ClipboardEvent& event)
{
    if (sessionType() != proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
        return;

    outgoing_message_.newMessage().mutable_clipboard_event()->CopyFrom(event);
    sendSessionMessage();
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::onCursorCaptureData(const base::MouseCursor* cursor)
{
    if (!cursor || !cursor_encoder_)
        return;

    proto::desktop::SessionToClient& message = outgoing_message_.newMessage();

    if (!cursor_encoder_->encode(*cursor, message.mutable_cursor_shape()))
        return;

    sendSessionMessage();
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::onAudioCaptureData(const proto::desktop::AudioPacket& audio_packet)
{
    if (is_audio_paused_ || !audio_encoder_)
        return;

    if (overflow_state_ == proto::desktop::Overflow::STATE_CRITICAL)
        return;

    if (!audio_encoder_->encode(audio_packet, outgoing_message_.newMessage().mutable_audio_packet()))
        return;

    sendSessionMessage();
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::start(const QString& ipc_channel_name)
{
    ipc_channel_->connectTo(ipc_channel_name);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::onIpcConnected()
{
    LOG(INFO) << "IPC channel is connected";
    ipc_channel_->setPaused(false);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::onIpcErrorOccurred()
{
    LOG(ERROR) << "Unable to connect to IPC server";
    ipc_channel_->disconnect();
    emit sig_finished();
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::onIpcDisconnected()
{
    LOG(INFO) << "IPC channel is disconnected";
    ipc_channel_->disconnect();
    emit sig_finished();
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::onIpcMessageReceived(quint32 channel_id, const QByteArray& buffer)
{
    quint16 tcp_channel_id = base::lowWord(channel_id);
    quint16 ipc_channel_id = base::highWord(channel_id);

    if (ipc_channel_id == proto::desktop::IPC_CHANNEL_ID_SESSION)
    {
        if (tcp_channel_id > std::numeric_limits<quint8>::max())
        {
            LOG(ERROR) << "Too big TCP channel ID number";
            return;
        }

        if (tcp_channel_id == proto::peer::CHANNEL_ID_0)
            readSessionMessage(buffer);
    }
    else if (ipc_channel_id == proto::desktop::IPC_CHANNEL_ID_SERVICE)
    {
        proto::desktop::ServiceToAgentClient message;

        if (!base::parse(buffer, &message))
        {
            LOG(ERROR) << "Unable to parse ServiceToDesktop message";
            return;
        }

        if (message.has_description())
        {
            session_type_ = message.description().session_type();
            sendCapabilities();
        }
        else if (message.has_overflow())
        {
            readOverflow(message.overflow().state());
        }
        else
        {
            LOG(WARNING) << "Unhandled message from service";
        }
    }
    else
    {
        LOG(WARNING) << "Unhandled message from channel:" << ipc_channel_id;
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::onTaskManagerMessage(const proto::task_manager::HostToClient& message)
{
    proto::desktop::Extension* extension = outgoing_message_.newMessage().mutable_extension();
    extension->set_name(common::kTaskManagerExtension);
    extension->set_data(message.SerializeAsString());
    sendSessionMessage();
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readSessionMessage(const QByteArray& buffer)
{
    if (!incoming_message_.parse(buffer))
    {
        LOG(ERROR) << "Invalid message from client";
        return;
    }

    if (incoming_message_->has_mouse_event())
        readMouseEvent(incoming_message_->mouse_event());
    else if (incoming_message_->has_key_event())
        readKeyEvent(incoming_message_->key_event());
    else if (incoming_message_->has_touch_event())
        readTouchEvent(incoming_message_->touch_event());
    else if (incoming_message_->has_text_event())
        readTextEvent(incoming_message_->text_event());
    else if (incoming_message_->has_clipboard_event())
        readClipboardEvent(incoming_message_->clipboard_event());
    else if (incoming_message_->has_extension())
        readExtension(incoming_message_->extension());
    else if (incoming_message_->has_config())
        readConfig(incoming_message_->config());
    else
        LOG(ERROR) << "Unhandled message from client";
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::sendSessionMessage(const QByteArray& buffer)
{
    quint32 channel_id = base::makeUint32(proto::desktop::IPC_CHANNEL_ID_SESSION, proto::peer::CHANNEL_ID_0);
    ipc_channel_->send(channel_id, buffer);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::sendSessionMessage()
{
    sendSessionMessage(outgoing_message_.serialize());
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readMouseEvent(const proto::desktop::MouseEvent& mouse_event)
{
    if (sessionType() != proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
    {
        LOG(ERROR) << "Mouse event for non-desktop-manage session";
        return;
    }

    if (!scale_reducer_)
    {
        LOG(ERROR) << "Scale reducer is NOT initialized";
        return;
    }

    int pos_x = static_cast<int>(
        static_cast<double>(mouse_event.x() * 100) / scale_reducer_->scaleFactorX());
    int pos_y = static_cast<int>(
        static_cast<double>(mouse_event.y() * 100) / scale_reducer_->scaleFactorY());

    proto::desktop::MouseEvent out_mouse_event;
    out_mouse_event.set_mask(mouse_event.mask());
    out_mouse_event.set_x(pos_x);
    out_mouse_event.set_y(pos_y);

    emit sig_injectMouseEvent(out_mouse_event);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readKeyEvent(const proto::desktop::KeyEvent& key_event)
{
    if (sessionType() != proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
    {
        LOG(ERROR) << "Key event for non-desktop-manage session";
        return;
    }

    emit sig_injectKeyEvent(incoming_message_->key_event());
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readTouchEvent(const proto::desktop::TouchEvent& touch_event)
{
    if (sessionType() != proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
    {
        LOG(ERROR) << "Touch event for non-desktop-manage session";
        return;
    }

    emit sig_injectTouchEvent(incoming_message_->touch_event());
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readTextEvent(const proto::desktop::TextEvent& text_event)
{
    if (sessionType() != proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
    {
        LOG(ERROR) << "Text event for non-desktop-manage session";
        return;
    }

    emit sig_injectTextEvent(incoming_message_->text_event());
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readClipboardEvent(const proto::desktop::ClipboardEvent& clipboard_event)
{
    if (sessionType() != proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
    {
        LOG(ERROR) << "Clipboard event for non-desktop-manage session";
        return;
    }

    emit sig_injectClipboardEvent(incoming_message_->clipboard_event());
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readExtension(const proto::desktop::Extension& extension)
{
    if (extension.name() == common::kKeyFrameExtension)
        readKeyFrameExtension(extension.data());
    if (extension.name() == common::kTaskManagerExtension)
        readTaskManagerExtension(extension.data());
    else if (extension.name() == common::kSelectScreenExtension)
        readSelectScreenExtension(extension.data());
    else if (extension.name() == common::kPreferredSizeExtension)
        readPreferredSizeExtension(extension.data());
    else if (extension.name() == common::kVideoPauseExtension)
        readVideoPauseExtension(extension.data());
    else if (extension.name() == common::kAudioPauseExtension)
        readAudioPauseExtension(extension.data());
    else if (extension.name() == common::kPowerControlExtension)
        readPowerControlExtension(extension.data());
    else if (extension.name() == common::kRemoteUpdateExtension)
        readRemoteUpdateExtension(extension.data());
    else if (extension.name() == common::kSystemInfoExtension)
        readSystemInfoExtension(extension.data());
    else
        LOG(ERROR) << "Unknown extension:" << extension.name();
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readConfig(const proto::desktop::Config& config)
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
                base::parse(config.pixel_format()), static_cast<int>(config.compress_ratio()));
            break;

        default:
            // No supported video encoding.
            LOG(ERROR) << "Unsupported video encoding:" << config.video_encoding();
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
            LOG(ERROR) << "Unsupported audio encoding:" << config.audio_encoding();
            audio_encoder_.reset();
            break;
    }

    cursor_encoder_.reset();
    if (config.flags() & proto::desktop::ENABLE_CURSOR_SHAPE)
    {
        LOG(INFO) << "Cursor shape enabled. Init cursor encoder";
        cursor_encoder_ = std::make_unique<base::CursorEncoder>();
    }

    scale_reducer_ = std::make_unique<base::ScaleReducer>();

    config_.disable_font_smoothing = (config.flags() & proto::desktop::DISABLE_FONT_SMOOTHING);
    config_.disable_effects = (config.flags() & proto::desktop::DISABLE_EFFECTS);
    config_.disable_wallpaper = (config.flags() & proto::desktop::DISABLE_WALLPAPER);
    config_.block_input = (config.flags() & proto::desktop::BLOCK_REMOTE_INPUT);
    config_.lock_at_disconnect = (config.flags() & proto::desktop::LOCK_AT_DISCONNECT);
    config_.clear_clipboard = (config.flags() & proto::desktop::CLEAR_CLIPBOARD);
    config_.cursor_position = (config.flags() & proto::desktop::CURSOR_POSITION);

    LOG(INFO) << "Config changed (encoding:" << config.video_encoding()
              << "cursor_shape:" << (cursor_encoder_ != nullptr)
              << "font_smoothing:" << config_.disable_font_smoothing
              << "effects:" << config_.disable_effects
              << "wallpaper:" << config_.disable_wallpaper
              << "block_input:" << config_.block_input
              << "lock_at_disconnect:" << config_.lock_at_disconnect
              << "clear_clipboard:" << config_.clear_clipboard
              << "cursor_position:" << config_.cursor_position << ")";

    emit sig_configured();
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readKeyFrameExtension(const std::string& /* data */)
{
    LOG(INFO) << "Key frame requested by client";
    if (video_encoder_)
        video_encoder_->setKeyFrameRequired(true);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readSelectScreenExtension(const std::string& data)
{
    proto::desktop::Screen screen;
    if (!screen.ParseFromString(data))
    {
        LOG(ERROR) << "Unable to parse select screen extension data";
        return;
    }

    LOG(INFO) << "Received:" << screen;

    base::ScreenCapturer::ScreenId screen_id = static_cast<base::ScreenCapturer::ScreenId>(screen.id());
    QSize resolution = base::parse(screen.resolution());

    emit sig_selectScreen(screen_id, resolution);
    preferred_size_ = QSize(0, 0);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readPreferredSizeExtension(const std::string& data)
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
void DesktopAgentClient::readVideoPauseExtension(const std::string& data)
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
void DesktopAgentClient::readAudioPauseExtension(const std::string& data)
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
void DesktopAgentClient::readPowerControlExtension(const std::string& data)
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

    LOG(INFO) << "Received:" << power_control;

    switch (power_control.action())
    {
        case proto::desktop::PowerControl::ACTION_SHUTDOWN:
        {
            if (!base::PowerController::shutdown())
                LOG(ERROR) << "Unable to shutdown";
        }
        break;

        case proto::desktop::PowerControl::ACTION_REBOOT:
        {
            if (!base::PowerController::reboot())
                LOG(ERROR) << "Unable to reboot";
        }
        break;

        case proto::desktop::PowerControl::ACTION_REBOOT_SAFE_MODE:
        {
#if defined(Q_OS_WINDOWS)
            if (!base::SafeModeUtil::setSafeModeService(Service::kName, true))
            {
                LOG(ERROR) << "Failed to add service to start in safe mode";
                return;
            }

            LOG(INFO) << "Service added successfully to start in safe mode";

            HostStorage storage;
            storage.setBootToSafeMode(true);

            if (!base::SafeModeUtil::setSafeMode(true))
            {
                LOG(ERROR) << "Failed to enable boot in Safe Mode";
                return;
            }

            LOG(INFO) << "Safe Mode boot enabled successfully";
            if (!base::PowerController::reboot())
                LOG(ERROR) << "Unable to reboot";
#endif // defined(Q_OS_WINDOWS)
        }
        break;

        case proto::desktop::PowerControl::ACTION_LOGOFF:
        {
            if (!base::PowerController::logoff())
                LOG(ERROR) << "base::PowerController::logoff failed";
        }
        break;

        case proto::desktop::PowerControl::ACTION_LOCK:
        {
            if (!base::PowerController::lock())
                LOG(ERROR) << "base::PowerController::lock failed";
        }
        break;

        default:
            LOG(ERROR) << "Unhandled power control action:" << power_control.action();
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readRemoteUpdateExtension(const std::string& /* data */)
{
#if defined(Q_OS_WINDOWS)
    LOG(INFO) << "Remote update requested";

    if (sessionType() != proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
    {
        LOG(ERROR) << "Update can only be launched from a desktop manage session";
        return;
    }

    launchUpdater(base::currentProcessSessionId());
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readSystemInfoExtension(const std::string& data)
{
#if defined(Q_OS_WINDOWS)
    proto::system_info::SystemInfoRequest system_info_request;

    if (!data.empty())
    {
        if (!system_info_request.ParseFromString(data))
            LOG(ERROR) << "Unable to parse system info request";
    }

    proto::system_info::SystemInfo system_info;
    createSystemInfo(system_info_request, &system_info);

    proto::desktop::Extension* desktop_extension = outgoing_message_.newMessage().mutable_extension();
    desktop_extension->set_name(common::kSystemInfoExtension);
    desktop_extension->set_data(system_info.SerializeAsString());

    sendSessionMessage();
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readTaskManagerExtension(const std::string& data)
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
                this, &DesktopAgentClient::onTaskManagerMessage);
    }

    task_manager_->readMessage(message);
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readOverflow(proto::desktop::Overflow::State state)
{
    if (state != overflow_state_)
    {
        LOG(INFO) << "Overflow state:" << state << "pressure score:" << pressure_score_;
        overflow_state_ = state;
    }

    auto scaled_size = [](const QSize& size, double factor) -> QSize
    {
        return QSize(double(size.width()) * factor, double(size.height()) * factor);
    };

    QSize forced_size = forced_size_;

    if (state == proto::desktop::Overflow::STATE_CRITICAL)
        pressure_score_ = std::min(100, pressure_score_ + 20);
    else if (state == proto::desktop::Overflow::STATE_WARNING)
        pressure_score_ = std::min(100, pressure_score_ + 8);
    else if (state == proto::desktop::Overflow::STATE_NONE)
        pressure_score_ = std::max(0, pressure_score_ - 3);

    if (pressure_score_ >= 90)
        forced_size = scaled_size(source_size_, 0.7);
    else if (pressure_score_ >= 80)
        forced_size = scaled_size(source_size_, 0.8);
    else if (pressure_score_ >= 70)
        forced_size = scaled_size(source_size_, 0.9);
    else
        forced_size = QSize();

    if (forced_size != forced_size_)
    {
        LOG(INFO) << "Forced size changed from" << forced_size_ << "to" << forced_size;
        forced_size_ = forced_size;
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::sendCapabilities()
{
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

    LOG(INFO) << "Sending config request (extensions:" << capabilities->extensions()
              << "video_encodings:" << capabilities->video_encodings()
              << "audio_encodings:" << capabilities->audio_encodings()
              << "OS:" << capabilities->os_type() << ")";
    sendSessionMessage();
}

} // namespace host
