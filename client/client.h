//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_H
#define _ASPIA_CLIENT__CLIENT_H

#include "client/client_session.h"
#include "network/network_channel.h"
#include "network/network_channel_proxy.h"
#include "proto/auth_session.pb.h"
#include "ui/status_dialog.h"

#include <mutex>

namespace aspia {

class Client :
    private NetworkChannel::Listener,
    private ClientSession::Delegate,
    private StatusDialog::Delegate
{
public:
    class Delegate
    {
    public:
        virtual void OnSessionTerminate() = 0;
    };

    Client(std::unique_ptr<NetworkChannel> channel,
           const ClientConfig& config,
           Delegate* delegate);

    ~Client();

    bool IsAliveSession() const;

private:
    // ClientSession::Delegate implementation.
    void OnSessionMessageAsync(IOBuffer buffer) override;
    void OnSessionMessage(const IOBuffer& buffer) override;
    void OnSessionTerminate() override;

    // NetworkChannel::Listener implementation.
    void OnNetworkChannelMessage(const IOBuffer& buffer) override;
    void OnNetworkChannelDisconnect() override;
    void OnNetworkChannelStarted() override;

    // StatusDialog::Delegate implementation.
    void OnStatusDialogOpen();

    bool ReadAuthResult(const IOBuffer& buffer);
    void CreateSession(proto::SessionType session_type);

    Delegate* delegate_;

    bool is_auth_complete_ = false;
    ClientStatus status_;
    StatusDialog status_dialog_;

    std::unique_ptr<NetworkChannel> channel_;
    std::shared_ptr<NetworkChannelProxy> channel_proxy_;

    ClientConfig config_;
    std::unique_ptr<ClientSession> session_;
    std::mutex session_lock_;

    DISALLOW_COPY_AND_ASSIGN(Client);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_H
