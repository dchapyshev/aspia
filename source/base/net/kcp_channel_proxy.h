//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_NET_KCP_CHANNEL_PROXY_H
#define BASE_NET_KCP_CHANNEL_PROXY_H

#include "base/net/kcp_channel.h"

#include <shared_mutex>

namespace base {

class TaskRunner;

class KcpChannelProxy : public std::enable_shared_from_this<KcpChannelProxy>
{
public:
    void send(uint8_t channel_id, ByteArray&& buffer);

private:
    friend class KcpChannel;
    KcpChannelProxy(std::shared_ptr<TaskRunner> task_runner, KcpChannel* channel);

    // Called directly by KcpChannel::~KcpChannel.
    void willDestroyCurrentChannel();

    void scheduleWrite();
    bool reloadWriteQueue(std::queue<WriteTask>* work_queue);

    std::shared_ptr<TaskRunner> task_runner_;

    KcpChannel* channel_;

    std::queue<WriteTask> incoming_queue_;
    std::mutex incoming_queue_lock_;

    DISALLOW_COPY_AND_ASSIGN(KcpChannelProxy);
};

} // namespace base

#endif // BASE_NET_KCP_CHANNEL_PROXY_H
