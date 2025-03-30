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

#ifndef BASE_IPC_IPC_CHANNEL_PROXY_H
#define BASE_IPC_IPC_CHANNEL_PROXY_H

#include "base/ipc/ipc_channel.h"

namespace base {

class TaskRunner;

class IpcChannelProxy : public std::enable_shared_from_this<IpcChannelProxy>
{
public:
    ~IpcChannelProxy();

    void send(ByteArray&& buffer);

private:
    friend class IpcChannel;
    IpcChannelProxy(std::shared_ptr<TaskRunner> task_runner, IpcChannel* channel);

    // Called directly by IpcChannel::~IpcChannel.
    void willDestroyCurrentChannel();

    void scheduleWrite();
    bool reloadWriteQueue(std::queue<ByteArray>* work_queue);

    std::shared_ptr<TaskRunner> task_runner_;
    IpcChannel* channel_;

    std::queue<ByteArray> incoming_queue_;
    std::mutex incoming_queue_lock_;

    DISALLOW_COPY_AND_ASSIGN(IpcChannelProxy);
};

} // namespace base

#endif // BASE_IPC_IPC_CHANNEL_PROXY_H
