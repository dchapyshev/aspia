//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_service.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_service.h"
#include "base/service_manager.h"
#include "base/security_helpers.h"
#include "base/scoped_com_initializer.h"
#include "base/strings/unicode.h"
#include "base/path.h"
#include "network/scoped_wsa_initializer.h"

#include <sddl.h>

namespace aspia {

static const WCHAR kHostServiceShortName[] = L"aspia-remote-desktop-host";
static const WCHAR kHostServiceFullName[] = L"Aspia Remote Desktop Host";

//
// Security descriptor allowing local processes running under SYSTEM or
// LocalService accounts to call COM methods exposed by the daemon.
//
const WCHAR kComProcessSd[] =
    SDDL_OWNER L":" SDDL_LOCAL_SYSTEM
    SDDL_GROUP L":" SDDL_LOCAL_SYSTEM
    SDDL_DACL L":"
    SDDL_ACE(SDDL_ACCESS_ALLOWED, SDDL_COM_EXECUTE_LOCAL, SDDL_LOCAL_SYSTEM)
    SDDL_ACE(SDDL_ACCESS_ALLOWED, SDDL_COM_EXECUTE_LOCAL, SDDL_LOCAL_SERVICE);

//
// Appended to |kComProcessSd| to specify that only callers running at medium or
// higher integrity level are allowed to call COM methods exposed by the daemon.
//
const WCHAR kComProcessMandatoryLabel[] =
    SDDL_SACL L":"
    SDDL_ACE(SDDL_MANDATORY_LABEL, SDDL_NO_EXECUTE_UP, SDDL_ML_MEDIUM);

HostService::HostService() :
    Service(kHostServiceShortName)
{
    // Nothing
}

void HostService::Worker()
{
    ScopedWsaInitializer wsa_initializer;
    CHECK(wsa_initializer.IsSucceeded());

    ScopedCOMInitializer com_initializer;
    CHECK(com_initializer.IsSucceeded());

    InitializeComSecurity(kComProcessSd, kComProcessMandatoryLabel, false);

    MessageLoop loop;
    runner_ = MessageLoopProxy::Current();

    HostPool host_pool(runner_);

    if (host_pool.Start())
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
    std::wstring command_line;

    if (!GetPathW(PathKey::FILE_EXE, command_line))
        return false;

    command_line.append(L" --run_mode=");
    command_line.append(kHostServiceSwitch);

    ServiceManager manager(ServiceManager::Create(command_line,
                                                  kHostServiceFullName,
                                                  kHostServiceShortName,
                                                  kHostServiceFullName));
    if (manager.IsValid())
        return manager.Start();

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
