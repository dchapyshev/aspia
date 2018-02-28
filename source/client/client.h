//
// PROJECT:         Aspia
// FILE:            client/client.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_H
#define _ASPIA_CLIENT__CLIENT_H

#include "client/client_session.h"
#include "base/threading/thread.h"
#include "network/network_channel.h"
#include "network/network_channel_proxy.h"
#include "proto/auth_session.pb.h"
#include "ui/status_dialog.h"

namespace aspia {

class Client :
    private Thread::Delegate,
    private StatusDialog::Delegate
{
public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;
        virtual void OnSessionTerminate() = 0;
    };

    Client(std::shared_ptr<NetworkChannel> channel,
           const proto::Computer& computer,
           Delegate* delegate);

    ~Client();

    bool IsTerminatedSession() const;

private:
    // Thread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    void OnNetworkChannelStatusChange(NetworkChannel::Status status);

    // StatusDialog::Delegate implementation.
    void OnStatusDialogOpen() override;

    void OnAllowMethodsReceived(const IOBuffer& buffer);
    void OnSelectMethodSended();
    void OnRequestReceived(const IOBuffer& buffer);
    void OnResponseSended();
    void OnResultReceived(const IOBuffer& buffer);

    void OpenStatusDialog();

    Delegate* delegate_;

    Thread ui_thread_;
    std::shared_ptr<MessageLoopProxy> runner_;

    proto::auth::Status status_;
    StatusDialog status_dialog_;

    std::shared_ptr<NetworkChannel> channel_;
    std::shared_ptr<NetworkChannelProxy> channel_proxy_;

    proto::Computer computer_;
    std::unique_ptr<ClientSession> session_;

    DISALLOW_COPY_AND_ASSIGN(Client);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_H
