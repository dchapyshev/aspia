//
// PROJECT:         Aspia
// FILE:            host/host_local_system_info.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_LOCAL_SYSTEM_INFO_H
#define _ASPIA_HOST__HOST_LOCAL_SYSTEM_INFO_H

#include "base/message_loop/message_loop_thread.h"
#include "report/report_creator.h"
#include "ui/system_info/system_info_window.h"

namespace aspia {

class HostLocalSystemInfo
    : private MessageLoopThread::Delegate,
      private SystemInfoWindow::Delegate
{
public:
    HostLocalSystemInfo() = default;

    void Run();

private:
    // MessageLoopThread::Delegate implementation.
    void OnBeforeThreadRunning() override;
    void OnAfterThreadRunning() override;

    // SystemInfoWindow::Delegate implementation.
    void OnWindowClose() override;
    void OnRequest(std::string_view guid,
                   std::shared_ptr<ReportCreatorProxy> report_creator) override;

    void ExecuteRequest(std::string_view guid,
                        std::shared_ptr<ReportCreatorProxy> report_creator);

    MessageLoopThread thread_;
    std::shared_ptr<MessageLoopProxy> runner_;
    std::unique_ptr<SystemInfoWindow> window_;
    std::shared_ptr<ReportCreatorProxy> creater_;
    CategoryMap map_;

    DISALLOW_COPY_AND_ASSIGN(HostLocalSystemInfo);
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_LOCAL_SYSTEM_INFO_H
