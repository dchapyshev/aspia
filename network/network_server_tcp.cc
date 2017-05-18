//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/network_server_tcp.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "network/network_server_tcp.h"
#include "base/version_helpers.h"
#include "base/path.h"

#include <ws2tcpip.h>

namespace aspia {

static const int kMaxClientCount = 10;

static const WCHAR kAppName[] = L"Aspia Remote Desktop";
static const WCHAR kRuleName[] = L"Aspia Remote Desktop Host";
static const WCHAR kRuleDesc[] = L"Allow incoming connections";

NetworkServerTcp::~NetworkServerTcp()
{
    accept_watcher_.StopWatching();
    server_socket_.Reset();

    if (firewall_manager_)
        firewall_manager_->DeleteRuleByName(kRuleName);

    if (firewall_manager_legacy_)
        firewall_manager_legacy_->DeleteRule();
}

void NetworkServerTcp::AddFirewallRule()
{
    std::wstring exe_path;

    if (!GetPathW(PathKey::FILE_EXE, exe_path))
        return;

    if (IsWindowsVistaOrGreater())
    {
        firewall_manager_.reset(new FirewallManager());

        if (!firewall_manager_->Init(kAppName, exe_path))
        {
            firewall_manager_.reset();
            return;
        }

        if (!firewall_manager_->AddTCPRule(kRuleName, kRuleDesc, port_))
        {
            firewall_manager_.reset();
            return;
        }
    }
    else
    {
        firewall_manager_legacy_.reset(new FirewallManagerLegacy());

        if (!firewall_manager_legacy_->Init(kAppName, exe_path))
        {
            firewall_manager_legacy_.reset();
            return;
        }

        if (!firewall_manager_legacy_->SetAllowIncomingConnection(true))
        {
            firewall_manager_legacy_.reset();
            return;
        }
    }
}

bool NetworkServerTcp::Start(uint16_t port, Delegate* delegate)
{
    DCHECK(delegate);

    if (IsStarted())
    {
        LOG(ERROR) << "Trying to start an already starting server";
        return false;
    }

    runner_ = MessageLoopProxy::Current();
    DCHECK(runner_);

    port_ = port;
    delegate_ = delegate;

    AddFirewallRule();

    server_socket_.Reset(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (!server_socket_.IsValid())
    {
        LOG(ERROR) << "socket() failed: " << WSAGetLastError();
        return false;
    }

    struct sockaddr_in local_addr = { 0 };

    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(port_);
    local_addr.sin_addr = in4addr_any;

    if (bind(server_socket_,
             reinterpret_cast<const struct sockaddr*>(&local_addr),
             sizeof(local_addr)) == SOCKET_ERROR)
    {
        LOG(ERROR) << "bind() failed: " << WSAGetLastError();
        return false;
    }

    if (listen(server_socket_, kMaxClientCount) == SOCKET_ERROR)
    {
        LOG(ERROR) << "listen() failed: " << WSAGetLastError();
        return false;
    }

    if (WSAEventSelect(server_socket_, accept_event_.Handle(), FD_ACCEPT) == SOCKET_ERROR)
    {
        LOG(ERROR) << "WSAEventSelect() failed: " << WSAGetLastError();
        return false;
    }

    return accept_watcher_.StartWatching(accept_event_.Handle(), this);
}

bool NetworkServerTcp::IsStarted() const
{
    return server_socket_.IsValid();
}

void NetworkServerTcp::OnObjectSignaled(HANDLE object)
{
    DCHECK_EQ(object, accept_event_.Handle());

    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&NetworkServerTcp::OnObjectSignaled, this, object));
        return;
    }

    accept_watcher_.StopWatching();

    if (!server_socket_.IsValid())
        return;

    accept_event_.Reset();
    accept_watcher_.StartWatching(accept_event_.Handle(), this);

    WSANETWORKEVENTS events = { 0 };

    if (WSAEnumNetworkEvents(server_socket_, accept_event_.Handle(), &events) == SOCKET_ERROR)
    {
        LOG(ERROR) << "WSAEnumNetworkEvents() failed: " << WSAGetLastError();
    }
    else if ((events.lNetworkEvents & FD_ACCEPT) &&
             (events.iErrorCode[FD_ACCEPT_BIT] == 0))
    {
        Socket client_socket(WSAAccept(server_socket_, nullptr, nullptr, nullptr, 0));

        if (!client_socket.IsValid())
        {
            LOG(ERROR) << "WSAAccept() failed: " << WSAGetLastError();
            return;
        }

        u_long non_blocking = 1;

        if (ioctlsocket(client_socket, FIONBIO, &non_blocking) == SOCKET_ERROR)
        {
            LOG(ERROR) << "ioctlsocket() failed: " << WSAGetLastError();
            return;
        }

        std::unique_ptr<NetworkChannelTcp> channel =
            NetworkChannelTcp::CreateServer(std::move(client_socket));

        if (!channel)
            return;

        delegate_->OnChannelConnected(std::move(channel));
    }
}

} // namespace aspia
