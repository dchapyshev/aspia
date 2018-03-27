//
// PROJECT:         Aspia
// FILE:            base/win/service.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__WIN__SERVICE_H
#define _ASPIA_BASE__WIN__SERVICE_H

#include <QtGlobal>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <atomic>
#include <string>

namespace aspia {

class Service
{
public:
    explicit Service(const wchar_t* service_name);
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

    Q_DISABLE_COPY(Service)
};

} // namespace aspia

#endif // _ASPIA_BASE__WIN__SERVICE_H
