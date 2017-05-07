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

    void SetRemoteAddress(const std::wstring& address);
    const std::wstring& RemoteAddress() const;

    void SetRemotePort(uint16_t port);
    uint16_t RemotePort() const;

    void SetSessionType(proto::SessionType session_type);
    proto::SessionType SessionType() const;

    const proto::DesktopConfig& desktop_config() const;
    proto::DesktopConfig* mutable_desktop_config();

    ClientConfig& operator=(const ClientConfig& other);

private:
    std::wstring address_;
    uint16_t port_;

    proto::SessionType session_type_;
    proto::DesktopConfig desktop_config_;
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_CONFIG_H
