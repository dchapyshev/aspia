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

#include "host/file_transfer_agent.h"

#include "base/logging.h"

namespace host {

//--------------------------------------------------------------------------------------------------
FileTransferAgent::FileTransferAgent(std::shared_ptr<base::TaskRunner> task_runner)
    : task_runner_(std::move(task_runner)),
      worker_(std::make_unique<common::FileWorkerImpl>())
{
    LOG(LS_INFO) << "Ctor";
    DCHECK(task_runner_);
}

//--------------------------------------------------------------------------------------------------
FileTransferAgent::~FileTransferAgent()
{
    LOG(LS_INFO) << "Dtor";
}

void FileTransferAgent::start(std::u16string_view channel_id)
{
    LOG(LS_INFO) << "Starting with channel id: " << channel_id.data();

    channel_ = std::make_unique<base::IpcChannel>();

    if (!channel_->connect(channel_id))
    {
        LOG(LS_ERROR) << "Connection failed";
        return;
    }

    channel_->setListener(this);
    channel_->resume();
}

//--------------------------------------------------------------------------------------------------
void FileTransferAgent::onIpcDisconnected()
{
    LOG(LS_INFO) << "IPC channel disconnected";
    task_runner_->postQuit();
}

//--------------------------------------------------------------------------------------------------
void FileTransferAgent::onIpcMessageReceived(const base::ByteArray& buffer)
{
    request_.Clear();
    reply_.Clear();

    if (!base::parse(buffer, &request_))
    {
        LOG(LS_ERROR) << "Unable to parse message";
        return;
    }

    worker_->doRequest(request_, &reply_);
    channel_->send(base::serialize(reply_));
}

} // namespace host
