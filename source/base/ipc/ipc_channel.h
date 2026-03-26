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

#include "base/logging.h"
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

    void connectTo(const QString& channel_name);

    bool isConnected() const;
    bool isPaused() const;
    void setPaused(bool enable);

    void send(quint32 channel_id, const QByteArray& buffer);

    quint32 instanceId() const { return instance_id_; }
    const QString& channelName() const { return channel_name_; }
    base::SessionId sessionId() const { return session_id_; }

signals:
    void sig_connected();
    void sig_disconnected();
    void sig_errorOccurred();
    void sig_messageReceived(quint32 channel_id, const QByteArray& buffer);

private:
    friend class IpcServer;

#if defined(Q_OS_WINDOWS)
    using Stream = asio::windows::stream_handle;
#else
    using Stream = asio::local::stream_protocol::socket;
#endif

    IpcChannel(const QString& channel_name, Stream&& stream, QObject* parent);
    static QString channelPath(const QString& channel_name);

    bool connectAttempt();
    void scheduleConnectAttempt(const std::chrono::milliseconds& delay, int count);
    void disconnectFrom();
    void onErrorOccurred(const Location& location, const std::error_code& error_code);
    void onMessageReceived();

    void doWriteHeader();
    void doWriteData();
    void doReadHeader();
    void doReadData();

    class WriteTask
    {
    public:
        WriteTask(quint32 channel_id, const QByteArray& data)
            : channel_id_(channel_id),
              data_(data)
        {
            // Nothing
        }

        WriteTask(const WriteTask& other) = default;
        WriteTask& operator=(const WriteTask& other) = default;

        quint32 channelId() const { return channel_id_; }
        const QByteArray& data() const { return data_; }
        QByteArray& data() { return data_; }

    private:
        quint32 channel_id_;
        QByteArray data_;
    };

    struct Header
    {
        quint32 message_size;
        quint32 channel_id;
    };

    enum class ReadState
    {
        IDLE,        // No reads are in progress right now.
        READ_HEADER, // Reading the message header.
        READ_DATA,   // Reading the contents of the user data.
        PENDING      // There is a message about which we did not notify.
    };

    using WriteQueue = QQueue<WriteTask>;

    const quint32 instance_id_;
    base::SessionId session_id_ = base::kInvalidSessionId;
    QString channel_name_;
    Stream stream_;

    bool is_connected_ = false;
    bool is_paused_ = true;

    WriteQueue write_queue_;
    Header write_header_;

    ReadState read_state_ = ReadState::IDLE;
    Header read_header_;
    QByteArray read_buffer_;

    LOG_DECLARE_CONTEXT(IpcChannel);
    Q_DISABLE_COPY_MOVE(IpcChannel)
};

} // namespace base

#endif // BASE_IPC_CHANNEL_H
