/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/runas.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_BASE__RUNAS_H
#define _ASPIA_BASE__RUNAS_H

#include "aspia_config.h"

#include <wtsapi32.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <userenv.h>
#include <string>
#include <memory>

#include "base/macros.h"
#include "base/scoped_native_library.h"
#include "base/service_win.h"
#include "base/logging.h"

class RunAs : public Service
{
public:
    RunAs(const WCHAR *service_name);
    ~RunAs();

    void Worker() override;
    void OnStart() override;
    void OnStop() override;

private:
    DWORD GetWinlogonProcessId(DWORD session_id);
    HANDLE GetWinlogonUserToken();

private:
    std::unique_ptr<ScopedNativeLibrary> kernel32_;
    std::unique_ptr<ScopedNativeLibrary> wtsapi32_;

    DISALLOW_COPY_AND_ASSIGN(RunAs);
};

#endif // _ASPIA_BASE__RUNAS_H
