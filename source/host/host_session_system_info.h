//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_session_system_info.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_SESSION_SYSTEM_INFO_H
#define _ASPIA_HOST__HOST_SESSION_SYSTEM_INFO_H

#include "ipc/pipe_channel.h"

#include <map>

namespace aspia {

class HostSessionSystemInfo
{
public:
    HostSessionSystemInfo() = default;
    ~HostSessionSystemInfo() = default;

    void Run(const std::wstring& channel_id);

private:
    void OnIpcChannelConnect(uint32_t user_data);
    void OnIpcChannelMessage(const IOBuffer& buffer);
    void OnIpcChannelMessageSended();

    void RegisterSupportedCategories();

    std::unique_ptr<PipeChannel> ipc_channel_;
    std::shared_ptr<PipeChannelProxy> ipc_channel_proxy_;

    std::map<std::string, std::function<std::string()>> map_;

    DISALLOW_COPY_AND_ASSIGN(HostSessionSystemInfo);
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_SESSION_SYSTEM_INFO_H
