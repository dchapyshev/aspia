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

#ifndef BASE_NET_TCP_CHANNEL_PROXY_H
#define BASE_NET_TCP_CHANNEL_PROXY_H

#include "base/net/tcp_channel.h"

#include <mutex>

namespace base {

class TaskRunner;

class TcpChannelProxy : public std::enable_shared_from_this<TcpChannelProxy>
{
public:
    void send(quint8 channel_id, QByteArray&& buffer);

private:
    friend class TcpChannel;
    TcpChannelProxy(std::shared_ptr<TaskRunner> task_runner, TcpChannel* channel);

    // Called directly by TcpChannel::~TcpChannel.
    void willDestroyCurrentChannel();

    void scheduleWrite();
    bool reloadWriteQueue(WriteQueue* work_queue);

    std::shared_ptr<TaskRunner> task_runner_;

    TcpChannel* channel_;

    WriteQueue incoming_queue_;
    std::mutex incoming_queue_lock_;

    DISALLOW_COPY_AND_ASSIGN(TcpChannelProxy);
};

} // namespace base

#endif // BASE_NET_TCP_CHANNEL_PROXY_H
