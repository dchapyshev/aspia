//
// PROJECT:         Aspia
// FILE:            host/host_session_launcher_service.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_session_launcher_service.h"
#include "base/files/base_paths.h"
#include "base/service_manager.h"
#include "base/logging.h"
#include "host/host_session_launcher.h"

namespace aspia {

static const WCHAR kServiceShortName[] = L"aspia-session-launcher";
static const WCHAR kServiceFullName[] = L"Aspia Session Launcher";

HostSessionLauncherService::HostSessionLauncherService(const std::wstring& service_id)
    : Service(ServiceManager::CreateUniqueServiceName(kServiceShortName, service_id))
{
    // Nothing
}

// static
bool HostSessionLauncherService::CreateStarted(const std::wstring& launcher_mode,
                                               uint32_t session_id,
                                               const std::wstring& channel_id)
{
    const std::wstring service_id = ServiceManager::GenerateUniqueServiceId();

    const std::wstring unique_short_name =
        ServiceManager::CreateUniqueServiceName(kServiceShortName, service_id);

    const std::wstring unique_full_name =
        ServiceManager::CreateUniqueServiceName(kServiceFullName, service_id);

    FilePath path;

    if (!GetBasePath(BasePathKey::FILE_EXE, path))
        return false;

    std::wstring command_line(path);

    command_line.append(L" --channel_id=");
    command_line.append(channel_id);
    command_line.append(L" --run_mode=");
    command_line.append(kSessionLauncherSwitch);
    command_line.append(L" --launcher_mode=");
    command_line.append(launcher_mode);
    command_line.append(L" --session_id=");
    command_line.append(std::to_wstring(session_id));
    command_line.append(L" --service_id=");
    command_line.append(service_id);

    // Install the service in the system.
    std::unique_ptr<ServiceManager> manager =
        ServiceManager::Create(command_line, unique_full_name, unique_short_name);

    // If the service was not installed.
    if (!manager)
        return false;

    if (!manager->Start())
    {
        manager->Remove();
        return false;
    }

    return true;
}

void HostSessionLauncherService::Worker()
{
    LaunchSessionProcessFromService(launcher_mode_, session_id_, channel_id_);
}

void HostSessionLauncherService::OnStop()
{
    // Nothing
}

void HostSessionLauncherService::RunLauncher(const std::wstring& launcher_mode,
                                             uint32_t session_id,
                                             const std::wstring& channel_id)
{
    CHECK(launcher_mode == kDesktopSessionSwitch || launcher_mode == kSystemInfoSessionSwitch);

    launcher_mode_ = launcher_mode;
    session_id_ = session_id;
    channel_id_ = channel_id;

    Run();

    ServiceManager manager(ServiceName());
    manager.Stop();
    manager.Remove();
}

} // namespace aspia
