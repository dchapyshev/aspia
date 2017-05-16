//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_config.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_config.h"

namespace aspia {

ClientConfig::ClientConfig(const ClientConfig& other)
{
    CopyFrom(other);
}

void ClientConfig::CopyFrom(const ClientConfig& other)
{
    address_ = other.address_;
    port_ = other.port_;
    session_type_ = other.session_type_;

    desktop_session_config_.CopyFrom(other.desktop_session_config_);
}

void ClientConfig::set_address(const std::wstring& address)
{
    address_ = address;
}

const std::wstring& ClientConfig::address() const
{
    return address_;
}

void ClientConfig::set_session_type(proto::SessionType session_type)
{
    session_type_ = session_type;
}

proto::SessionType ClientConfig::session_type() const
{
    return session_type_;
}

void ClientConfig::set_port(uint16_t port)
{
    port_ = port;
}

uint16_t ClientConfig::port() const
{
    return port_;
}

const proto::DesktopSessionConfig& ClientConfig::desktop_session_config() const
{
    return desktop_session_config_;
}

proto::DesktopSessionConfig* ClientConfig::mutable_desktop_session_config()
{
    return &desktop_session_config_;
}

ClientConfig& ClientConfig::operator=(const ClientConfig& other)
{
    CopyFrom(other);
    return *this;
}

} // namespace aspia
