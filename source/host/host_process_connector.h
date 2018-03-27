//
// PROJECT:         Aspia
// FILE:            host/host_process_connector.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_PROCESS_CONNECTOR_H
#define _ASPIA_HOST__HOST_PROCESS_CONNECTOR_H

#include <condition_variable>
#include <mutex>

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
    void OnIpcChannelMessage(QByteArray& buffer);
    void OnNetworkChannelMessage(QByteArray& buffer);

    std::shared_ptr<NetworkChannelProxy> network_channel_proxy_;

    std::unique_ptr<PipeChannel> ipc_channel_;
    std::shared_ptr<PipeChannelProxy> ipc_channel_proxy_;
    std::mutex ipc_channel_proxy_lock_;

    std::condition_variable ready_event_;
    std::mutex ready_lock_;
    bool ready_ = false;

    Q_DISABLE_COPY(HostProcessConnector)
};

}

#endif // _ASPIA_HOST__HOST_PROCESS_CONNECTOR_H
