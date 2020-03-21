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

#include "host/client_session_file_transfer.h"

#include "base/threading/thread.h"
#include "base/win/scoped_impersonator.h"
#include "base/win/scoped_object.h"
#include "common/file_task.h"
#include "common/file_task_producer.h"
#include "common/file_task_producer_proxy.h"
#include "common/file_worker.h"
#include "common/message_serialization.h"
#include "net/network_channel.h"
#include "net/network_channel_proxy.h"
#include "proto/file_transfer.pb.h"

#include <wtsapi32.h>

namespace host {

namespace {

bool createLoggedOnUserToken(DWORD session_id, base::win::ScopedHandle* token_out)
{
    base::win::ScopedHandle user_token;
    if (!WTSQueryUserToken(session_id, user_token.recieve()))
    {
        PLOG(LS_WARNING) << "WTSQueryUserToken failed";
        return false;
    }

    TOKEN_ELEVATION_TYPE elevation_type;
    DWORD returned_length;

    if (!GetTokenInformation(user_token,
                             TokenElevationType,
                             &elevation_type,
                             sizeof(elevation_type),
                             &returned_length))
    {
        PLOG(LS_WARNING) << "GetTokenInformation failed";
        return false;
    }

    switch (elevation_type)
    {
        // The token is a limited token.
        case TokenElevationTypeLimited:
        {
            TOKEN_LINKED_TOKEN linked_token_info;

            // Get the unfiltered token for a silent UAC bypass.
            if (!GetTokenInformation(user_token,
                                     TokenLinkedToken,
                                     &linked_token_info,
                                     sizeof(linked_token_info),
                                     &returned_length))
            {
                PLOG(LS_WARNING) << "GetTokenInformation failed";
                return false;
            }

            // Attach linked token.
            token_out->reset(linked_token_info.LinkedToken);
        }
        break;

        case TokenElevationTypeDefault: // The token does not have a linked token.
        case TokenElevationTypeFull:    // The token is an elevated token.
        default:
            token_out->reset(user_token.release());
            break;
    }

    return true;
}

} // namespace

class ClientSessionFileTransfer::Worker
    : public base::Thread::Delegate,
      public common::FileTaskProducer
{
public:
    Worker(base::SessionId session_id, std::shared_ptr<net::ChannelProxy> channel_proxy);
    ~Worker();

    void start();
    void postRequest(std::unique_ptr<proto::FileRequest> request);

protected:
    // base::Thread::Delegate implementation.
    void onBeforeThreadRunning() override;
    void onAfterThreadRunning() override;

    // common::FileTaskProducer implementation.
    void onTaskDone(std::shared_ptr<common::FileTask> task) override;

private:
    base::Thread thread_;
    const base::SessionId session_id_;
    std::unique_ptr<base::win::ScopedImpersonator> impersonator_;
    std::shared_ptr<net::ChannelProxy> channel_proxy_;
    std::shared_ptr<common::FileTaskProducerProxy> producer_proxy_;
    std::unique_ptr<common::FileWorker> impl_;

    DISALLOW_COPY_AND_ASSIGN(Worker);
};

ClientSessionFileTransfer::Worker::Worker(
    base::SessionId session_id, std::shared_ptr<net::ChannelProxy> channel_proxy)
    : session_id_(session_id),
      channel_proxy_(std::move(channel_proxy))
{
    // Nothing
}

ClientSessionFileTransfer::Worker::~Worker()
{
    thread_.stop();
}

void ClientSessionFileTransfer::Worker::start()
{
    thread_.start(base::MessageLoop::Type::DEFAULT, this);
}

void ClientSessionFileTransfer::Worker::postRequest(std::unique_ptr<proto::FileRequest> request)
{
    if (impl_)
    {
        std::shared_ptr<common::FileTask> task = std::make_shared<common::FileTask>(
            producer_proxy_, std::move(request), common::FileTask::Target::LOCAL);

        impl_->doTask(std::move(task));
    }
    else
    {
        proto::FileReply reply;
        reply.set_error_code(proto::FILE_ERROR_NO_LOGGED_ON_USER);
        channel_proxy_->send(common::serializeMessage(reply));
    }
}

void ClientSessionFileTransfer::Worker::onBeforeThreadRunning()
{
    base::win::ScopedHandle user_token;
    if (!createLoggedOnUserToken(session_id_, &user_token))
        return;

    impersonator_ = std::make_unique<base::win::ScopedImpersonator>();
    if (!impersonator_->loggedOnUser(user_token))
        return;

    producer_proxy_ = std::make_shared<common::FileTaskProducerProxy>(this);
    impl_ = std::make_unique<common::FileWorker>(thread_.taskRunner());
}

void ClientSessionFileTransfer::Worker::onAfterThreadRunning()
{
    if (producer_proxy_)
    {
        producer_proxy_->dettach();
        producer_proxy_.reset();
    }

    impl_.reset();
    impersonator_.reset();
}

void ClientSessionFileTransfer::Worker::onTaskDone(std::shared_ptr<common::FileTask> task)
{
    channel_proxy_->send(common::serializeMessage(task->reply()));
}

ClientSessionFileTransfer::ClientSessionFileTransfer(std::unique_ptr<net::Channel> channel)
    : ClientSession(proto::SESSION_TYPE_FILE_TRANSFER, std::move(channel))
{
    // Nothing
}

ClientSessionFileTransfer::~ClientSessionFileTransfer() = default;

void ClientSessionFileTransfer::onMessageReceived(const base::ByteArray& buffer)
{
    std::unique_ptr<proto::FileRequest> request = std::make_unique<proto::FileRequest>();

    if (!common::parseMessage(buffer, request.get()))
    {
        LOG(LS_ERROR) << "Invalid message from client";
        return;
    }

    if (!worker_)
    {
        worker_ = std::make_unique<Worker>(sessionId(), channelProxy());
        worker_->start();
    }

    worker_->postRequest(std::move(request));
}

void ClientSessionFileTransfer::onMessageWritten()
{
    // Nothing
}

void ClientSessionFileTransfer::onStarted()
{
    // Nothing
}

} // namespace host
