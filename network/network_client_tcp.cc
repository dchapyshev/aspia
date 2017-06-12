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
static const size_t kMaximumHostNameLength = 64;

static bool IsValidHostNameChar(wchar_t c)
{
    if (isalnum(c) != 0)
        return true;

    if (c == '.' || c == ' ' || c == '_' || c == '-')
        return true;

    return false;
}

// static
bool NetworkClientTcp::IsValidHostName(const std::wstring& hostname)
{
    if (hostname.empty())
        return false;

    size_t length = hostname.length();

    if (length > kMaximumHostNameLength)
        return false;

    for (size_t i = 0; i < length; ++i)
    {
        if (!IsValidHostNameChar(hostname[i]))
            return false;
    }

    return true;
}

// static
bool NetworkClientTcp::IsValidPort(uint16_t port)
{
    if (port == 0 || port >= 65535)
        return false;

    return true;
}

NetworkClientTcp::NetworkClientTcp(std::shared_ptr<MessageLoopProxy> runner) :
    connect_event_(WaitableEvent::ResetPolicy::MANUAL,
                   WaitableEvent::InitialState::NOT_SIGNALED),
    runner_(runner)
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
    if (!IsValidHostName(address) || !IsValidPort(port))
        return false;

    delegate_ = delegate;

    ADDRINFOW hints;
    memset(&hints, 0, sizeof(hints));

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
            LOG(ERROR) << "socket() failed: " << GetLastSystemErrorString();
            continue;
        }

        u_long non_blocking = 1;

        if (ioctlsocket(socket_, FIONBIO, &non_blocking) == SOCKET_ERROR)
        {
            LOG(ERROR) << "ioctlsocket() failed: " << GetLastSystemErrorString();
            continue;
        }

        connect_event_.Reset();

        if (WSAEventSelect(socket_, connect_event_.Handle(), FD_CONNECT) == SOCKET_ERROR)
        {
            LOG(ERROR) << "WSAEventSelect() failed: " << GetLastSystemErrorString();
            continue;
        }

        if (WSAConnect(socket_, curr->ai_addr, static_cast<int>(curr->ai_addrlen),
                       nullptr, nullptr, nullptr, nullptr) == SOCKET_ERROR)
        {
            ret = WSAGetLastError();

            if (ret != WSAEWOULDBLOCK)
            {
                LOG(ERROR) << "WSAConnect() failed: " << SystemErrorCodeToString(ret);
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

    WSANETWORKEVENTS events;
    memset(&events, 0, sizeof(events));

    if (WSAEnumNetworkEvents(socket_, connect_event_.Handle(), &events) == SOCKET_ERROR)
    {
        LOG(ERROR) << "WSAEnumNetworkEvents() failed: " << GetLastSystemErrorString();
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
