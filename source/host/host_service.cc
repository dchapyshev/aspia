//
// PROJECT:         Aspia
// FILE:            host/host_service.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_service.h"
#include "base/service_manager.h"
#include "base/security_helpers.h"
#include "base/scoped_com_initializer.h"
#include "base/files/base_paths.h"
#include "command_line_switches.h"

#include <sddl.h>

namespace aspia {

namespace {

static const WCHAR kHostServiceShortName[] = L"aspia-host";
static const WCHAR kHostServiceFullName[] = L"Aspia Host";

// Security descriptor allowing local processes running under SYSTEM or
// LocalService accounts to call COM methods exposed by the daemon.
const WCHAR kComProcessSd[] =
    SDDL_OWNER L":" SDDL_LOCAL_SYSTEM
    SDDL_GROUP L":" SDDL_LOCAL_SYSTEM
    SDDL_DACL L":"
    SDDL_ACE(SDDL_ACCESS_ALLOWED, SDDL_COM_EXECUTE_LOCAL, SDDL_LOCAL_SYSTEM)
    SDDL_ACE(SDDL_ACCESS_ALLOWED, SDDL_COM_EXECUTE_LOCAL, SDDL_LOCAL_SERVICE);

// Appended to |kComProcessSd| to specify that only callers running at medium
// or higher integrity level are allowed to call COM methods exposed by the
// daemon.
const WCHAR kComProcessMandatoryLabel[] =
    SDDL_SACL L":"
    SDDL_ACE(SDDL_MANDATORY_LABEL, SDDL_NO_EXECUTE_UP, SDDL_ML_MEDIUM);

} // namespace

HostService::HostService() :
    Service(kHostServiceShortName)
{
    // Nothing
}

void HostService::Worker()
{
    ScopedCOMInitializer com_initializer;
    CHECK(com_initializer.IsSucceeded());

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
bool HostService::IsInstalled()
{
    return ServiceManager::IsServiceInstalled(kHostServiceShortName);
}

// static
bool HostService::Install()
{
    std::experimental::filesystem::path program_path;

    if (!GetBasePath(BasePathKey::FILE_EXE, program_path))
        return false;

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

} // namespace aspia
