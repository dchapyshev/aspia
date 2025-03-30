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

#ifndef HOST_FILE_TRANSFER_AGENT_H
#define HOST_FILE_TRANSFER_AGENT_H

#include "base/macros_magic.h"
#include "base/task_runner.h"
#include "base/ipc/ipc_channel.h"
#include "base/memory/serializer.h"
#include "common/file_worker_impl.h"

namespace host {

class FileTransferAgent final : public base::IpcChannel::Listener
{
public:
    explicit FileTransferAgent(std::shared_ptr<base::TaskRunner> task_runner);
    ~FileTransferAgent();

    void start(std::u16string_view channel_id);

protected:
    // base::IpcChannel::Listener implementation.
    void onIpcDisconnected() final;
    void onIpcMessageReceived(const base::ByteArray& buffer) final;
    void onIpcMessageWritten(base::ByteArray&& buffer) final;

private:
    std::shared_ptr<base::TaskRunner> task_runner_;
    std::unique_ptr<base::IpcChannel> channel_;
    std::unique_ptr<common::FileWorkerImpl> worker_;

    base::Serializer serializer_;
    proto::FileRequest request_;
    proto::FileReply reply_;

    DISALLOW_COPY_AND_ASSIGN(FileTransferAgent);
};

} // namespace host

#endif // HOST_FILE_TRANSFER_AGENT_H
