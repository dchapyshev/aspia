//
// PROJECT:         Aspia
// FILE:            base/win/service.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/win/service.h"
#include "base/logging.h"

namespace aspia {

namespace {

static Service* _self = nullptr;

} // namespace

// public
Service::Service(const wchar_t* service_name) :
    service_name_(service_name)
{
    DCHECK(!_self) << "Another instance of the service has already been created";

    memset(&status_, 0, sizeof(status_));

    _self = this;
}

// public
Service::~Service()
{
    _self = nullptr;
}

// static
DWORD WINAPI Service::ServiceControlHandler(
    DWORD control_code, DWORD /* event_type */, LPVOID /* event_data */, LPVOID context)
{
    switch (control_code)
    {
        case SERVICE_CONTROL_INTERROGATE:
            return NO_ERROR;

        case SERVICE_CONTROL_SHUTDOWN:
        case SERVICE_CONTROL_STOP:
        {
            Service* self = reinterpret_cast<Service*>(context);

            self->terminating_ = true;
            self->OnStop();

            return NO_ERROR;
        }

        default:
            return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

// static
void Service::ServiceMain(DWORD /* argc */, LPWSTR* /* argv */)
{
    DCHECK(_self);

    _self->terminating_ = false;

    _self->status_handle_ =
        RegisterServiceCtrlHandlerExW(_self->service_name_.c_str(),
                                      reinterpret_cast<LPHANDLER_FUNCTION_EX>(ServiceControlHandler),
                                      _self);
    if (!_self->status_handle_)
    {
        PLOG(LS_ERROR) << "RegisterServiceCtrlHandlerExW failed";
        return;
    }

    _self->SetStatus(SERVICE_START_PENDING);
    _self->SetStatus(SERVICE_RUNNING);

    _self->Worker();

    _self->SetStatus(SERVICE_STOP_PENDING);
    _self->SetStatus(SERVICE_STOPPED);
}

// private
void Service::SetStatus(DWORD state)
{
    status_.dwServiceType             = SERVICE_WIN32;
    status_.dwControlsAccepted        = 0;
    status_.dwCurrentState            = state;
    status_.dwWin32ExitCode           = NO_ERROR;
    status_.dwServiceSpecificExitCode = NO_ERROR;
    status_.dwWaitHint                = 0;

    if (state == SERVICE_RUNNING)
    {
        status_.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    }

    if (state != SERVICE_RUNNING && state != SERVICE_STOPPED)
    {
        ++status_.dwCheckPoint;
    }
    else
    {
        status_.dwCheckPoint = 0;
    }

    SetServiceStatus(status_handle_, &status_);
}

bool Service::Run()
{
    SERVICE_TABLE_ENTRYW service_table[1];

    memset(&service_table, 0, sizeof(service_table));

    service_table[0].lpServiceName = &service_name_[0];
    service_table[0].lpServiceProc = ServiceMain;

    if (!StartServiceCtrlDispatcherW(service_table))
    {
        PLOG(LS_ERROR) << "StartServiceCtrlDispatcherW failed";
        return false;
    }

    return true;
}

const std::wstring& Service::ServiceName() const
{
    return service_name_;
}

bool Service::IsTerminating() const
{
    return terminating_;
}

} // namespace aspia
