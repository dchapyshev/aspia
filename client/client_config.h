//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_config.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_CONFIG_H
#define _ASPIA_CLIENT__CLIENT_CONFIG_H

#include "client/screen_config.h"

namespace aspia {

class ClientConfig
{
public:
    ClientConfig();
    ~ClientConfig();

    void SetRemoteAddress(const std::string& address);
    const std::string& RemoteAddress() const;

    void SetRemotePort(uint16_t port);
    int RemotePort() const;

    const ScreenConfig& screen_config() const;
    ScreenConfig* mutable_screen_config();

private:
    std::string address_;
    uint16_t port_;
    ScreenConfig screen_;
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_CONFIG_H
