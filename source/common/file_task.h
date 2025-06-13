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

#ifndef COMMON_FILE_TASK_H
#define COMMON_FILE_TASK_H

#include <QMetaType>
#include <QPointer>

#include "base/shared_pointer.h"
#include "proto/file_transfer.h"

namespace common {

class FileTaskFactory;

class FileTask
{
public:
    enum class Target
    {
        LOCAL, // Local task.
        REMOTE // Remote task.
    };

    FileTask() = default;
    FileTask(QPointer<FileTaskFactory> factory, proto::file_transfer::Request&& request, Target target);
    ~FileTask();

    // Returns the target of the current request. It can be a local computer or a remote computer.
    Target target() const;

    // Returns the data of the current request.
    const proto::file_transfer::Request& request() const;

    // Returns reply data for the current request.
    // If method setReply has not been called and data has not been set, an empty reply will be
    // returned.
    const proto::file_transfer::Reply& reply() const;

    // Sets the reply to the current request. The sender will be notified of this reply.
    void onReply(proto::file_transfer::Reply&& reply);

private:
    class Data
    {
    public:
        Data(QPointer<FileTaskFactory> factory, proto::file_transfer::Request&& request, Target target)
            : factory(std::move(factory)),
              target(target),
              request(std::move(request))
        {
            // Nothing
        }

        QPointer<FileTaskFactory> factory;
        FileTask::Target target;
        proto::file_transfer::Request request;
        proto::file_transfer::Reply reply;
    };

    base::SharedPointer<Data> data_;
};

} // namespace common

Q_DECLARE_METATYPE(common::FileTask)
Q_DECLARE_METATYPE(common::FileTask::Target)

#endif // COMMON_FILE_TASK_H
