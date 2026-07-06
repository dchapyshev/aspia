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

#include "host/android/desktop_client.h"

#include <limits>

#include "base/logging.h"
#include "base/serialization.h"
#include "common/desktop_session_constants.h"
#include "proto/desktop_channel.h"
#include "proto/desktop_clipboard.h"
#include "proto/desktop_control.h"
#include "proto/desktop_input.h"
#include "proto/desktop_screen.h"
#include "proto/desktop_video.h"

//--------------------------------------------------------------------------------------------------
DesktopClient::DesktopClient(TcpChannel* tcp_channel, QObject* parent)
    : Client(tcp_channel, parent)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
DesktopClient::~DesktopClient()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onVideoData(const QByteArray& buffer, bool is_key_frame)
{
    if (!is_paused_)
        send(proto::desktop::CHANNEL_ID_VIDEO, buffer, is_key_frame);
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onScreenListData(const QByteArray& buffer)
{
    send(proto::desktop::CHANNEL_ID_SCREEN, buffer, true);
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onClipboardData(const QByteArray& buffer)
{
    send(proto::desktop::CHANNEL_ID_CLIPBOARD, buffer, true);
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onAudioData(const QByteArray& buffer)
{
    send(proto::desktop::CHANNEL_ID_AUDIO, buffer, true);
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onStart()
{
    LOG(INFO) << "Desktop session started";

    // Unpause the channel (the base class does this on sig_started) so the client's handshake messages
    // reach onMessage. The client opens the exchange by sending its capabilities.
    emit sig_started();
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::onMessage(quint8 channel_id, const QByteArray& buffer)
{
    switch (channel_id)
    {
        case proto::desktop::CHANNEL_ID_CONTROL:
            handleControl(buffer);
            break;

        case proto::desktop::CHANNEL_ID_VIDEO:
            handleVideoControl(buffer);
            break;

        case proto::desktop::CHANNEL_ID_SCREEN:
            handleScreenControl(buffer);
            break;

        case proto::desktop::CHANNEL_ID_INPUT:
            handleInput(buffer);
            break;

        case proto::desktop::CHANNEL_ID_CLIPBOARD:
            handleClipboard(buffer);
            break;

        default:
            LOG(INFO) << "Unhandled channel:" << channel_id;
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::handleControl(const QByteArray& buffer)
{
    proto::control::ClientToHost message;
    if (!parse(buffer, &message))
    {
        LOG(ERROR) << "Unable to parse control message";
        return;
    }

    if (message.has_capabilities())
    {
        const proto::control::Capabilities& capabilities = message.capabilities();

        auto has_flag = [&capabilities](const char* name)
        {
            for (int i = 0; i < capabilities.flag_size(); ++i)
            {
                if (capabilities.flag(i).name() == name)
                    return capabilities.flag(i).value();
            }
            return false;
        };

        vp8_supported_ = has_flag(kFlagVideoVP8);
        vp9_supported_ = has_flag(kFlagVideoVP9);
        h264_supported_ = has_flag(kFlagVideoH264);
        opus_supported_ = has_flag(kFlagAudioOpus);

        LOG(INFO) << "Client capabilities: vp8:" << vp8_supported_ << "vp9:" << vp9_supported_
                  << "h264:" << h264_supported_ << "opus:" << opus_supported_;

        sendCapabilities();

        // A capability change after the client is already configured (e.g. dropping a codec) must
        // re-select the encoder in the agent.
        if (configured_)
            emit sig_configured();
    }
    else if (message.has_config())
    {
        const proto::control::Config& config = message.config();

        // The initial preferred size arrives in the config; later PreferredSize messages take over.
        if (preferred_size_.isEmpty() && config.has_preferred_size() &&
            config.preferred_size().width() > 0 && config.preferred_size().height() > 0)
        {
            preferred_size_.setWidth(config.preferred_size().width());
            preferred_size_.setHeight(config.preferred_size().height());
        }

        audio_enabled_ = config.audio();
        configured_ = true;
        emit sig_configured();
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::handleVideoControl(const QByteArray& buffer)
{
    proto::video::ClientToHost message;
    if (!parse(buffer, &message))
    {
        LOG(ERROR) << "Unable to parse video control message";
        return;
    }

    if (message.has_key_frame())
    {
        emit sig_keyFrameRequested();
    }
    else if (message.has_preferred_size())
    {
        const proto::video::PreferredSize& size = message.preferred_size();

        static const int kMaxScreenSize = std::numeric_limits<qint16>::max();
        if (size.width() < 0 || size.width() > kMaxScreenSize ||
            size.height() < 0 || size.height() > kMaxScreenSize)
        {
            LOG(ERROR) << "Invalid preferred size:" << size.width() << "x" << size.height();
            return;
        }

        preferred_size_ = QSize(size.width(), size.height());
        LOG(INFO) << "Preferred size changed:" << preferred_size_;

        emit sig_preferredSizeChanged();
    }
    else if (message.has_pause())
    {
        is_paused_ = message.pause().enable();
        LOG(INFO) << "Video paused:" << is_paused_;

        if (!is_paused_)
            emit sig_keyFrameRequested();
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::handleScreenControl(const QByteArray& buffer)
{
    proto::screen::ClientToHost message;
    if (!parse(buffer, &message))
    {
        LOG(ERROR) << "Unable to parse screen control message";
        return;
    }

    // The Android host exposes a single screen, so there is nothing to switch; just ask the agent to
    // re-send the list and a fresh key frame.
    if (message.has_screen())
        emit sig_screenListRequested();
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::handleInput(const QByteArray& buffer)
{
    proto::input::ClientToHost message;
    if (!parse(buffer, &message))
    {
        LOG(ERROR) << "Unable to parse input message";
        return;
    }

    // The shared injector lives in the agent, so the event is only forwarded here.
    if (message.has_mouse())
        emit sig_injectMouseEvent(message.mouse());
    else if (message.has_touch())
        emit sig_injectTouchEvent(message.touch());
    else if (message.has_key())
        emit sig_injectKeyEvent(message.key());
    else if (message.has_text())
        emit sig_injectTextEvent(message.text());
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::handleClipboard(const QByteArray& buffer)
{
    proto::clipboard::ClientToHost message;
    if (!parse(buffer, &message))
    {
        LOG(ERROR) << "Unable to parse clipboard message";
        return;
    }

    // The shared clipboard lives in the agent; the event is only forwarded here.
    if (message.has_event())
        emit sig_injectClipboardEvent(message.event());
}

//--------------------------------------------------------------------------------------------------
void DesktopClient::sendCapabilities()
{
    // The Android host advertises its OS and text clipboard; it offers none of the other extensions
    // (power control, screen selection, file clipboard, etc.) that the desktop hosts do.
    proto::control::HostToClient message;
    proto::control::Capabilities* capabilities = message.mutable_capabilities();

    auto add_flag = [capabilities](const char* name)
    {
        proto::control::Capabilities::Flag* flag = capabilities->add_flag();
        flag->set_name(name);
        flag->set_value(true);
    };

    add_flag(kFlagOSAndroid);
    add_flag(kFlagClipboard);

    send(proto::desktop::CHANNEL_ID_CONTROL, serialize(message), true);
}
