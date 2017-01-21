//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/runas_service.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__RUNAS_SERVICE_H
#define _ASPIA_BASE__RUNAS_SERVICE_H

#include "aspia_config.h"

#include <memory>

#include "base/macros.h"
#include "base/scoped_kernel32_library.h"
#include "base/scoped_wtsapi32_library.h"
#include "base/service.h"

namespace aspia {

//
// Класс для запуска процесса от имени пользователя "SYSTEM".
//
class RunAsService : private Service
{
public:
    RunAsService();
    ~RunAsService();

    void DoService();

    static bool InstallAndStartService();

private:
    void Worker() override;
    void OnStop() override;

    DWORD GetWinlogonProcessId(DWORD session_id);
    HANDLE GetWinlogonUserToken();

private:
    ScopedKernel32Library kernel32_;
    ScopedWtsApi32Library wtsapi32_;

    DISALLOW_COPY_AND_ASSIGN(RunAsService);
};

} // namespace aspia

#endif // _ASPIA_BASE__RUNAS_SERVICE_H
