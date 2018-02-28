//
// PROJECT:         Aspia
// FILE:            client/client_session.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_SESSION_H
#define _ASPIA_CLIENT__CLIENT_SESSION_H

#include "network/network_channel_proxy.h"
#include "proto/computer.pb.h"

namespace aspia {

class ClientSession
{
public:
    ClientSession(const proto::Computer& computer,
                  std::shared_ptr<NetworkChannelProxy> channel_proxy)
        : channel_proxy_(channel_proxy),
          computer_(computer)
    {
        // Nothing
    }

    virtual ~ClientSession() = default;

    static std::unique_ptr<ClientSession> Create(
        const proto::Computer& computer, std::shared_ptr<NetworkChannelProxy> channel_proxy);

protected:
    std::shared_ptr<NetworkChannelProxy> channel_proxy_;
    proto::Computer computer_;

private:
    DISALLOW_COPY_AND_ASSIGN(ClientSession);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_SESSION_H
