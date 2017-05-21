//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_config.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_CONFIG_H
#define _ASPIA_CLIENT__CLIENT_CONFIG_H

#include "proto/desktop_session.pb.h"
#include "proto/auth_session.pb.h"

namespace aspia {

class ClientConfig
{
public:
    ClientConfig();
    ClientConfig(const ClientConfig& other);
    ~ClientConfig() = default;

    void CopyFrom(const ClientConfig& other);

    void set_address(const std::wstring& address);
    const std::wstring& address() const;

    void set_port(uint16_t port);
    uint16_t port() const;

    void set_session_type(proto::SessionType session_type);
    proto::SessionType session_type() const;

    const proto::DesktopSessionConfig& desktop_session_config() const;
    proto::DesktopSessionConfig* mutable_desktop_session_config();

    ClientConfig& operator=(const ClientConfig& other);

    void SetDefaultDesktopSessionConfig();

private:
    std::wstring address_;
    uint16_t port_ = kDefaultHostTcpPort;

    proto::SessionType session_type_ = proto::SessionType::SESSION_TYPE_UNKNOWN;
    proto::DesktopSessionConfig desktop_session_config_;
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_CONFIG_H
