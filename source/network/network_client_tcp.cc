//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/network_client_tcp.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "network/network_client_tcp.h"
#include "base/strings/unicode.h"

namespace aspia {

static const size_t kMaxHostNameLength = 64;

static bool IsFailureCode(const std::error_code& code)
{
    return code.value() != 0;
}

static bool IsValidHostNameChar(wchar_t c)
{
    if (iswalnum(c) != 0)
        return true;

    if (c == L'.' || c == L' ' || c == L'_' || c == L'-')
        return true;

    return false;
}

// static
bool NetworkClientTcp::IsValidHostName(const std::wstring& hostname)
{
    if (hostname.empty())
        return false;

    size_t length = hostname.length();

    if (length > kMaxHostNameLength)
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

NetworkClientTcp::NetworkClientTcp(const std::wstring& address,
                                   uint16_t port,
                                   ConnectCallback connect_callback)
    : connect_callback_(std::move(connect_callback))
{
    asio::ip::tcp::resolver::query query(ANSIfromUNICODE(address), std::to_string(port));

    channel_ = std::make_unique<NetworkChannelTcp>(NetworkChannelTcp::Mode::CLIENT);

    resolver_ = std::make_unique<asio::ip::tcp::resolver>(channel_->io_service());

    resolver_->async_resolve(query,
                             std::bind(&NetworkClientTcp::OnResolve,
                                       this, std::placeholders::_1, std::placeholders::_2));
}

NetworkClientTcp::~NetworkClientTcp()
{
    std::lock_guard<std::mutex> lock(channel_lock_);

    if (channel_)
    {
        channel_->io_service().dispatch(std::bind(&NetworkClientTcp::DoStop, this));
        channel_.reset();
    }
}

void NetworkClientTcp::OnResolve(const std::error_code& code,
                                 asio::ip::tcp::resolver::iterator endpoint_iterator)
{
    if (IsFailureCode(code))
    {
        if (!terminating_)
            connect_callback_(nullptr);
        return;
    }

    std::lock_guard<std::mutex> lock(channel_lock_);

    if (!channel_)
        return;

    channel_->socket().async_connect(
        endpoint_iterator->endpoint(),
        std::bind(&NetworkClientTcp::OnConnect,
                  this, std::placeholders::_1));
}

void NetworkClientTcp::OnConnect(const std::error_code& code)
{
    if (IsFailureCode(code))
    {
        if (!terminating_)
            connect_callback_(nullptr);
        return;
    }

    std::lock_guard<std::mutex> lock(channel_lock_);

    if (!channel_)
        return;

    connect_callback_(std::move(channel_));
}

void NetworkClientTcp::DoStop()
{
    terminating_ = true;

    if (resolver_)
    {
        resolver_->cancel();
        resolver_.reset();
    }
}

} // namespace aspia
