//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef IPC__IPC_CHANNEL_PROXY_H
#define IPC__IPC_CHANNEL_PROXY_H

#include "ipc/ipc_channel.h"

namespace base {
class TaskRunner;
} // namespace base

namespace ipc {

class ChannelProxy : public std::enable_shared_from_this<ChannelProxy>
{
public:
    void send(base::ByteArray&& buffer);

private:
    friend class Channel;
    ChannelProxy(std::shared_ptr<base::TaskRunner> task_runner, Channel* channel);

    // Called directly by Channel::~Channel.
    void willDestroyCurrentChannel();

    void scheduleWrite();
    bool reloadWriteQueue(base::ScalableQueue<base::ByteArray>* work_queue);

    std::shared_ptr<base::TaskRunner> task_runner_;
    Channel* channel_;

    base::ScalableQueue<base::ByteArray> incoming_queue_;
    std::mutex incoming_queue_lock_;

    DISALLOW_COPY_AND_ASSIGN(ChannelProxy);
};

} // namespace ipc

#endif // IPC__IPC_CHANNEL_PROXY_H
