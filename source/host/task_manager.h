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

#ifndef HOST_TASK_MANAGER_H
#define HOST_TASK_MANAGER_H

#include "base/macros_magic.h"
#include "proto/task_manager.h"

#include <memory>

namespace host {

class ProcessMonitor;

class TaskManager
{
public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onTaskManagerMessage(const proto::task_manager::HostToClient& message) = 0;
    };

    explicit TaskManager(Delegate* delegate);
    ~TaskManager();

    void readMessage(const proto::task_manager::ClientToHost& message);

private:
    void sendProcessList(quint32 flags);
    void sendServiceList();
    void sendUserList();

    std::unique_ptr<ProcessMonitor> process_monitor_;
    Delegate* delegate_;

    DISALLOW_COPY_AND_ASSIGN(TaskManager);
};

} // namespace host

#endif // HOST_TASK_MANAGER_H
