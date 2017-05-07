//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/network_client_tcp.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "network/network_client_tcp.h"
#include "network/scoped_addrinfo.h"

namespace aspia {

static const std::chrono::milliseconds kConnectTimeout{ 10000 };

NetworkClientTcp::NetworkClientTcp() :
    delegate_(nullptr)
{
    // Nothing
}

NetworkClientTcp::~NetworkClientTcp()
{
    connect_watcher_.StopWatching();
    socket_.Reset();
}

bool NetworkClientTcp::Connect(const std::wstring& address, uint16_t port, Delegate* delegate)
{
    delegate_ = delegate;

    runner_ = MessageLoopProxy::Current();

    ADDRINFOW hints = { 0 };

    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    ScopedAddrInfo result;

    int ret = GetAddrInfoW(address.c_str(),
                           std::to_wstring(port).c_str(),
                           &hints,
                           result.Recieve());
    if (ret != 0)
    {
        LOG(ERROR) << "getaddrinfo() failed: " << ret;
        return false;
    }

    for (ADDRINFOW* curr = result.Get(); curr != nullptr; curr = curr->ai_next)
    {
        socket_.Reset(socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol));
        if (!socket_.IsValid())
        {
            LOG(ERROR) << "socket() failed: " << WSAGetLastError();
            continue;
        }

        u_long non_blocking = 1;

        if (ioctlsocket(socket_, FIONBIO, &non_blocking) == SOCKET_ERROR)
        {
            LOG(ERROR) << "ioctlsocket() failed: " << WSAGetLastError();
            continue;
        }

        connect_event_.Reset();

        if (WSAEventSelect(socket_, connect_event_.Handle(), FD_CONNECT) == SOCKET_ERROR)
        {
            LOG(ERROR) << "WSAEventSelect() failed: " << WSAGetLastError();
            continue;
        }

        if (WSAConnect(socket_, curr->ai_addr, curr->ai_addrlen,
                       nullptr, nullptr, nullptr, nullptr) == SOCKET_ERROR)
        {
            ret = WSAGetLastError();

            if (ret != WSAEWOULDBLOCK)
            {
                LOG(ERROR) << "WSAConnect() failed: " << ret;
                continue;
            }
        }

        return connect_watcher_.StartTimedWatching(connect_event_.Handle(),
                                                   kConnectTimeout,
                                                   this);
    }

    return false;
}

void NetworkClientTcp::OnObjectSignaled(HANDLE object)
{
    DCHECK_EQ(object, connect_event_.Handle());

    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&NetworkClientTcp::OnObjectSignaled, this, object));
        return;
    }

    connect_watcher_.StopWatching();

    WSANETWORKEVENTS events = { 0 };

    if (WSAEnumNetworkEvents(socket_, connect_event_.Handle(), &events) == SOCKET_ERROR)
    {
        LOG(ERROR) << "WSAEnumNetworkEvents() failed: " << WSAGetLastError();
    }
    else if ((events.lNetworkEvents & FD_CONNECT) &&
             (events.iErrorCode[FD_CONNECT_BIT] == 0))
    {
        std::unique_ptr<NetworkChannel> channel =
            NetworkChannelTcp::CreateClient(std::move(socket_));

        if (channel)
        {
            delegate_->OnConnectionSuccess(std::move(channel));
            return;
        }
    }

    delegate_->OnConnectionError();
}

void NetworkClientTcp::OnObjectTimeout(HANDLE object)
{
    DCHECK_EQ(object, connect_event_.Handle());

    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&NetworkClientTcp::OnObjectTimeout, this, object));
        return;
    }

    connect_watcher_.StopWatching();

    if (socket_.IsValid())
    {
        socket_.Reset();
        delegate_->OnConnectionTimeout();
    }
}

} // namespace aspia
