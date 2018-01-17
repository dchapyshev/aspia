//
// PROJECT:         Aspia
// FILE:            client/client_config.h
// LICENSE:         Mozilla Public License Version 2.0
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

    void set_session_type(proto::auth::SessionType session_type);
    proto::auth::SessionType session_type() const;

    const proto::SessionConfig& desktop_session_config() const;
    proto::SessionConfig* mutable_desktop_session_config();

    ClientConfig& operator=(const ClientConfig& other);

    void SetDefaultDesktopSessionConfig();

private:
    std::wstring address_;
    uint16_t port_ = kDefaultHostTcpPort;

    proto::auth::SessionType session_type_ =
        proto::auth::SessionType::SESSION_TYPE_UNKNOWN;

    proto::SessionConfig desktop_session_config_;
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_CONFIG_H
