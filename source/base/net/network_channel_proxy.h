//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__NET__NETWORK_CHANNEL_PROXY_H
#define BASE__NET__NETWORK_CHANNEL_PROXY_H

#include "base/net/network_channel.h"

#include <shared_mutex>

namespace base {

class TaskRunner;

class NetworkChannelProxy : public std::enable_shared_from_this<NetworkChannelProxy>
{
public:
    void send(ByteArray&& buffer);

private:
    friend class NetworkChannel;
    NetworkChannelProxy(std::shared_ptr<TaskRunner> task_runner, NetworkChannel* channel);

    // Called directly by NetworkChannel::~NetworkChannel.
    void willDestroyCurrentChannel();

    void scheduleWrite();
    bool reloadWriteQueue(std::queue<WriteTask>* work_queue);

    std::shared_ptr<TaskRunner> task_runner_;

    NetworkChannel* channel_;

    std::queue<WriteTask> incoming_queue_;
    std::mutex incoming_queue_lock_;

    DISALLOW_COPY_AND_ASSIGN(NetworkChannelProxy);
};

} // namespace base

#endif // BASE__NET__NETWORK_CHANNEL_PROXY_H
