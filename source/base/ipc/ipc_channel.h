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

#ifndef BASE_IPC_CHANNEL_H
#define BASE_IPC_CHANNEL_H

#include <QByteArray>
#include <QObject>
#include <QQueue>

#if defined(Q_OS_WINDOWS)
#include <asio/windows/stream_handle.hpp>
#else
#include <asio/local/stream_protocol.hpp>
#endif

#include "base/session_id.h"

namespace base {

class IpcServer;
class Location;

class IpcChannel final : public QObject
{
    Q_OBJECT

public:
    explicit IpcChannel(QObject* parent = nullptr);
    ~IpcChannel();

    bool connectTo(const QString& channel_id);

    void disconnectFrom();

    bool isConnected() const;
    bool isPaused() const;

    void pause();
    void resume();

    void send(const QByteArray& buffer);

    SessionId peerSessionId() const { return peer_session_id_; }

signals:
    void sig_disconnected();
    void sig_messageReceived(const QByteArray& buffer);

private:
    friend class IpcServer;

#if defined(Q_OS_WINDOWS)
    using Stream = asio::windows::stream_handle;
#else
    using Stream = asio::local::stream_protocol::socket;
#endif

    IpcChannel(const QString& channel_name, Stream&& stream, QObject* parent);
    static QString channelName(const QString& channel_id);

    void onErrorOccurred(const Location& location, const std::error_code& error_code);
    void onMessageReceived();

    void doWriteSize();
    void doWriteData();
    void doReadSize();
    void doReadData();

    QString channel_name_;
    Stream stream_;

    bool is_connected_ = false;
    bool is_paused_ = true;

    QQueue<QByteArray> write_queue_;
    quint32 write_size_ = 0;

    quint32 read_size_ = 0;
    QByteArray read_buffer_;

    SessionId peer_session_id_ = kInvalidSessionId;

    Q_DISABLE_COPY(IpcChannel)
};

} // namespace base

#endif // BASE_IPC_CHANNEL_H
