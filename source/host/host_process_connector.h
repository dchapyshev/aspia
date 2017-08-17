//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_process_connector.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_PROCESS_CONNECTOR_H
#define _ASPIA_HOST__HOST_PROCESS_CONNECTOR_H

#include "base/synchronization/waitable_event.h"
#include "network/network_channel_proxy.h"
#include "ipc/pipe_channel_proxy.h"

namespace aspia {

class HostProcessConnector
{
public:
    HostProcessConnector(
        std::shared_ptr<NetworkChannelProxy> network_channel_proxy);
    ~HostProcessConnector();

    bool StartServer(std::wstring& channel_id);

    bool Accept(uint32_t& user_data, PipeChannel::DisconnectHandler disconnect_handler);

    void Disconnect();

private:
    void OnIpcChannelMessage(IOBuffer& buffer);
    void OnNetworkChannelMessage(IOBuffer& buffer);

    std::shared_ptr<NetworkChannelProxy> network_channel_proxy_;

    std::unique_ptr<PipeChannel> ipc_channel_;
    std::shared_ptr<PipeChannelProxy> ipc_channel_proxy_;
    std::mutex ipc_channel_proxy_lock_;

    WaitableEvent ready_event_{ WaitableEvent::ResetPolicy::MANUAL,
                                WaitableEvent::InitialState::NOT_SIGNALED };

    DISALLOW_COPY_AND_ASSIGN(HostProcessConnector);
};

}

#endif // _ASPIA_HOST__HOST_PROCESS_CONNECTOR_H
