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

#include "host/workers/portable_desktop_worker.h"

#include <algorithm>
#include <limits>

#include "base/logging.h"
#include "host/desktop_agent_client.h"

//--------------------------------------------------------------------------------------------------
PortableDesktopWorker::PortableDesktopWorker()
    : Worker(Thread::AsioDispatcher, Seconds(1))
{
    LOG(TRACE) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
PortableDesktopWorker::~PortableDesktopWorker()
{
    LOG(TRACE) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void PortableDesktopWorker::onVideoData(const QByteArray& buffer, bool is_key_frame)
{
    for (const ClientEntry& entry : std::as_const(clients_))
        entry.client->onVideoData(buffer, is_key_frame);
}

//--------------------------------------------------------------------------------------------------
void PortableDesktopWorker::onCursorShapeData(const QByteArray& buffer)
{
    for (const ClientEntry& entry : std::as_const(clients_))
        entry.client->onCursorShapeData(buffer);
}

//--------------------------------------------------------------------------------------------------
void PortableDesktopWorker::onCursorPositionData(const QByteArray& buffer)
{
    for (const ClientEntry& entry : std::as_const(clients_))
        entry.client->onCursorPositionData(buffer);
}

//--------------------------------------------------------------------------------------------------
void PortableDesktopWorker::onScreenListData(const QByteArray& buffer)
{
    for (const ClientEntry& entry : std::as_const(clients_))
        entry.client->onScreenListData(buffer);
}

//--------------------------------------------------------------------------------------------------
void PortableDesktopWorker::onScreenTypeData(const QByteArray& buffer)
{
    for (const ClientEntry& entry : std::as_const(clients_))
        entry.client->onScreenTypeData(buffer);
}

//--------------------------------------------------------------------------------------------------
void PortableDesktopWorker::onAudioData(const QByteArray& buffer)
{
    for (const ClientEntry& entry : std::as_const(clients_))
        entry.client->onAudioData(buffer);
}

//--------------------------------------------------------------------------------------------------
void PortableDesktopWorker::onClientStarted(quint32 client_id)
{
    if (indexOfClient(client_id) != -1)
    {
        LOG(ERROR) << "Client already started:" << client_id;
        return;
    }

    DesktopAgentClient* client = new DesktopAgentClient(this);
    clients_.append({ client_id, client });

    connect(client, &DesktopAgentClient::sig_injectMouseEvent, this, &PortableDesktopWorker::sig_injectMouseEvent);
    connect(client, &DesktopAgentClient::sig_injectKeyEvent, this, &PortableDesktopWorker::sig_injectKeyEvent);
    connect(client, &DesktopAgentClient::sig_injectTextEvent, this, &PortableDesktopWorker::sig_injectTextEvent);
    connect(client, &DesktopAgentClient::sig_injectTouchEvent, this, &PortableDesktopWorker::sig_injectTouchEvent);
    connect(client, &DesktopAgentClient::sig_selectScreen, this, &PortableDesktopWorker::sig_selectScreen);
    connect(client, &DesktopAgentClient::sig_preferredSizeChanged, this, &PortableDesktopWorker::onPreferredSizeChanged);
    connect(client, &DesktopAgentClient::sig_keyFrameRequested, this, &PortableDesktopWorker::sig_keyFrameRequested);
    connect(client, &DesktopAgentClient::sig_bandwidthChanged, this, &PortableDesktopWorker::onClientBandwidthChanged);
    connect(client, &DesktopAgentClient::sig_configured, this, &PortableDesktopWorker::onClientConfigured);
    connect(client, &DesktopAgentClient::sig_sendMessage, this,
            [this, client_id](quint32 channel_id, const QByteArray& buffer, bool reliable)
    {
        emit sig_clientMessage(client_id, channel_id, buffer, reliable);
    });

    LOG(INFO) << "Client started:" << client_id;
}

//--------------------------------------------------------------------------------------------------
void PortableDesktopWorker::onClientMessage(quint32 client_id, quint32 channel_id, const QByteArray& buffer)
{
    const qsizetype index = indexOfClient(client_id);
    if (index == -1)
    {
        LOG(ERROR) << "Message for unknown client:" << client_id;
        return;
    }

    clients_[index].client->onMessageReceived(channel_id, buffer, true);
}

//--------------------------------------------------------------------------------------------------
void PortableDesktopWorker::onClientFinished(quint32 client_id)
{
    const qsizetype index = indexOfClient(client_id);
    if (index == -1)
    {
        LOG(ERROR) << "Unknown client finished:" << client_id;
        return;
    }

    DesktopAgentClient* client = clients_.takeAt(index).client;

    LOG(INFO) << "Client finished:" << client_id;

    client->disconnect();
    client->deleteLater();

    if (!clients_.isEmpty())
    {
        onClientConfigured();
        onPreferredSizeChanged();
        onClientBandwidthChanged();
        return;
    }

    LOG(INFO) << "Last desktop client disconnected";
    lastClientFinished();
}

//--------------------------------------------------------------------------------------------------
void PortableDesktopWorker::onClientChannelChanged()
{
    // When changing the connection (for example, when switching from TCP to UDP), a keyframe,
    // resetting the cursor cache, etc. are required.
    onClientConfigured();
}

//--------------------------------------------------------------------------------------------------
void PortableDesktopWorker::onUserPause(bool enable)
{
    emit sig_paused(enable);
}

//--------------------------------------------------------------------------------------------------
void PortableDesktopWorker::onUserLockMouse(bool enable)
{
    emit sig_mouseLocked(enable);
}

//--------------------------------------------------------------------------------------------------
void PortableDesktopWorker::onUserLockKeyboard(bool enable)
{
    emit sig_keyboardLocked(enable);
}

//--------------------------------------------------------------------------------------------------
void PortableDesktopWorker::onStart()
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void PortableDesktopWorker::onStop()
{
    for (const ClientEntry& entry : std::as_const(clients_))
    {
        entry.client->disconnect();
        entry.client->deleteLater();
    }
    clients_.clear();
}

//--------------------------------------------------------------------------------------------------
void PortableDesktopWorker::onTimer(TimePoint /* now */)
{
    if (clients_.isEmpty())
        return;

    proto::desktop::Overflow::State state = proto::desktop::Overflow::STATE_NONE;

    for (const ClientEntry& entry : std::as_const(clients_))
    {
        if (entry.client->overflowState() > state)
            state = entry.client->overflowState();
    }

    emit sig_overflowStateChanged(state);
}

//--------------------------------------------------------------------------------------------------
void PortableDesktopWorker::onClientConfigured()
{
    proto::control::Config merged_config;
    merged_config.set_effects(true);
    merged_config.set_wallpaper(true);

    bool has_configured = false;
    bool opus_supported = true;
    bool vp8_supported = true;
    bool vp9_supported = true;

    for (const ClientEntry& entry : std::as_const(clients_))
    {
        std::optional<proto::control::Config> config = entry.client->config();
        if (!config.has_value()) // Not configured yet.
            continue;

        has_configured = true;

        // If at least one client does not support a codec, it must be disabled.
        if (!entry.client->isVp8Supported())
            vp8_supported = false;
        if (!entry.client->isVp9Supported())
            vp9_supported = false;
        if (!entry.client->isOpusSupported())
            opus_supported = false;

        merged_config.set_audio(merged_config.audio() || config->audio());
        merged_config.set_effects(merged_config.effects() && config->effects());
        merged_config.set_wallpaper(merged_config.wallpaper() && config->wallpaper());
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
              << "vp9:" << vp9_supported << "opus:" << opus_supported;

    emit sig_configure(merged_config, vp8_supported, vp9_supported);
    emit sig_audioEnabled(merged_config.audio() && opus_supported);
}

//--------------------------------------------------------------------------------------------------
void PortableDesktopWorker::onPreferredSizeChanged()
{
    if (clients_.isEmpty())
        return;

    QList<QSize> sizes;

    for (const ClientEntry& entry : std::as_const(clients_))
        sizes.emplace_back(entry.client->preferredSize());

    QSize max_size = *std::max_element(sizes.begin(), sizes.end(), [](const QSize& a, const QSize& b)
    {
        return a.width() * a.height() < b.width() * b.height();
    });

    emit sig_preferredSizeChanged(max_size);
}

//--------------------------------------------------------------------------------------------------
void PortableDesktopWorker::onClientBandwidthChanged()
{
    emit sig_bandwidthChanged(minimalBandwidth());
}

//--------------------------------------------------------------------------------------------------
qsizetype PortableDesktopWorker::indexOfClient(quint32 client_id) const
{
    for (qsizetype i = 0; i < clients_.size(); ++i)
    {
        if (clients_[i].id == client_id)
            return i;
    }

    return -1;
}

//--------------------------------------------------------------------------------------------------
void PortableDesktopWorker::lastClientFinished()
{
    emit sig_stopCapture();
    emit sig_paused(false);
    emit sig_mouseLocked(false);
    emit sig_keyboardLocked(false);
}

//--------------------------------------------------------------------------------------------------
qint64 PortableDesktopWorker::minimalBandwidth() const
{
    qint64 minimal_bandwidth = std::numeric_limits<qint64>::max();

    for (const ClientEntry& entry : std::as_const(clients_))
    {
        qint64 bandwidth = entry.client->bandwidth();
        if (bandwidth != 0 && bandwidth < minimal_bandwidth)
            minimal_bandwidth = bandwidth;
    }

    return minimal_bandwidth != std::numeric_limits<qint64>::max() ? minimal_bandwidth : 0;
}
