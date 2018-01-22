//
// PROJECT:         Aspia
// FILE:            base/service_manager.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/service_manager.h"
#include "base/strings/unicode.h"
#include "base/strings/string_printf.h"
#include "base/logging.h"

#include <atomic>
#include <random>

namespace aspia {

namespace {

uint32_t GetCurrentServiceId()
{
    static std::atomic_uint32_t last_service_id = 0;
    return last_service_id++;
}

} // namespace

ServiceManager::ServiceManager(const std::wstring_view& service_short_name) :
    sc_manager_(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS))
{
    if (!sc_manager_.IsValid())
    {
        PLOG(LS_ERROR) << "OpenSCManagerW() failed";
    }
    else
    {
        service_ = OpenServiceW(sc_manager_,
                                service_short_name.data(),
                                SERVICE_ALL_ACCESS);
        if (!service_.IsValid())
        {
            PLOG(LS_ERROR) << "OpenServiceW() failed";
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
                       const std::wstring_view& service_full_name,
                       const std::wstring_view& service_short_name,
                       const std::wstring_view& service_description)
{
    // Open the service manager.
    ScopedScHandle sc_manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
    if (!sc_manager)
    {
        PLOG(LS_ERROR) << "OpenSCManagerW() failed";
        return nullptr;
    }

    // Trying to create a service.
    ScopedScHandle service(CreateServiceW(sc_manager,
                                          service_short_name.data(),
                                          service_full_name.data(),
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
        PLOG(LS_ERROR) << "CreateServiceW() failed";
        return nullptr;
    }

    if (!service_description.empty())
    {
        SERVICE_DESCRIPTIONW description;
        description.lpDescription = const_cast<LPWSTR>(service_description.data());

        // Set the service description.
        if (!ChangeServiceConfig2W(service,
                                   SERVICE_CONFIG_DESCRIPTION,
                                   &description))
        {
            PLOG(LS_WARNING) << "ChangeServiceConfig2W() failed";
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
        PLOG(LS_WARNING) << "ChangeServiceConfig2W() failed";
    }

    return std::unique_ptr<ServiceManager>(
        new ServiceManager(sc_manager.Release(), service.Release()));
}

// static
std::wstring ServiceManager::GenerateUniqueServiceId()
{
    uint32_t process_id = GetCurrentProcessId();

    std::random_device device;
    std::uniform_int_distribution<uint32_t> uniform(0, std::numeric_limits<uint32_t>::max());

    uint32_t random = uniform(device);

    return StringPrintf(L"%lu.%lu.%lu", process_id, GetCurrentServiceId(), random);
}

// static
std::wstring ServiceManager::CreateUniqueServiceName(const std::wstring_view& service_name,
                                                     const std::wstring_view& service_id)
{
    std::wstring unique_name(service_name);

    unique_name.append(L".");
    unique_name.append(service_id);

    return unique_name;
}

// static
bool ServiceManager::IsServiceInstalled(const std::wstring& service_name)
{
    ScopedScHandle sc_manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));

    if (!sc_manager.IsValid())
    {
        PLOG(LS_ERROR) << "OpenSCManagerW() failed";
        return false;
    }

    ScopedScHandle service(OpenServiceW(sc_manager,
                                        service_name.c_str(),
                                        SERVICE_QUERY_STATUS));
    if (!service.IsValid())
    {
        DWORD error = GetLastError();

        if (error != ERROR_SERVICE_DOES_NOT_EXIST)
        {
            LOG(LS_ERROR) << "OpenServiceW() failed: " << SystemErrorCodeToString(error);
        }

        return false;
    }

    return true;
}

bool ServiceManager::Start() const
{
    if (!StartServiceW(service_, 0, nullptr))
    {
        PLOG(LS_ERROR) << "StartServiceW() failed";
        return false;
    }

    return true;
}

bool ServiceManager::Stop() const
{
    SERVICE_STATUS status;

    if (!ControlService(service_, SERVICE_CONTROL_STOP, &status))
    {
        DPLOG(LS_ERROR) << "ControlService() failed";
        return false;
    }

    return true;
}

bool ServiceManager::Remove()
{
    if (!DeleteService(service_))
    {
        PLOG(LS_ERROR) << "DeleteService() failed";
        return false;
    }

    service_.Reset();
    sc_manager_.Reset();

    return true;
}

} // namespace aspia
