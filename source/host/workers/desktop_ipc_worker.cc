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

#include "host/workers/desktop_ipc_worker.h"

#include <QTimer>

#include <limits>

#include "base/core_application.h"
#include "base/logging.h"
#include "base/power_controller.h"
#include "base/ipc/ipc_channel.h"
#include "host/desktop_agent_client.h"
#include "host/host_constants.h"

//--------------------------------------------------------------------------------------------------
DesktopIpcWorker::DesktopIpcWorker()
    : Worker(Thread::AsioDispatcher, Seconds(1))
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
DesktopIpcWorker::~DesktopIpcWorker()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void DesktopIpcWorker::onVideoData(const QByteArray& buffer, bool is_key_frame)
{
    for (auto* client : std::as_const(clients_))
        client->onVideoData(buffer, is_key_frame);
}

//--------------------------------------------------------------------------------------------------
void DesktopIpcWorker::onCursorShapeData(const QByteArray& buffer)
{
    for (auto* client : std::as_const(clients_))
        client->onCursorShapeData(buffer);
}

//--------------------------------------------------------------------------------------------------
void DesktopIpcWorker::onCursorPositionData(const QByteArray& buffer)
{
    for (auto* client : std::as_const(clients_))
        client->onCursorPositionData(buffer);
}

//--------------------------------------------------------------------------------------------------
void DesktopIpcWorker::onScreenListData(const QByteArray& buffer)
{
    for (auto* client : std::as_const(clients_))
        client->onScreenListData(buffer);
}

//--------------------------------------------------------------------------------------------------
void DesktopIpcWorker::onScreenTypeData(const QByteArray& buffer)
{
    for (auto* client : std::as_const(clients_))
        client->onScreenTypeData(buffer);
}

//--------------------------------------------------------------------------------------------------
void DesktopIpcWorker::onClipboardData(const QByteArray& buffer)
{
    for (auto* client : std::as_const(clients_))
        client->onClipboardData(buffer);
}

//--------------------------------------------------------------------------------------------------
void DesktopIpcWorker::onAudioData(const QByteArray& buffer)
{
    for (auto* client : std::as_const(clients_))
        client->onAudioData(buffer);
}

//--------------------------------------------------------------------------------------------------
void DesktopIpcWorker::onStart()
{
    LOG(INFO) << "IPC worker started";
    connectToService();
}

//--------------------------------------------------------------------------------------------------
void DesktopIpcWorker::onStop()
{
    LOG(INFO) << "IPC worker stopped";

    for (auto* client : std::as_const(clients_))
    {
        client->disconnect();
        delete client;
    }
    clients_.clear();

    ipc_channel_.reset();
}

//--------------------------------------------------------------------------------------------------
void DesktopIpcWorker::onTimer()
{
    if (clients_.isEmpty())
        return;

    proto::desktop::Overflow::State state = proto::desktop::Overflow::STATE_NONE;

    for (auto* client : std::as_const(clients_))
    {
        if (client->overflowState() > state)
            state = client->overflowState();
    }

    emit sig_overflowStateChanged(state);
}

//--------------------------------------------------------------------------------------------------
void DesktopIpcWorker::onIpcConnected()
{
    ipc_channel_->setPaused(false);
}

//--------------------------------------------------------------------------------------------------
void DesktopIpcWorker::onIpcDisconnected()
{
#if defined(Q_OS_MACOS)
    // The macOS desktop agent is a persistent launchd agent: the service opens and closes the control
    // channel as clients come and go, so a dropped channel is normal - reset and reconnect instead of
    // exiting (which would make KeepAlive respawn the process in a loop). When this agent's session is
    // going away the next session's agent supersedes it on the service (newest connection wins), so
    // this one is harmlessly dropped here and the OS unloads it shortly after.
    LOG(INFO) << "Control channel dropped; resetting and reconnecting";

    for (auto* client : std::as_const(clients_))
    {
        client->disconnect();
        client->deleteLater();
    }
    clients_.clear();

    emit sig_stopCapture();

    QTimer::singleShot(Seconds(1), this, [this]()
    {
        ipc_channel_.reset();
        connectToService();
    });
#else
    // On Windows and Linux the service spawns the agent per attach, so a dropped channel means the
    // session is gone: terminate instead of reconnecting.
    LOG(ERROR) << "IPC channel is disconnected. Terminate application";
    CoreApplication::quit();
#endif // defined(Q_OS_MACOS)
}

