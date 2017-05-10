//
// PROJECT:         Aspia Remote Desktop
// FILE:            network/network_channel_tcp.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__NETWORK_CHANNEL_TCP_H
#define _ASPIA_NETWORK__NETWORK_CHANNEL_TCP_H

#include "network/network_channel.h"
#include "network/wsa_waitable_event.h"
#include "network/socket.h"

#include <winsock2.h>

namespace aspia {

class NetworkChannelTcp : public NetworkChannel
{
public:
    ~NetworkChannelTcp();

    static std::unique_ptr<NetworkChannelTcp> CreateClient(Socket socket);
    static std::unique_ptr<NetworkChannelTcp> CreateServer(Socket socket);

    void Close() override;
    bool IsConnected() const override;

protected:
    bool KeyExchange() override;
    bool WriteData(const uint8_t* buffer, size_t size) override;
    bool ReadData(uint8_t* buffer, size_t size) override;

    enum class Mode { SERVER, CLIENT };

private:
    NetworkChannelTcp(Socket socket, Mode mode);

    bool ClientKeyExchange();
    bool ServerKeyExchange();

    Mode mode_;

    Socket socket_;

    WsaWaitableEvent read_event_;
    WsaWaitableEvent write_event_;

    DISALLOW_COPY_AND_ASSIGN(NetworkChannelTcp);
};

} // namespace aspia

#endif // _ASPIA_NETWORK__NETWORK_CHANNEL_TCP_H
