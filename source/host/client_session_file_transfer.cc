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

#include "host/client_session_file_transfer.h"

#include "build/build_config.h"
#include "base/logging.h"
#include "base/net/network_channel_proxy.h"
#include "base/threading/thread.h"
#include "common/file_task.h"
#include "common/file_task_producer.h"
#include "common/file_task_producer_proxy.h"
#include "common/file_worker.h"
#include "proto/file_transfer.pb.h"

#if defined(OS_WIN)
#include "base/win/scoped_impersonator.h"
#include "base/win/scoped_object.h"

#include <WtsApi32.h>
#endif // defined(OS_WIN)

namespace host {

namespace {

#if defined(OS_WIN)
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
#endif // defined(OS_WIN)

} // namespace

class ClientSessionFileTransfer::Worker
    : public base::Thread::Delegate,
      public common::FileTaskProducer
{
public:
    Worker(base::SessionId session_id, std::shared_ptr<base::NetworkChannelProxy> channel_proxy);
    ~Worker() override;

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
    std::shared_ptr<base::NetworkChannelProxy> channel_proxy_;
    std::shared_ptr<common::FileTaskProducerProxy> producer_proxy_;
    std::unique_ptr<common::FileWorker> impl_;

#if defined(OS_WIN)
    std::unique_ptr<base::win::ScopedImpersonator> impersonator_;
#endif // defined(OS_WIN)

    DISALLOW_COPY_AND_ASSIGN(Worker);
};

ClientSessionFileTransfer::Worker::Worker(
    base::SessionId session_id, std::shared_ptr<base::NetworkChannelProxy> channel_proxy)
    : session_id_(session_id),
      channel_proxy_(std::move(channel_proxy))
{
    LOG(LS_INFO) << "Ctor";
}

ClientSessionFileTransfer::Worker::~Worker()
{
    LOG(LS_INFO) << "Dtor";
    thread_.stop();
}

void ClientSessionFileTransfer::Worker::start()
{
    LOG(LS_INFO) << "Start worker";
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
        channel_proxy_->send(proto::HOST_CHANNEL_ID_SESSION, base::serialize(reply));
    }
}

void ClientSessionFileTransfer::Worker::onBeforeThreadRunning()
{
    LOG(LS_INFO) << "After thread running";

#if defined(OS_WIN)
    base::win::ScopedHandle user_token;
    if (!createLoggedOnUserToken(session_id_, &user_token))
    {
        LOG(LS_WARNING) << "createLoggedOnUserToken failed";
        return;
    }

    impersonator_ = std::make_unique<base::win::ScopedImpersonator>();
    if (!impersonator_->loggedOnUser(user_token))
    {
        LOG(LS_WARNING) << "loggedOnUser failed";
        return;
    }
#endif // defined(OS_WIN)

    producer_proxy_ = std::make_shared<common::FileTaskProducerProxy>(this);
    impl_ = std::make_unique<common::FileWorker>(thread_.taskRunner());
}

void ClientSessionFileTransfer::Worker::onAfterThreadRunning()
{
    LOG(LS_INFO) << "Before thread running";

    if (producer_proxy_)
    {
        producer_proxy_->dettach();
        producer_proxy_.reset();
    }
    else
    {
        LOG(LS_WARNING) << "Invalid producer proxy";
    }

    impl_.reset();

#if defined(OS_WIN)
    impersonator_.reset();
#endif // defined(OS_WIN)
}

void ClientSessionFileTransfer::Worker::onTaskDone(std::shared_ptr<common::FileTask> task)
{
    channel_proxy_->send(proto::HOST_CHANNEL_ID_SESSION, base::serialize(task->reply()));
}

ClientSessionFileTransfer::ClientSessionFileTransfer(std::unique_ptr<base::NetworkChannel> channel)
    : ClientSession(proto::SESSION_TYPE_FILE_TRANSFER, std::move(channel))
{
    LOG(LS_INFO) << "Ctor";
}

ClientSessionFileTransfer::~ClientSessionFileTransfer()
{
    LOG(LS_INFO) << "Dtor";
}

void ClientSessionFileTransfer::onStarted()
{
    // Nothing
}

void ClientSessionFileTransfer::onReceived(uint8_t /* channel_id */, const base::ByteArray& buffer)
{
    std::unique_ptr<proto::FileRequest> request = std::make_unique<proto::FileRequest>();

    if (!base::parse(buffer, request.get()))
    {
        LOG(LS_ERROR) << "Invalid message from client";
        return;
    }

    if (!worker_)
    {
        LOG(LS_INFO) << "Create worker";

        worker_ = std::make_unique<Worker>(sessionId(), channelProxy());
        worker_->start();
    }

    worker_->postRequest(std::move(request));
}

void ClientSessionFileTransfer::onWritten(uint8_t /* channel_id */, size_t /* pending */)
{
    // Nothing
}

} // namespace host
