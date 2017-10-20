//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_local_system_info_pool.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_LOCAL_SYSTEM_INFO_POOL_H
#define _ASPIA_CLIENT__CLIENT_LOCAL_SYSTEM_INFO_POOL_H

#include "base/message_loop/message_loop_proxy.h"
#include "client/client_local_system_info.h"

#include <list>

namespace aspia {

class ClientLocalSystemInfoPool
    : private ClientLocalSystemInfo::Delegate
{
public:
    explicit ClientLocalSystemInfoPool(std::shared_ptr<MessageLoopProxy> runner);
    ~ClientLocalSystemInfoPool();

    void Open();

private:
    // ClientLocalSystemInfo::Delegate implementation.
    void OnClose() override;

    bool terminating_ = false;

    std::shared_ptr<MessageLoopProxy> runner_;
    std::list<std::unique_ptr<ClientLocalSystemInfo>> list_;

    DISALLOW_COPY_AND_ASSIGN(ClientLocalSystemInfoPool);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_LOCAL_SYSTEM_INFO_POOL_H
