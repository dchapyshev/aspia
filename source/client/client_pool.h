//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_pool.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_POOL_H
#define _ASPIA_CLIENT__CLIENT_POOL_H

#include "base/message_loop/message_loop_proxy.h"
#include "client/client.h"
#include "client/client_config.h"
#include "network/network_client_tcp.h"
#include "ui/status_dialog.h"

#include <list>

namespace aspia {

class ClientPool
    : private StatusDialog::Delegate,
      private Client::Delegate
{
public:
    explicit ClientPool(std::shared_ptr<MessageLoopProxy> runner);
    ~ClientPool();

    void Connect(HWND parent, const ClientConfig& config);

private:
    // StatusDialog::Delegate implementation.
    void OnStatusDialogOpen() override;

    void OnConnect(std::shared_ptr<NetworkChannel> channel);

    // Client::Delegate implementation.
    void OnSessionTerminate() override;

    bool terminating_ = false;

    ClientConfig config_;
    StatusDialog status_dialog_;

    std::shared_ptr<MessageLoopProxy> runner_;
    std::unique_ptr<NetworkClientTcp> network_client_;
    std::list<std::unique_ptr<Client>> session_list_;

    DISALLOW_COPY_AND_ASSIGN(ClientPool);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_POOL_H
