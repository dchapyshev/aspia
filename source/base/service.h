//
// PROJECT:         Aspia
// FILE:            base/service.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SERVICE_H
#define _ASPIA_BASE__SERVICE_H

#include <atomic>
#include <string>

#include "base/macros.h"

namespace aspia {

class Service
{
public:
    explicit Service(const std::wstring& service_name);
    virtual ~Service();

    // The method starts execution of the service and returns control when
    // it stops.
    // If the service was launched and executed, it returns true, if the
    // service execution fails, false is returned.
    bool Run();

    const std::wstring& ServiceName() const;

protected:
    virtual void Worker() = 0;

    //
    // Метод вызывается при остановке службы.
    //
    virtual void OnStop() = 0;

    bool IsTerminating() const;

private:
    static void WINAPI ServiceMain(DWORD argc, LPWSTR* argv);

    static DWORD WINAPI ServiceControlHandler(DWORD control_code,
                                              DWORD event_type,
                                              LPVOID event_data,
                                              LPVOID context);

    void SetStatus(DWORD state);

    std::wstring service_name_;
    std::atomic_bool terminating_;

    SERVICE_STATUS_HANDLE status_handle_ = nullptr;
    SERVICE_STATUS status_;

    DISALLOW_COPY_AND_ASSIGN(Service);
};

} // namespace aspia

#endif // _ASPIA_BASE__SERVICE_H
