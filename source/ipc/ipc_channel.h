//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include <QObject>

#include <queue>

class QLocalSocket;

namespace ipc {

class ChannelProxy;
class Listener;
class Server;

class Channel : public QObject
{
    Q_OBJECT

public:
    Channel();
    ~Channel();

    std::shared_ptr<ChannelProxy> channelProxy() { return proxy_; }

    void setListener(Listener* listener);
    void connectToServer(const QString& channel_name);

    bool isConnected() const { return is_connected_; }

    void start();

    // Sends a message.
    void send(const QByteArray& buffer);

#if defined(OS_WIN)
    base::ProcessId peerProcessId() const { return peer_process_id_; }
    base::win::SessionId peerSessionId() const { return peer_session_id_; }
#endif // defined(OS_WIN)

private slots:
    void onBytesWritten(int64_t bytes);
    void onReadyRead();

private:
    friend class Server;
    Channel(QLocalSocket* socket);

    void init();
    void scheduleWrite();

    QLocalSocket* socket_;

    std::shared_ptr<ChannelProxy> proxy_;
    Listener* listener_ = nullptr;
    bool is_connected_ = false;

#if defined(USE_TBB)
    using QueueAllocator = tbb::scalable_allocator<QByteArray>;
#else // defined(USE_TBB)
    using QueueAllocator = std::allocator<QByteArray>;
#endif // defined(USE_*)

    using QueueContainer = std::deque<QByteArray, QueueAllocator>;

    // The queue contains unencrypted source messages.
    std::queue<QByteArray, QueueContainer> write_queue_;
    uint32_t write_size_ = 0;
    int64_t written_ = 0;

    bool read_size_received_ = false;
    QByteArray read_buffer_;
    uint32_t read_size_ = 0;
    int64_t read_ = 0;

#if defined(OS_WIN)
    base::ProcessId peer_process_id_ = base::kNullProcessId;
    base::win::SessionId peer_session_id_ = base::win::kInvalidSessionId;
#endif // defined(OS_WIN)

    DISALLOW_COPY_AND_ASSIGN(Channel);
};

} // namespace ipc

#endif // IPC__IPC_CHANNEL_H
