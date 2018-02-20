//
// PROJECT:         Aspia
// FILE:            network/network_server_tcp.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "network/network_server_tcp.h"
#include "base/files/base_paths.h"

namespace aspia {

namespace {

constexpr wchar_t kAppName[] = L"Aspia";
constexpr wchar_t kRuleName[] = L"Aspia Host";
constexpr wchar_t kRuleDesc[] = L"Allow incoming connections";

bool IsFailureCode(const std::error_code& code)
{
    return code.value() != 0;
}

} // namespace

NetworkServerTcp::NetworkServerTcp(uint16_t port, ConnectCallback connect_callback)
    : connect_callback_(std::move(connect_callback)),
      port_(port)
{
    thread_.Start(MessageLoop::TYPE_DEFAULT, this);
}

NetworkServerTcp::~NetworkServerTcp()
{
    thread_.Stop();
}

void NetworkServerTcp::AddFirewallRule()
{
    std::experimental::filesystem::path exe_path;

    if (!GetBasePath(BasePathKey::FILE_EXE, exe_path))
        return;

    firewall_manager_ = std::make_unique<FirewallManager>();

    if (!firewall_manager_->Init(kAppName, exe_path))
    {
        firewall_manager_.reset();
        return;
    }

    if (!firewall_manager_->AddTCPRule(kRuleName, kRuleDesc, port_))
    {
        firewall_manager_.reset();
    }
}


void NetworkServerTcp::OnBeforeThreadRunning()
{
    runner_ = thread_.message_loop_proxy();
    DCHECK(runner_);

    AddFirewallRule();
    DoAccept();
}

void NetworkServerTcp::OnAfterThreadRunning()
{
    {
        std::lock_guard<std::mutex> lock(channel_lock_);

        if (channel_)
        {
            channel_->io_service().dispatch(std::bind(&NetworkServerTcp::DoStop, this));
            channel_.reset();
        }
    }

    if (firewall_manager_)
        firewall_manager_->DeleteRuleByName(kRuleName);
}

void NetworkServerTcp::OnAccept(const std::error_code& code)
{
    if (!runner_->BelongsToCurrentThread())
    {
        runner_->PostTask(std::bind(&NetworkServerTcp::OnAccept, this, code));
        return;
    }

    if (IsFailureCode(code))
    {
        DLOG(LS_ERROR) << "accept failure: " << code.message();
        return;
    }

    if (!acceptor_ || !acceptor_->is_open())
        return;

    {
        std::lock_guard<std::mutex> lock(channel_lock_);

        if (!channel_)
            return;

        connect_callback_(std::move(channel_));
    }

    DoAccept();
}

void NetworkServerTcp::DoAccept()
{
    DCHECK(runner_->BelongsToCurrentThread());

    std::lock_guard<std::mutex> lock(channel_lock_);

    channel_ = std::make_unique<NetworkChannelTcp>(NetworkChannelTcp::Mode::SERVER);

    acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(
        channel_->io_service(),
        asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port_));

    acceptor_->async_accept(channel_->socket(),
                            std::bind(&NetworkServerTcp::OnAccept, this, std::placeholders::_1));
}

void NetworkServerTcp::DoStop()
{
    if (acceptor_)
    {
        std::error_code ignored_code;
        acceptor_->close(ignored_code);
        acceptor_.reset();
    }
}

} // namespace aspia
