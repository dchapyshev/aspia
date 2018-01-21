//
// PROJECT:         Aspia
// FILE:            host/host_service.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_SERVICE_H
#define _ASPIA_HOST__HOST_SERVICE_H

#include "base/message_loop/message_loop_proxy.h"
#include "base/service.h"
#include "host/host_pool.h"

namespace aspia {

class HostService : public Service
{
public:
    HostService();

    static bool IsInstalled();
    static bool Install();
    static bool Remove();

private:
    // Service implementation.
    void Worker() override;
    void OnStop() override;

    std::shared_ptr<MessageLoopProxy> runner_;

    DISALLOW_COPY_AND_ASSIGN(HostService);
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_SERVICE_H
