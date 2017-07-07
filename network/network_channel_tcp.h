//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/network_channel_tcp.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__NETWORK_CHANNEL_TCP_H
#define _ASPIA_NETWORK__NETWORK_CHANNEL_TCP_H

#include "network/network_channel.h"
#include "network/socket.h"
#include "base/synchronization/waitable_event.h"

#include <winsock2.h>

namespace aspia {

class NetworkChannelTcp : public NetworkChannel
{
public:
    ~NetworkChannelTcp();

    static std::unique_ptr<NetworkChannelTcp> CreateClient(Socket socket);
    static std::unique_ptr<NetworkChannelTcp> CreateServer(Socket socket);

protected:
    bool KeyExchange() override;
    bool WriteData(const uint8_t* buffer, size_t size) override;
    bool ReadData(uint8_t* buffer, size_t size) override;
    void Close() override;

    enum class Mode { SERVER, CLIENT };

private:
    NetworkChannelTcp(Socket socket, Mode mode);

    bool ClientKeyExchange();
    bool ServerKeyExchange();

    Mode mode_;

    Socket socket_;

    WaitableEvent read_event_;
    WaitableEvent write_event_;

    DISALLOW_COPY_AND_ASSIGN(NetworkChannelTcp);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__NETWORK_CHANNEL_TCP_H
