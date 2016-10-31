/*
* PROJECT:         Aspia Remote Desktop
* FILE:            base/service_win.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_BASE__SERVICE_WIN_H
#define _ASPIA_BASE__SERVICE_WIN_H

#include "aspia_config.h"

#include <memory>

#include "base/macros.h"
#include "base/event.h"
#include "base/logging.h"

class Service
{
public:
    Service(const WCHAR *service_name);
    virtual ~Service();

    bool Start();

    virtual void Worker() = 0;
    virtual void OnStart() = 0;
    virtual void OnStop() = 0;

private:
    static void WINAPI ServiceMain(int argc, LPWSTR argv);

    static DWORD WINAPI ServiceControlHandler(DWORD control_code,
                                              DWORD event_type,
                                              LPVOID event_data,
                                              LPVOID context);

    void SetStatus(DWORD state);

private:
    std::wstring service_name_;

    SERVICE_STATUS_HANDLE status_handle_;
    SERVICE_STATUS status_;

    DISALLOW_COPY_AND_ASSIGN(Service);
};

#endif // _ASPIA_BASE__SERVICE_WIN_H
