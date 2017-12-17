//
// PROJECT:         Aspia
// FILE:            host/host_session_system_info.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_session_system_info.h"
#include "base/process/process.h"
#include "ipc/pipe_channel_proxy.h"
#include "proto/auth_session.pb.h"
#include "proto/system_info_session.pb.h"
#include "protocol/message_serialization.h"

namespace aspia {

static const size_t kGuidLength = 38;

void HostSessionSystemInfo::Run(const std::wstring& channel_id)
{
    ipc_channel_ = PipeChannel::CreateClient(channel_id);
    if (!ipc_channel_)
        return;

    ipc_channel_proxy_ = ipc_channel_->pipe_channel_proxy();

    uint32_t user_data = Process::Current().Pid();

    if (ipc_channel_->Connect(user_data))
    {
        OnIpcChannelConnect(user_data);

        // Waiting for the connection to close.
        ipc_channel_proxy_->WaitForDisconnect();
    }
}

void HostSessionSystemInfo::OnIpcChannelConnect(uint32_t user_data)
{
    // The server sends the session type in user_data.
    proto::SessionType session_type = static_cast<proto::SessionType>(user_data);

    DCHECK(session_type == proto::SESSION_TYPE_SYSTEM_INFO);

    map_ = std::move(CreateCategoryMap());

    ipc_channel_proxy_->Receive(std::bind(
        &HostSessionSystemInfo::OnIpcChannelMessage, this, std::placeholders::_1));
}

void HostSessionSystemInfo::OnIpcChannelMessage(const IOBuffer& buffer)
{
    proto::system_info::ClientToHost request;

    if (ParseMessage(buffer, request))
    {
        const std::string& guid = request.guid();

        if (guid.length() == kGuidLength && guid.front() == '{' && guid.back() == '}')
        {
            proto::system_info::HostToClient reply;

            reply.set_guid(guid);

            // Looking for a category by GUID.
            const auto category = map_.find(guid);
            if (category != map_.end())
            {
                // TODO: COMPRESSOR_ZLIB support.
                reply.set_compressor(proto::system_info::HostToClient::COMPRESSOR_RAW);
                reply.set_data(category->second->Serialize());
            }

            // Sending response to the request.
            ipc_channel_proxy_->Send(
                SerializeMessage<IOBuffer>(reply),
                std::bind(&HostSessionSystemInfo::OnIpcChannelMessageSended, this));
            return;
        }
    }

    ipc_channel_proxy_->Disconnect();
}

void HostSessionSystemInfo::OnIpcChannelMessageSended()
{
    // The response to the request was sent. Now we can receive the following request.
    ipc_channel_proxy_->Receive(std::bind(
        &HostSessionSystemInfo::OnIpcChannelMessage, this, std::placeholders::_1));
}

} // namespace aspia
