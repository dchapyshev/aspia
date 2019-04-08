//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef IPC__IPC_CHANNEL_H
#define IPC__IPC_CHANNEL_H

#include "base/macros_magic.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/process_handle.h"
#include "base/win/session_id.h"
#endif // defined(OS_WIN)

#if defined(USE_TBB)
#include <tbb/scalable_allocator.h>
#endif // defined(USE_TBB)

#include <QByteArray>
#include <QLocalSocket>
#include <QPointer>

#include <queue>

namespace ipc {

class Server;

class Channel : public QObject
{
    Q_OBJECT

public:
    ~Channel() = default;

    static Channel* createClient(QObject* parent = nullptr);

    enum class Type { SERVER, CLIENT };
    Type type() const { return type_; }

    void connectToServer(const QString& channel_name);

#if defined(OS_WIN)
    base::ProcessId clientProcessId() const { return client_process_id_; }
    base::ProcessId serverProcessId() const { return server_process_id_; }
    base::win::SessionId clientSessionId() const { return client_session_id_; }
    base::win::SessionId serverSessionId() const { return server_session_id_; }
#endif // defined(OS_WIN)

public slots:
    void stop();

    // Starts reading the message.
    void start();

    // Sends a message.
    void send(const QByteArray& buffer);

signals:
    void connected();
    void disconnected();
    void errorOccurred();
    void messageReceived(const QByteArray& buffer);

private slots:
    void onError(QLocalSocket::LocalSocketError socket_error);
    void onBytesWritten(int64_t bytes);
    void onReadyRead();

private:
    friend class Server;
    Channel(Type type, QLocalSocket* socket, QObject* parent);

    void initConnected();
    void scheduleWrite();

    using MessageSizeType = uint32_t;

    const Type type_;
    QPointer<QLocalSocket> socket_;

#if defined(USE_TBB)
    using QueueAllocator = tbb::scalable_allocator<QByteArray>;
#else // defined(USE_TBB)
    using QueueAllocator = std::allocator<QByteArray>;
#endif // defined(USE_*)

    using QueueContainer = std::deque<QByteArray, QueueAllocator>;

    // The queue contains unencrypted source messages.
    std::queue<QByteArray, QueueContainer> write_queue_;
    MessageSizeType write_size_ = 0;
    int64_t written_ = 0;

    bool read_size_received_ = false;
    QByteArray read_buffer_;
    MessageSizeType read_size_ = 0;
    int64_t read_ = 0;

#if defined(OS_WIN)
    base::ProcessId client_process_id_ = base::kNullProcessId;
    base::ProcessId server_process_id_ = base::kNullProcessId;
    base::win::SessionId client_session_id_ = base::win::kInvalidSessionId;
    base::win::SessionId server_session_id_ = base::win::kInvalidSessionId;
#endif // defined(OS_WIN)

    DISALLOW_COPY_AND_ASSIGN(Channel);
};

} // namespace ipc

#endif // IPC__IPC_CHANNEL_H
