//
// PROJECT:         Aspia Remote Desktop
// FILE:            client/client_local_system_info.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__CLIENT_LOCAL_SYSTEM_INFO_H
#define _ASPIA_CLIENT__CLIENT_LOCAL_SYSTEM_INFO_H

#include "base/message_loop/message_loop_thread.h"
#include "ui/system_info/system_info_window.h"

namespace aspia {

class ClientLocalSystemInfo
    : private MessageLoopThread::Delegate,
      private SystemInfoWindow::Delegate
{
public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;
        virtual void OnClose() = 0;
    };

    ClientLocalSystemInfo(Delegate* delegate);
    ~ClientLocalSystemInfo();

    bool IsTerminated() const;

private:
    // MessageLoopThread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    // SystemInfoWindow::Delegate implementation.
    void OnRequest(GuidList list, std::shared_ptr<OutputProxy> output) override;
    void OnWindowClose() override;

    void SendRequest();

    Delegate* delegate_;

    MessageLoopThread thread_;
    std::shared_ptr<MessageLoopProxy> runner_;

    std::unique_ptr<SystemInfoWindow> window_;
    CategoryMap map_;

    GuidList guid_list_;
    std::shared_ptr<OutputProxy> output_;

    bool terminated_ = false;

    DISALLOW_COPY_AND_ASSIGN(ClientLocalSystemInfo);
};

} // namespace aspia

#endif // _ASPIA_CLIENT__CLIENT_LOCAL_SYSTEM_INFO_H
