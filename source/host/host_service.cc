//
// PROJECT:         Aspia
// FILE:            host/host_service.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_service.h"

#include <QDebug>
#include <sddl.h>

#include "base/win/service_manager.h"
#include "base/win/security_helpers.h"
#include "base/win/scoped_com_initializer.h"
#include "base/system_error_code.h"

namespace aspia {

namespace {

static const wchar_t kHostServiceShortName[] = L"aspia-host-service";
static const wchar_t kHostServiceFullName[] = L"Aspia Host Service";

// Security descriptor allowing local processes running under SYSTEM or
// LocalService accounts to call COM methods exposed by the daemon.
const wchar_t kComProcessSd[] =
    SDDL_OWNER L":" SDDL_LOCAL_SYSTEM
    SDDL_GROUP L":" SDDL_LOCAL_SYSTEM
    SDDL_DACL L":"
    SDDL_ACE(SDDL_ACCESS_ALLOWED, SDDL_COM_EXECUTE_LOCAL, SDDL_LOCAL_SYSTEM)
    SDDL_ACE(SDDL_ACCESS_ALLOWED, SDDL_COM_EXECUTE_LOCAL, SDDL_LOCAL_SERVICE);

// Appended to |kComProcessSd| to specify that only callers running at medium
// or higher integrity level are allowed to call COM methods exposed by the
// daemon.
const wchar_t kComProcessMandatoryLabel[] =
    SDDL_SACL L":"
    SDDL_ACE(SDDL_MANDATORY_LABEL, SDDL_NO_EXECUTE_UP, SDDL_ML_MEDIUM);

} // namespace

HostService::HostService()
    : Service(kHostServiceShortName)
{
    // Nothing
}

void HostService::Worker()
{
    ScopedCOMInitializer com_initializer;
    if (!com_initializer.IsSucceeded())
        qFatal("COM not initialized");

    InitializeComSecurity(kComProcessSd, kComProcessMandatoryLabel, false);

    MessageLoop loop;
    runner_ = MessageLoopProxy::Current();

    HostPool host_pool(runner_);
    loop.Run();
}

void HostService::OnStop()
{
    if (runner_)
        runner_->PostQuit();
}

// static
uint32_t HostService::GetStatus()
{
    uint32_t status = ServiceManager::GetServiceStatus(kHostServiceShortName);

    if (status & ServiceManager::SERVICE_STATUS_INSTALLED)
    {
        if (status & ServiceManager::SERVICE_STATUS_STARTED)
            return STATUS_INSTALLED | STATUS_STARTED;

        return STATUS_INSTALLED;
    }

    return 0;
}

// static
bool HostService::Install()
{
    wchar_t program_path[MAX_PATH];

    if (!GetModuleFileNameW(nullptr, program_path, _countof(program_path)))
    {
        qWarning() << "GetModuleFileNameW failed: " << lastSystemErrorString();
        return false;
    }

    CommandLine command_line(program_path);

    std::unique_ptr<ServiceManager> manager =
        ServiceManager::Create(command_line,
                               kHostServiceFullName,
                               kHostServiceShortName,
                               kHostServiceFullName);
    if (manager)
        return manager->Start();

    return false;
}

// static
bool HostService::Remove()
{
    ServiceManager manager(kHostServiceShortName);
    manager.Stop();
    return manager.Remove();
}

// static
bool HostService::Start()
{
    return ServiceManager(kHostServiceShortName).Start();
}

// static
bool HostService::Stop()
{
    return ServiceManager(kHostServiceShortName).Stop();
}

} // namespace aspia