//--------------------------------------------------------------------------------------------------
void DesktopIpcWorker::onIpcErrorOccurred()
{
#if defined(Q_OS_MACOS)
    // The macOS desktop agent is launched by launchd, possibly before the service opens the control
    // channel. Recreate the channel and retry (deferred, so the errored channel is not deleted
    // mid-signal) until the service is serving.
    LOG(INFO) << "IPC server not available yet, will retry";
    QTimer::singleShot(Seconds(1), this, [this]()
    {
        ipc_channel_.reset();
        connectToService();
    });
#else
    LOG(ERROR) << "Error when connection to IPC server. Terminate application";
    CoreApplication::quit();
#endif // defined(Q_OS_MACOS)
}

//--------------------------------------------------------------------------------------------------
void DesktopIpcWorker::onIpcMessageReceived(
    quint32 /* ipc_channel_id */, const QByteArray& buffer, bool /* reliable */)
{
    proto::desktop::ServiceToAgent message;
    if (!parse(buffer, &message))
    {
        LOG(ERROR) << "Unable to parse message from service";
        return;
    }

    if (message.has_control())
    {
        const proto::desktop::AgentControl& control = message.control();
        const std::string& command_name = control.command_name();

        LOG(INFO) << "Command:" << command_name;

        if (command_name == "start_client")
        {
            CHECK(control.has_utf8_string());
            startClient(QString::fromStdString(control.utf8_string()));
        }
        else if (command_name == "pause")
        {
            CHECK(control.has_boolean());
            emit sig_paused(control.boolean());
        }
        else if (command_name == "lock_mouse")
        {
            CHECK(control.has_boolean());
            emit sig_mouseLocked(control.boolean());
        }
        else if (command_name == "lock_keyboard")
        {
            CHECK(control.has_boolean());
            emit sig_keyboardLocked(control.boolean());
        }
        else if (command_name == "channel_changed")
        {
            // When changing the connection (for example, when switching from TCP to UDP), a keyframe,
            // resetting the cursor cache, etc. are required.
            onClientConfigured();
        }
        else
        {
            LOG(ERROR) << "Unhandled command:" << command_name;
        }
    }
    else
    {
        LOG(ERROR) << "Unhandled message from service";
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopIpcWorker::onClientConfigured()
{
    proto::control::Config merged_config;
    merged_config.set_effects(true);
    merged_config.set_wallpaper(true);

    bool has_configured = false;
    bool opus_supported = true;
    bool vp8_supported = true;
    bool vp9_supported = true;
    bool h264_supported = true;

    for (auto* client : std::as_const(clients_))
    {
        std::optional<proto::control::Config> config = client->config();
        if (!config.has_value()) // Not configured yet.
            continue;

        has_configured = true;

        // If at least one client does not support a codec, it must be disabled.
        if (!client->isVp8Supported())
            vp8_supported = false;
        if (!client->isVp9Supported())
            vp9_supported = false;
        if (!client->isH264Supported())
            h264_supported = false;
        if (!client->isOpusSupported())
            opus_supported = false;

        merged_config.set_audio(merged_config.audio() || config->audio());
        merged_config.set_effects(merged_config.effects() && config->effects());
        merged_config.set_wallpaper(merged_config.wallpaper() && config->wallpaper());
        merged_config.set_block_input(merged_config.block_input() || config->block_input());
        merged_config.set_lock_at_disconnect(merged_config.lock_at_disconnect() || config->lock_at_disconnect());
        merged_config.set_cursor_position(merged_config.cursor_position() || config->cursor_position());
        merged_config.set_cursor_shape(merged_config.cursor_shape() || config->cursor_shape());

        // The first client that requested a preferred resolution wins; it is carried in the merged
        // config to the screen worker (which itself keeps the first non-empty value).
        if (!merged_config.has_preferred_resolution() && config->has_preferred_resolution() &&
            config->preferred_resolution().width() > 0 && config->preferred_resolution().height() > 0)
        {
            merged_config.mutable_preferred_resolution()->set_width(
                config->preferred_resolution().width());
            merged_config.mutable_preferred_resolution()->set_height(
                config->preferred_resolution().height());
        }
    }

    if (!has_configured)
        return;

    LOG(INFO) << "Merged configuration:" << merged_config << "vp8:" << vp8_supported
              << "vp9:" << vp9_supported << "h264:" << h264_supported << "opus:" << opus_supported;

    is_lock_at_disconnect_ = merged_config.lock_at_disconnect();

    emit sig_configure(merged_config, vp8_supported, vp9_supported, h264_supported);
    emit sig_audioEnabled(merged_config.audio() && opus_supported);
    emit sig_blockInput(merged_config.block_input());
}

//--------------------------------------------------------------------------------------------------
void DesktopIpcWorker::onClientFinished()
{
    DesktopAgentClient* client = dynamic_cast<DesktopAgentClient*>(sender());
    CHECK(client);

    client->disconnect();
    client->deleteLater();
    clients_.removeOne(client);

    if (!clients_.isEmpty())
    {
        onClientConfigured();
        onPreferredSizeChanged();
        onClientBandwidthChanged();
        return;
    }

    LOG(INFO) << "Last desktop client disconnected";
    emit sig_stopCapture();

    if (is_lock_at_disconnect_)
    {
        if (!PowerController::lock())
            LOG(ERROR) << "Unable to lock user session";
        else
            LOG(INFO) << "User session locked";
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopIpcWorker::onPreferredSizeChanged()
{
    if (clients_.isEmpty())
        return;

    QList<QSize> sizes;

    for (auto* client : std::as_const(clients_))
        sizes.emplace_back(client->preferredSize());

    QSize max_size = *std::max_element(sizes.begin(), sizes.end(), [](const QSize& a, const QSize& b)
    {
        return a.width() * a.height() < b.width() * b.height();
    });

    emit sig_preferredSizeChanged(max_size);
}

//--------------------------------------------------------------------------------------------------
void DesktopIpcWorker::onClientBandwidthChanged()
{
    emit sig_bandwidthChanged(minimalBandwidth());
}

//--------------------------------------------------------------------------------------------------
void DesktopIpcWorker::startClient(const QString& ipc_channel_name)
{
    DesktopAgentClient* client = new DesktopAgentClient(this);
    clients_.append(client);

    connect(client, &DesktopAgentClient::sig_injectMouseEvent, this, &DesktopIpcWorker::sig_injectMouseEvent);
    connect(client, &DesktopAgentClient::sig_injectKeyEvent, this, &DesktopIpcWorker::sig_injectKeyEvent);
    connect(client, &DesktopAgentClient::sig_injectTextEvent, this, &DesktopIpcWorker::sig_injectTextEvent);
    connect(client, &DesktopAgentClient::sig_injectTouchEvent, this, &DesktopIpcWorker::sig_injectTouchEvent);
    connect(client, &DesktopAgentClient::sig_selectScreen, this, &DesktopIpcWorker::sig_selectScreen);
    connect(client, &DesktopAgentClient::sig_clipboardEvent, this, &DesktopIpcWorker::sig_clipboardEvent);
    connect(client, &DesktopAgentClient::sig_preferredSizeChanged, this, &DesktopIpcWorker::onPreferredSizeChanged);
    connect(client, &DesktopAgentClient::sig_keyFrameRequested, this, &DesktopIpcWorker::sig_keyFrameRequested);
    connect(client, &DesktopAgentClient::sig_bandwidthChanged, this, &DesktopIpcWorker::onClientBandwidthChanged);
    connect(client, &DesktopAgentClient::sig_configured, this, &DesktopIpcWorker::onClientConfigured);
    connect(client, &DesktopAgentClient::sig_finished, this, &DesktopIpcWorker::onClientFinished);

    LOG(INFO) << "Starting client...";
    client->start(ipc_channel_name);
}

//--------------------------------------------------------------------------------------------------
void DesktopIpcWorker::connectToService()
{
    ipc_channel_ = new IpcChannel(this);
    connect(ipc_channel_, &IpcChannel::sig_connected, this, &DesktopIpcWorker::onIpcConnected);
    connect(ipc_channel_, &IpcChannel::sig_disconnected, this, &DesktopIpcWorker::onIpcDisconnected);
    connect(ipc_channel_, &IpcChannel::sig_errorOccurred, this, &DesktopIpcWorker::onIpcErrorOccurred);
    connect(ipc_channel_, &IpcChannel::sig_messageReceived, this, &DesktopIpcWorker::onIpcMessageReceived);

    ipc_channel_->connectTo(kDesktopAgentChannelId);
}

//--------------------------------------------------------------------------------------------------
qint64 DesktopIpcWorker::minimalBandwidth() const
{
    qint64 minimal_bandwidth = std::numeric_limits<qint64>::max();

    for (auto* client : std::as_const(clients_))
    {
        qint64 bandwidth = client->bandwidth();
        if (bandwidth != 0 && bandwidth < minimal_bandwidth)
            minimal_bandwidth = bandwidth;
    }

    return minimal_bandwidth != std::numeric_limits<qint64>::max() ? minimal_bandwidth : 0;
}
