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
    void OnSessionMessage(std::unique_ptr<IOBuffer> buffer) override;
    void OnSessionTerminate() override;

    // NetworkChannel::Listener implementation.
    void OnNetworkChannelMessage(const IOBuffer* buffer) override;
    void OnNetworkChannelDisconnect() override;
    void OnNetworkChannelStarted() override;

    // StatusDialog::Delegate implementation.
    void OnStatusDialogOpen();

    bool ReadAuthResult(const IOBuffer* buffer);
    void CreateSession(proto::SessionType session_type);

    Delegate* delegate_;

    bool is_auth_complete_;
    ClientStatus status_;
    std::unique_ptr<StatusDialog> status_dialog_;

    std::unique_ptr<NetworkChannel> channel_;

    ClientConfig config_;
    std::unique_ptr<ClientSession> session_;
    std::mutex session_lock_;

    DISALLOW_COPY_AND_ASSIGN(Client);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_H
