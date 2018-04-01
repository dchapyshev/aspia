//
// PROJECT:         Aspia
// FILE:            host/host_session_file_transfer.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_session_file_transfer.h"

#include <QDebug>

#include "base/process/process.h"
#include "ipc/pipe_channel_proxy.h"
#include "protocol/message_serialization.h"
#include "proto/auth_session.pb.h"

namespace aspia {

void HostSessionFileTransfer::Run(const std::wstring& channel_id)
{
    ipc_channel_ = PipeChannel::CreateClient(channel_id);
    if (ipc_channel_)
    {
        ipc_channel_proxy_ = ipc_channel_->pipe_channel_proxy();

        uint32_t user_data = Process::Current().Pid();

        if (ipc_channel_->Connect(user_data))
        {
            OnIpcChannelConnect(user_data);
            ipc_channel_proxy_->WaitForDisconnect();
        }

        ipc_channel_.reset();
    }
}

void HostSessionFileTransfer::OnIpcChannelConnect(uint32_t user_data)
{
    // The server sends the session type in user_data.
    proto::auth::SessionType session_type =
        static_cast<proto::auth::SessionType>(user_data);

    if (session_type != proto::auth::SESSION_TYPE_FILE_TRANSFER)
    {
        qFatal("Invalid session type passed");
        return;
    }

    ipc_channel_proxy_->Receive(std::bind(
        &HostSessionFileTransfer::OnIpcChannelMessage, this, std::placeholders::_1));
}

void HostSessionFileTransfer::OnIpcChannelMessage(const QByteArray& buffer)
{
    proto::file_transfer::Request request;

    if (!ParseMessage(buffer, request))
    {
        ipc_channel_proxy_->Disconnect();
        return;
    }

    SendReply(worker_.doRequest(request));
}

void HostSessionFileTransfer::SendReply(const proto::file_transfer::Reply& reply)
{
    ipc_channel_proxy_->Send(
        SerializeMessage(reply),
        std::bind(&HostSessionFileTransfer::OnReplySended, this));
}

void HostSessionFileTransfer::OnReplySended()
{
    // Receive next request.
    ipc_channel_proxy_->Receive(std::bind(
        &HostSessionFileTransfer::OnIpcChannelMessage, this, std::placeholders::_1));
}

} // namespace aspia
