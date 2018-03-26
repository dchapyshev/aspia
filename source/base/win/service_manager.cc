//
// PROJECT:         Aspia
// FILE:            base/win/service_manager.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/win/service_manager.h"
#include "base/logging.h"

namespace aspia {

ServiceManager::ServiceManager(const wchar_t* service_short_name) :
    sc_manager_(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS))
{
    if (!sc_manager_.IsValid())
    {
        PLOG(LS_ERROR) << "OpenSCManagerW failed";
    }
    else
    {
        service_ = OpenServiceW(sc_manager_, service_short_name, SERVICE_ALL_ACCESS);
        if (!service_.IsValid())
        {
            PLOG(LS_ERROR) << "OpenServiceW failed";
            sc_manager_.Reset();
        }
    }
}

// private
ServiceManager::ServiceManager(SC_HANDLE sc_manager, SC_HANDLE service) :
    sc_manager_(sc_manager),
    service_(service)
{
    // Nothing
}

ServiceManager::~ServiceManager()
{
    service_.Reset();
    sc_manager_.Reset();
}

// static
std::unique_ptr<ServiceManager>
ServiceManager::Create(const CommandLine& command_line,
                       const wchar_t* service_full_name,
                       const wchar_t* service_short_name,
                       const wchar_t* service_description)
{
    // Open the service manager.
    ScopedScHandle sc_manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
    if (!sc_manager)
    {
        PLOG(LS_ERROR) << "OpenSCManagerW failed";
        return nullptr;
    }

    // Trying to create a service.
    ScopedScHandle service(CreateServiceW(sc_manager,
                                          service_short_name,
                                          service_full_name,
                                          SERVICE_ALL_ACCESS,
                                          SERVICE_WIN32_OWN_PROCESS,
                                          SERVICE_AUTO_START,
                                          SERVICE_ERROR_NORMAL,
                                          command_line.GetCommandLineString().c_str(),
                                          nullptr,
                                          nullptr,
                                          nullptr,
                                          nullptr,
                                          nullptr));
    if (!service.IsValid())
    {
        PLOG(LS_ERROR) << "CreateServiceW failed";
        return nullptr;
    }

    if (service_description)
    {
        SERVICE_DESCRIPTIONW description;
        description.lpDescription = const_cast<LPWSTR>(service_description);

        // Set the service description.
        if (!ChangeServiceConfig2W(service,
                                   SERVICE_CONFIG_DESCRIPTION,
                                   &description))
        {
            PLOG(LS_WARNING) << "ChangeServiceConfig2W failed";
        }
    }

    SC_ACTION action;
    action.Type  = SC_ACTION_RESTART;
    action.Delay = 60000; // 60 seconds

    SERVICE_FAILURE_ACTIONS actions;
    actions.dwResetPeriod = 0;
    actions.lpRebootMsg   = nullptr;
    actions.lpCommand     = nullptr;
    actions.cActions      = 1;
    actions.lpsaActions   = &action;

    if (!ChangeServiceConfig2W(service,
                               SERVICE_CONFIG_FAILURE_ACTIONS,
                               &actions))
    {
        PLOG(LS_WARNING) << "ChangeServiceConfig2W failed";
    }

    return std::unique_ptr<ServiceManager>(
        new ServiceManager(sc_manager.Release(), service.Release()));
}

// static
uint32_t ServiceManager::GetServiceStatus(const wchar_t* service_name)
{
    ScopedScHandle sc_manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));

    if (!sc_manager.IsValid())
    {
        PLOG(LS_ERROR) << "OpenSCManagerW failed";
        return 0;
    }

    ScopedScHandle service(OpenServiceW(sc_manager, service_name, SERVICE_QUERY_STATUS));
    if (!service.IsValid())
    {
        DWORD error = GetLastError();

        if (error != ERROR_SERVICE_DOES_NOT_EXIST)
        {
            LOG(LS_ERROR) << "OpenServiceW failed: " << SystemErrorCodeToString(error);
        }

        return 0;
    }

    SERVICE_STATUS_PROCESS status;
    memset(&status, 0, sizeof(status));

    DWORD bytes_needed;

    if (QueryServiceStatusEx(service,
                             SC_STATUS_PROCESS_INFO,
                             reinterpret_cast<LPBYTE>(&status),
                             sizeof(status),
                             &bytes_needed))
    {
        switch (status.dwCurrentState)
        {
            case SERVICE_RUNNING:
            case SERVICE_START_PENDING:
            case SERVICE_CONTINUE_PENDING:
                return SERVICE_STATUS_INSTALLED | SERVICE_STATUS_STARTED;

            default:
                break;
        }
    }

    return SERVICE_STATUS_INSTALLED;
}

bool ServiceManager::Start() const
{
    if (!StartServiceW(service_, 0, nullptr))
    {
        PLOG(LS_ERROR) << "StartServiceW failed";
        return false;
    }

    return true;
}

bool ServiceManager::Stop() const
{
    SERVICE_STATUS status;

    if (!ControlService(service_, SERVICE_CONTROL_STOP, &status))
    {
        DPLOG(LS_ERROR) << "ControlService failed";
        return false;
    }

    return true;
}

bool ServiceManager::Remove()
{
    if (!DeleteService(service_))
    {
        PLOG(LS_ERROR) << "DeleteService failed";
        return false;
    }

    service_.Reset();
    sc_manager_.Reset();

    return true;
}

} // namespace aspia
