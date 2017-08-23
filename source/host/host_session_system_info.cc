//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_session_system_info.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_session_system_info.h"
#include "base/process/process.h"
#include "ipc/pipe_channel_proxy.h"
#include "proto/auth_session.pb.h"

namespace aspia {

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

    ipc_channel_proxy_->Receive(std::bind(
        &HostSessionSystemInfo::OnIpcChannelMessage, this, std::placeholders::_1));
}

void HostSessionSystemInfo::OnIpcChannelMessage(const IOBuffer& buffer)
{
    // TODO
}

} // namespace aspia
