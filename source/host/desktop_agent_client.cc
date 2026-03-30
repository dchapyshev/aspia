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
#include "proto/desktop_channel.h"
#include "proto/peer.h"

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
void DesktopAgentClient::onVideoData(const QByteArray& buffer, bool is_key_frame)
{
    if (!is_video_paused_)
        sendSessionMessage(proto::desktop::CHANNEL_ID_VIDEO, buffer, is_key_frame);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::onScreenListData(const QByteArray& buffer)
{
    sendSessionMessage(proto::desktop::CHANNEL_ID_SCREEN, buffer, true);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::onScreenTypeData(const QByteArray& buffer)
{
    sendSessionMessage(proto::desktop::CHANNEL_ID_SCREEN, buffer, true);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::onCursorPositionData(const QByteArray& buffer)
{
    if (config_.has_value() && config_->cursor_position())
        sendSessionMessage(proto::desktop::CHANNEL_ID_CURSOR, buffer, false);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::onCursorShapeData(const QByteArray& buffer)
{
    if (config_.has_value() && config_->cursor_shape())
        sendSessionMessage(proto::desktop::CHANNEL_ID_CURSOR, buffer, true);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::onAudioData(const QByteArray& buffer)
{
    if (is_audio_paused_ || !config_.has_value() || !config_->audio())
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
void DesktopAgentClient::readSessionMessage(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id == proto::desktop::CHANNEL_ID_INPUT)
    {
        proto::input::ClientToHost message;
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
    else if (channel_id == proto::desktop::CHANNEL_ID_VIDEO)
    {
        proto::video::ClientToHost message;
        if (!base::parse(buffer, &message))
        {
            CLOG(ERROR) << "Unable to parse video control message";
            return;
        }

        if (message.has_key_frame())
            emit sig_keyFrameRequested();
        else if (message.has_preferred_size())
            readPreferredSize(message.preferred_size());
        else if (message.has_pause())
            readVideoPause(message.pause());
    }
    else if (channel_id == proto::desktop::CHANNEL_ID_SCREEN)
    {
        proto::screen::ClientToHost message;
        if (!base::parse(buffer, &message))
        {
            CLOG(ERROR) << "Unable to parse screen control message";
            return;
        }

        if (message.has_screen())
            emit sig_selectScreen(message.screen());
    }
    else if (channel_id == proto::desktop::CHANNEL_ID_AUDIO)
    {
        proto::audio::ClientToHost message;
        if (!base::parse(buffer, &message))
        {
            CLOG(ERROR) << "Unable to parse audio control message";
            return;
        }

        if (message.has_pause())
            readAudioPause(message.pause());
    }
    else if (channel_id == proto::desktop::CHANNEL_ID_POWER)
    {
        proto::power::ClientToHost message;
        if (!base::parse(buffer, &message))
        {
            CLOG(ERROR) << "Unable to parse power message";
            return;
        }

        if (message.has_power_control())
            readPowerControl(message.power_control());
    }
    else if (channel_id == proto::desktop::CHANNEL_ID_CONTROL)
    {
        proto::control::ClientToHost message;
        if (!base::parse(buffer, &message))
        {
            CLOG(ERROR) << "Unable to parse control message";
            return;
        }

        if (message.has_capabilities())
            readCapabilities(message.capabilities());
        else if (message.has_config())
            readConfig(message.config());
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::sendSessionMessage(quint8 net_channel_id, const QByteArray& buffer, bool reliable)
{
    quint32 channel_id = base::makeUint32(proto::desktop::IPC_CHANNEL_ID_SESSION, net_channel_id);
    ipc_channel_->send(channel_id, buffer, reliable);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readMouseEvent(const proto::input::MouseEvent& mouse_event)
{
    if (sessionType() == proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
        emit sig_injectMouseEvent(mouse_event);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readKeyEvent(const proto::input::KeyEvent& key_event)
{
    if (sessionType() == proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
        emit sig_injectKeyEvent(key_event);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readTouchEvent(const proto::input::TouchEvent& touch_event)
{
    if (sessionType() == proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
        emit sig_injectTouchEvent(touch_event);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readTextEvent(const proto::input::TextEvent& text_event)
{
    if (sessionType() == proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
        emit sig_injectTextEvent(text_event);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readPreferredSize(const proto::video::PreferredSize& size)
{
    static const int kMaxScreenSize = std::numeric_limits<qint16>::max();

    if (size.width() < 0 || size.width() > kMaxScreenSize ||
        size.height() < 0 || size.height() > kMaxScreenSize)
    {
        CLOG(ERROR) << "Invalid preferred size:" << size;
        return;
    }

    CLOG(INFO) << "Preferred size changed:" << size;
    preferred_size_.setWidth(size.width());
    preferred_size_.setHeight(size.height());
    emit sig_preferredSizeChanged(preferred_size_);
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readVideoPause(const proto::video::Pause& pause)
{
    is_video_paused_ = pause.enable();
    CLOG(INFO) << "Video paused:" << is_video_paused_;
    if (!is_video_paused_)
        emit sig_keyFrameRequested();
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readAudioPause(const proto::audio::Pause& pause)
{
    is_audio_paused_ = pause.enable();
    CLOG(INFO) << "Audio paused:" << is_audio_paused_;
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readCapabilities(const proto::control::Capabilities& capabilities)
{
    CLOG(INFO) << "Capabilities changed:" << capabilities;

    auto has_flag = [](const proto::control::Capabilities& capabilities, const char* name) -> bool
    {
        for (int i = 0; i < capabilities.flag_size(); ++i)
        {
            const proto::control::Capabilities::Flag& flag = capabilities.flag(i);
            if (flag.name() == name)
                return flag.value();
        }
        return false;
    };

    vp8_supported_ = has_flag(capabilities, common::kFlagVideoVP8);
    vp9_supported_ = has_flag(capabilities, common::kFlagVideoVP9);
    opus_supported_ = has_flag(capabilities, common::kFlagAudioOpus);

    sendCapabilities();
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readConfig(const proto::control::Config& config)
{
    config_ = config;
    CLOG(INFO) << "Config changed:" << config;
    emit sig_configured();
}

//--------------------------------------------------------------------------------------------------
void DesktopAgentClient::readPowerControl(const proto::power::Control& control)
{
    if (sessionType() != proto::peer::SESSION_TYPE_DESKTOP_MANAGE)
    {
        CLOG(ERROR) << "Power management is only accessible from a desktop manage session";
        return;
    }

    CLOG(INFO) << "Received:" << control;

    switch (control.action())
    {
        case proto::power::Control::ACTION_LOGOFF:
            base::PowerController::logoff();
            break;

        case proto::power::Control::ACTION_LOCK:
            base::PowerController::lock();
            break;

        default:
            CLOG(ERROR) << "Unhandled power control action:" << control.action();
            break;
    }
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
    // Create a capabilities.
    proto::control::HostToClient message;
    proto::control::Capabilities* capabilities = message.mutable_capabilities();

    auto add_flag = [capabilities](const char* name, bool value)
    {
        proto::control::Capabilities::Flag* flag = capabilities->add_flag();
        flag->set_name(name);
        flag->set_value(value);
    };

    add_flag(common::kFlagVideoVP8, true);
    add_flag(common::kFlagVideoVP9, true);

#if defined(Q_OS_WINDOWS)
    add_flag(common::kFlagOSWindows, true);
    add_flag(common::kFlagAudioOpus, true);
    add_flag(common::kFlagPasteAsKeystrokes, true);
    add_flag(common::kFlagBlockInput, true);
    add_flag(common::kFlagDesktopEffects, true);
    add_flag(common::kFlagDesktopWallpaper, true);
    add_flag(common::kFlagLockAtDisconnect, true);
    add_flag(common::kFlagPowerControl, true);
    add_flag(common::kFlagSelectScreen, true);
    add_flag(common::kFlagSystemInfo, true);
    add_flag(common::kFlagTaskManager, true);
#elif defined(Q_OS_LINUX)
    add_flag(common::kFlagOSLinux, true);
#elif defined(Q_OS_MACOS)
    add_flag(common::kFlagOSMacOS, true);
#else
#warning Not implemented
#endif

    CLOG(INFO) << "Sending:" << *capabilities;
    sendSessionMessage(proto::desktop::CHANNEL_ID_CONTROL, base::serialize(message), true);
}

} // namespace host
