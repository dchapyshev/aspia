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

#include "base/numeric_utils.h"
#include "base/power_controller.h"
#include "base/serialization.h"
#include "base/ipc/ipc_channel.h"
#include "common/desktop_session_constants.h"
#include "host/host_storage.h"
#include "host/service.h"
#include "proto/desktop_channel.h"
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
    CLOG(INFO) << "Ctor";
    connect(ipc_channel_, &base::IpcChannel::sig_connected, this, &DesktopAgentClient::onIpcConnected);
    connect(ipc_channel_, &base::IpcChannel::sig_disconnected, this, &DesktopAgentClient::onIpcDisconnected);
    connect(ipc_channel_, &base::IpcChannel::sig_errorOccurred, this, &DesktopAgentClient::onIpcErrorOccurred);
    connect(ipc_channel_, &base::IpcChannel::sig_messageReceived, this, &DesktopAgentClient::onIpcMessageReceived);
}

//--------------------------------------------------------------------------------------------------
DesktopAgentClient::~DesktopAgentClient()
{
    CLOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::onScreenData(const QByteArray& buffer)
{
    if (!is_video_paused_)
        sendSessionMessage(proto::desktop::CHANNEL_ID_SCREEN, buffer, false);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::onScreenListData(const QByteArray& buffer)
{
    sendSessionMessage(proto::desktop::CHANNEL_ID_EXTENSION, buffer, true);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::onScreenTypeData(const QByteArray& buffer)
{
    sendSessionMessage(proto::desktop::CHANNEL_ID_EXTENSION, buffer, true);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::onCursorPositionData(const QByteArray& buffer)
{
    if (config_.cursor_position)
        sendSessionMessage(proto::desktop::CHANNEL_ID_SCREEN, buffer, false);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::onCursorData(const QByteArray& buffer)
{
    if (config_.cursor_shape)
        sendSessionMessage(proto::desktop::CHANNEL_ID_SCREEN, buffer, true);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::onAudioData(const QByteArray& buffer)
{
    if (is_audio_paused_ || config_.audio_encoding != proto::desktop::AUDIO_ENCODING_OPUS)
        return;

    if (overflow_state_ == proto::desktop::Overflow::STATE_CRITICAL)
        return;

    sendSessionMessage(proto::desktop::CHANNEL_ID_AUDIO, buffer, false);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::start(const QString& ipc_channel_name)
{
    ipc_channel_->connectTo(ipc_channel_name);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::onIpcConnected()
{
    CLOG(INFO) << "IPC channel is connected";
    ipc_channel_->setPaused(false);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::onIpcErrorOccurred()
{
    CLOG(ERROR) << "Unable to connect to IPC server";
    ipc_channel_->disconnect();
    emit sig_finished();
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::onIpcDisconnected()
{
    CLOG(INFO) << "IPC channel is disconnected";
    ipc_channel_->disconnect();
    emit sig_finished();
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::onIpcMessageReceived(
    quint32 channel_id, const QByteArray& buffer, bool /* reliable */)
{
    quint16 net_channel_id = base::lowWord(channel_id);
    quint16 ipc_channel_id = base::highWord(channel_id);

    if (ipc_channel_id == proto::desktop::IPC_CHANNEL_ID_SESSION)
    {
        if (net_channel_id > std::numeric_limits<quint8>::max())
        {
            CLOG(ERROR) << "Too big TCP channel ID number";
            return;
        }

        readSessionMessage(net_channel_id, buffer);
    }
    else if (ipc_channel_id == proto::desktop::IPC_CHANNEL_ID_SERVICE)
    {
        proto::desktop::ServiceToAgentClient message;
        if (!base::parse(buffer, &message))
        {
            CLOG(ERROR) << "Unable to parse ServiceToDesktop message";
            return;
        }

        if (message.has_description())
        {
            session_type_ = message.description().session_type();
            sendCapabilities();
        }
        else if (message.has_config())
        {
            readConfig(message.config());
        }
        else if (message.has_overflow())
        {
            readOverflow(message.overflow().state());
        }
        else if (message.has_bandwidth_change())
        {
            bandwidth_ = message.bandwidth_change().bandwidth();
            CLOG(INFO) << "Bandwidth changed:" << bandwidth_;
        }
        else
        {
            CLOG(WARNING) << "Unhandled message from service";
        }
    }
    else
    {
        CLOG(WARNING) << "Unhandled message from channel:" << ipc_channel_id;
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::onTaskManagerMessage(const proto::task_manager::HostToClient& extension_message)
{
    proto::desktop::ExtensionData message;
    message.set_name(common::kTaskManagerExtension);
    message.set_data(extension_message.SerializeAsString());
    sendSessionMessage(proto::desktop::CHANNEL_ID_EXTENSION, base::serialize(message), true);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readSessionMessage(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id == proto::desktop::CHANNEL_ID_INPUT)
    {
        proto::desktop::InputData message;
        if (!base::parse(buffer, &message))
        {
            CLOG(ERROR) << "Unable to parse input message";
            return;
        }

        if (message.has_mouse())
            readMouseEvent(message.mouse());
        else if (message.has_touch())
            readTouchEvent(message.touch());
        else if (message.has_key())
            readKeyEvent(message.key());
        else if (message.has_text())
            readTextEvent(message.text());
    }
    else if (channel_id == proto::desktop::CHANNEL_ID_EXTENSION)
    {
        proto::desktop::ExtensionData message;
        if (!base::parse(buffer, &message))
        {
            CLOG(ERROR) << "Unable to parse extension message";
            return;
        }

        readExtension(message);
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::sendSessionMessage(quint8 net_channel_id, const QByteArray& buffer, bool reliable)
{
    quint32 channel_id = base::makeUint32(proto::desktop::IPC_CHANNEL_ID_SESSION, net_channel_id);
    ipc_channel_->send(channel_id, buffer, reliable);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readMouseEvent(const proto::desktop::MouseEvent& mouse_event)
{
    if (sessionType() == proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
        emit sig_injectMouseEvent(mouse_event);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readKeyEvent(const proto::desktop::KeyEvent& key_event)
{
    if (sessionType() == proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
        emit sig_injectKeyEvent(key_event);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readTouchEvent(const proto::desktop::TouchEvent& touch_event)
{
    if (sessionType() == proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
        emit sig_injectTouchEvent(touch_event);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readTextEvent(const proto::desktop::TextEvent& text_event)
{
    if (sessionType() == proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
        emit sig_injectTextEvent(text_event);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readExtension(const proto::desktop::ExtensionData& extension)
{
    if (extension.name() == common::kKeyFrameExtension)
        readKeyFrameExtension(extension.data());
    else if (extension.name() == common::kTaskManagerExtension)
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
        CLOG(ERROR) << "Unknown extension:" << extension.name();
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readConfig(const proto::desktop::Config& config)
{
    config_.video_encoding = config.video_encoding();
    config_.audio_encoding = config.audio_encoding();
    config_.disable_effects = (config.flags() & proto::desktop::DISABLE_EFFECTS);
    config_.disable_wallpaper = (config.flags() & proto::desktop::DISABLE_WALLPAPER);
    config_.block_input = (config.flags() & proto::desktop::BLOCK_REMOTE_INPUT);
    config_.lock_at_disconnect = (config.flags() & proto::desktop::LOCK_AT_DISCONNECT);
    config_.cursor_position = (config.flags() & proto::desktop::CURSOR_POSITION);
    config_.cursor_shape = (config.flags() & proto::desktop::ENABLE_CURSOR_SHAPE);

    CLOG(INFO) << "Config changed (encoding:" << config.video_encoding()
               << "cursor_shape:" << config_.cursor_shape
               << "effects:" << config_.disable_effects
               << "wallpaper:" << config_.disable_wallpaper
               << "block_input:" << config_.block_input
               << "lock_at_disconnect:" << config_.lock_at_disconnect
               << "cursor_position:" << config_.cursor_position
               << "cursor_shape:" << config_.cursor_shape << ")";
    emit sig_configured();
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readKeyFrameExtension(const std::string& /* data */)
{
    CLOG(INFO) << "Key frame requested by client";
    emit sig_keyFrameRequested();
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readSelectScreenExtension(const std::string& data)
{
    proto::desktop::Screen screen;
    if (!screen.ParseFromString(data))
    {
        CLOG(ERROR) << "Unable to parse select screen extension data";
        return;
    }

    CLOG(INFO) << "Received:" << screen;
    emit sig_selectScreen(screen);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readPreferredSizeExtension(const std::string& data)
{
    proto::desktop::Size preferred_size;
    if (!preferred_size.ParseFromString(data))
    {
        CLOG(ERROR) << "Unable to parse preferred size extension data";
        return;
    }

    static const int kMaxScreenSize = std::numeric_limits<qint16>::max();

    if (preferred_size.width() < 0 || preferred_size.width() > kMaxScreenSize ||
        preferred_size.height() < 0 || preferred_size.height() > kMaxScreenSize)
    {
        CLOG(ERROR) << "Invalid preferred size:" << preferred_size;
        return;
    }

    CLOG(INFO) << "Preferred size changed:" << preferred_size;
    preferred_size_ = base::parse(preferred_size);
    emit sig_preferredSizeChanged(preferred_size_);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readVideoPauseExtension(const std::string& data)
{
    proto::desktop::Pause pause;
    if (!pause.ParseFromString(data))
    {
        CLOG(ERROR) << "Unable to parse video pause extension data";
        return;
    }

    is_video_paused_ = pause.enable();
    CLOG(INFO) << "Video paused:" << is_video_paused_;

    if (!is_video_paused_)
        emit sig_keyFrameRequested();
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readAudioPauseExtension(const std::string& data)
{
    proto::desktop::Pause pause;
    if (!pause.ParseFromString(data))
    {
        CLOG(ERROR) << "Unable to parse pause extension data";
        return;
    }

    is_audio_paused_ = pause.enable();
    CLOG(INFO) << "Audio paused:" << is_audio_paused_;
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readPowerControlExtension(const std::string& data)
{
    if (sessionType() != proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
    {
        CLOG(ERROR) << "Power management is only accessible from a desktop manage session";
        return;
    }

    proto::desktop::PowerControl power_control;
    if (!power_control.ParseFromString(data))
    {
        CLOG(ERROR) << "Unable to parse power control extension data";
        return;
    }

    CLOG(INFO) << "Received:" << power_control;

    switch (power_control.action())
    {
        case proto::desktop::PowerControl::ACTION_SHUTDOWN:
            base::PowerController::shutdown();
            break;

        case proto::desktop::PowerControl::ACTION_REBOOT:
            base::PowerController::reboot();
            break;

        case proto::desktop::PowerControl::ACTION_REBOOT_SAFE_MODE:
        {
#if defined(Q_OS_WINDOWS)
            if (!base::SafeModeUtil::setSafeModeService(Service::kName, true))
            {
                CLOG(ERROR) << "Failed to add service to start in safe mode";
                return;
            }

            CLOG(INFO) << "Service added successfully to start in safe mode";

            HostStorage storage;
            storage.setBootToSafeMode(true);

            if (!base::SafeModeUtil::setSafeMode(true))
            {
                CLOG(ERROR) << "Failed to enable boot in Safe Mode";
                return;
            }

            CLOG(INFO) << "Safe Mode boot enabled successfully";
            if (!base::PowerController::reboot())
                CLOG(ERROR) << "Unable to reboot";
#endif // defined(Q_OS_WINDOWS)
        }
        break;

        case proto::desktop::PowerControl::ACTION_LOGOFF:
            base::PowerController::logoff();
            break;

        case proto::desktop::PowerControl::ACTION_LOCK:
            base::PowerController::lock();
            break;

        default:
            CLOG(ERROR) << "Unhandled power control action:" << power_control.action();
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readRemoteUpdateExtension(const std::string& /* data */)
{
#if defined(Q_OS_WINDOWS)
    CLOG(INFO) << "Remote update requested";
    if (sessionType() == proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
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
            CLOG(ERROR) << "Unable to parse system info request";
    }

    proto::system_info::SystemInfo system_info;
    createSystemInfo(system_info_request, &system_info);

    proto::desktop::ExtensionData message;
    message.set_name(common::kSystemInfoExtension);
    message.set_data(system_info.SerializeAsString());

    sendSessionMessage(proto::desktop::CHANNEL_ID_EXTENSION, base::serialize(message), true);
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readTaskManagerExtension(const std::string& data)
{
#if defined(Q_OS_WINDOWS)
    proto::task_manager::ClientToHost message;
    if (!message.ParseFromString(data))
    {
        CLOG(ERROR) << "Unable to parse task manager extension data";
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
    if (state == overflow_state_)
        return;

    CLOG(INFO) << "Overflow state changed from" << overflow_state_ << "to" << state;
    overflow_state_ = state;
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
    proto::desktop::ServiceToClient message;
    proto::desktop::Capabilities* capabilities = message.mutable_capabilities();

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
#elif defined(Q_OS_MACOS)
    capabilities->set_os_type(proto::DesktopCapabilities::OS_TYPE_MACOSX);
#else
#warning Not implemented
#endif

    CLOG(INFO) << "Sending:" << *capabilities;
    sendSessionMessage(proto::desktop::CHANNEL_ID_CONTROL, base::serialize(message), true);
}

} // namespace host
