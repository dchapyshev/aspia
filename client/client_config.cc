//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_config.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_config.h"

namespace aspia {

ClientConfig::ClientConfig() :
    port_(kDefaultHostTcpPort),
    session_type_(proto::SessionType::SESSION_NONE)
{
    // Nothing
}

ClientConfig::ClientConfig(const ClientConfig& other)
{
    CopyFrom(other);
}

void ClientConfig::CopyFrom(const ClientConfig& other)
{
    address_ = other.address_;
    port_ = other.port_;
    session_type_ = other.session_type_;

    desktop_config_.CopyFrom(other.desktop_config_);
}

void ClientConfig::SetRemoteAddress(const std::wstring& address)
{
    address_ = address;
}

const std::wstring& ClientConfig::RemoteAddress() const
{
    return address_;
}

void ClientConfig::SetSessionType(proto::SessionType session_type)
{
    session_type_ = session_type;
}

proto::SessionType ClientConfig::SessionType() const
{
    return session_type_;
}

void ClientConfig::SetRemotePort(uint16_t port)
{
    port_ = port;
}

uint16_t ClientConfig::RemotePort() const
{
    return port_;
}

const proto::DesktopConfig& ClientConfig::desktop_config() const
{
    return desktop_config_;
}

proto::DesktopConfig* ClientConfig::mutable_desktop_config()
{
    return &desktop_config_;
}

ClientConfig& ClientConfig::operator=(const ClientConfig& other)
{
    CopyFrom(other);
    return *this;
}

} // namespace aspia
