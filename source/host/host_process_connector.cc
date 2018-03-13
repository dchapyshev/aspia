//
// PROJECT:         Aspia
// FILE:            host/host_process_connector.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_process_connector.h"

namespace aspia {

HostProcessConnector::HostProcessConnector(
    std::shared_ptr<NetworkChannelProxy> network_channel_proxy)
    : network_channel_proxy_(std::move(network_channel_proxy))
{
    // Start receiving messages from the network channel.
    network_channel_proxy_->Receive(std::bind(
        &HostProcessConnector::OnNetworkChannelMessage, this, std::placeholders::_1));
}

HostProcessConnector::~HostProcessConnector()
{
    Disconnect();

    {
        std::scoped_lock<std::mutex> lock(ipc_channel_proxy_lock_);
        ipc_channel_proxy_ = nullptr;
    }

    ready_event_.notify_one();
}

bool HostProcessConnector::StartServer(std::wstring& channel_id)
{
    ipc_channel_ = PipeChannel::CreateServer(channel_id);
    if (!ipc_channel_)
        return false;

    return true;
}

bool HostProcessConnector::Accept(uint32_t& user_data,
                                  PipeChannel::DisconnectHandler disconnect_handler)
{
    // Connecting to the host process.
    if (ipc_channel_->Connect(user_data, std::move(disconnect_handler)))
    {
        {
            std::scoped_lock<std::mutex> lock(ipc_channel_proxy_lock_);
            ipc_channel_proxy_ = ipc_channel_->pipe_channel_proxy();
        }

        {
            std::scoped_lock<std::mutex> lock(ready_lock_);
            ready_ = true;
        }

        // The channel is ready to receive incoming messages.
        ready_event_.notify_one();

        // Start receiving messages from the IPC channel.
        ipc_channel_proxy_->Receive(std::bind(
            &HostProcessConnector::OnIpcChannelMessage, this,
            std::placeholders::_1));

        return true;
    }

    return false;
}

void HostProcessConnector::Disconnect()
{
    // Now the channel is not ready to receive incoming messages.
    {
        std::scoped_lock<std::mutex> lock(ready_lock_);
        ready_ = false;
    }

    ipc_channel_.reset();
}

void HostProcessConnector::OnIpcChannelMessage(QByteArray& buffer)
{
    network_channel_proxy_->Send(std::move(buffer));

    std::scoped_lock<std::mutex> lock(ipc_channel_proxy_lock_);

    if (!ipc_channel_proxy_)
        return;

    // Receive next message.
    ipc_channel_proxy_->Receive(std::bind(
        &HostProcessConnector::OnIpcChannelMessage, this, std::placeholders::_1));
}

void HostProcessConnector::OnNetworkChannelMessage(QByteArray& buffer)
{
    // A network channel can start receiving messages before connecting IPC
    // channel. We are waiting for the connection of IPC channel. If the
    // channel is already connected, the call returns immediately.
    {
        std::unique_lock<std::mutex> lock(ready_lock_);

        while (!ready_)
            ready_event_.wait(lock);
    }

    {
        std::scoped_lock<std::mutex> lock(ipc_channel_proxy_lock_);

        if (!ipc_channel_proxy_)
            return;

        ipc_channel_proxy_->Send(std::move(buffer));
    }

    // Receive next message.
    network_channel_proxy_->Receive(std::bind(
        &HostProcessConnector::OnNetworkChannelMessage, this, std::placeholders::_1));
}

} // namespace aspia
