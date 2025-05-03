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

#include "base/process_handle.h"
#include "base/session_id.h"
#include "base/memory/local_memory.h"
#include "base/threading/thread_checker.h"

#if defined(OS_WIN)
#include <asio/windows/stream_handle.hpp>
#elif defined(OS_POSIX)
#include <asio/local/stream_protocol.hpp>
#endif

#include <filesystem>
#include <queue>

#include <QByteArray>
#include <QObject>

namespace base {

class IpcChannelProxy;
class IpcServer;
class Location;

class IpcChannel final : public QObject
{
    Q_OBJECT

public:
    explicit IpcChannel(QObject* parent = nullptr);
    ~IpcChannel();

    std::shared_ptr<IpcChannelProxy> channelProxy();

    [[nodiscard]]
    bool connect(const QString& channel_id);

    void disconnect();

    bool isConnected() const;
    bool isPaused() const;

    void pause();
    void resume();

    void send(QByteArray&& buffer);

    ProcessId peerProcessId() const { return peer_process_id_; }
    SessionId peerSessionId() const { return peer_session_id_; }
    std::filesystem::path peerFilePath() const;

signals:
    void sig_disconnected();
    void sig_messageReceived(const QByteArray& buffer);

private:
    friend class IpcServer;
    friend class IpcChannelProxy;

#if defined(OS_WIN)
    using Stream = asio::windows::stream_handle;
#elif defined(OS_POSIX)
    using Stream = asio::local::stream_protocol::socket;
#endif

    IpcChannel(const QString& channel_name, Stream&& stream, QObject* parent);
    static QString channelName(const QString& channel_id);

    void onErrorOccurred(const Location& location, const std::error_code& error_code);
    void doWriteSize();
    void onWriteSize(const std::error_code& error_code, size_t bytes_transferred);
    void doWriteData();
    void onWriteData(const std::error_code& error_code, size_t bytes_transferred);
    void doReadSize();
    void onReadSize(const std::error_code& error_code, size_t bytes_transferred);
    void doReadData();
    void onReadData(const std::error_code& error_code, size_t bytes_transferred);

    void onMessageReceived();

    QString channel_name_;
    Stream stream_;

    std::shared_ptr<IpcChannelProxy> proxy_;

    bool is_connected_ = false;
    bool is_paused_ = true;

    std::queue<QByteArray> write_queue_;
    quint32 write_size_ = 0;

    quint32 read_size_ = 0;
    QByteArray read_buffer_;

    ProcessId peer_process_id_ = kNullProcessId;
    SessionId peer_session_id_ = kInvalidSessionId;

    class Handler;
    base::local_shared_ptr<Handler> handler_;

    THREAD_CHECKER(thread_checker_);

    DISALLOW_COPY_AND_ASSIGN(IpcChannel);
};

} // namespace base

#endif // BASE_IPC_CHANNEL_H
