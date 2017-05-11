//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/network_client_tcp.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "network/network_client_tcp.h"
#include "base/unicode.h"

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

    runner_ = MessageLoopProxy::Current();
    DCHECK(runner_);

    std::string address_ansi;
    CHECK(UNICODEtoANSI(address, address_ansi));

    sockaddr_in dest_addr;

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    dest_addr.sin_addr.s_addr = inet_addr(address_ansi.c_str());

    if (dest_addr.sin_addr.s_addr == INADDR_NONE)
    {
        HOSTENT* host = gethostbyname(address_ansi.c_str());

        if (!host)
            return false;

        ((ULONG *) &dest_addr.sin_addr)[0] = ((ULONG **) host->h_addr_list)[0][0];
    }

    socket_.Reset(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (!socket_.IsValid())
    {
        LOG(ERROR) << "socket() failed: " << WSAGetLastError();
        return false;
    }

    u_long non_blocking = 1;

    if (ioctlsocket(socket_, FIONBIO, &non_blocking) == SOCKET_ERROR)
    {
        LOG(ERROR) << "ioctlsocket() failed: " << WSAGetLastError();
        return false;
    }

    connect_event_.Reset();

    if (WSAEventSelect(socket_, connect_event_.Handle(), FD_CONNECT) == SOCKET_ERROR)
    {
        LOG(ERROR) << "WSAEventSelect() failed: " << WSAGetLastError();
        return false;
    }

    if (WSAConnect(socket_,
                   reinterpret_cast<const sockaddr*>(&dest_addr),
                   sizeof(dest_addr),
                   nullptr, nullptr, nullptr, nullptr) == SOCKET_ERROR)
    {
        int ret = WSAGetLastError();

        if (ret != WSAEWOULDBLOCK)
        {
            LOG(ERROR) << "WSAConnect() failed: " << ret;
            return false;
        }
    }

    return connect_watcher_.StartTimedWatching(connect_event_.Handle(),
                                               kConnectTimeout,
                                               this);
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
