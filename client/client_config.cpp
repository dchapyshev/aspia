//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_config.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_config.h"

namespace aspia {

ClientConfig::ClientConfig() :
    port_(11011)
{
    // Nothing
}

ClientConfig::~ClientConfig()
{
    // Nothing
}

void ClientConfig::SetRemoteAddress(const std::string &address)
{
    address_ = address;
}

const std::string& ClientConfig::RemoteAddress() const
{
    return address_;
}

void ClientConfig::SetRemotePort(int port)
{
    port_ = port;
}

int ClientConfig::RemotePort() const
{
    return port_;
}

const ScreenConfig& ClientConfig::screen_config() const
{
    return screen_;
}

ScreenConfig* ClientConfig::mutable_screen_config()
{
    return &screen_;
}

} // namespace aspia
