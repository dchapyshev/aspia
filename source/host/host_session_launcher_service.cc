//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_session_launcher_service.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_session_launcher_service.h"
#include "base/files/base_paths.h"
#include "base/service_manager.h"
#include "host/host_session_launcher.h"

namespace aspia {

static const WCHAR kServiceShortName[] = L"aspia-desktop-session-launcher";
static const WCHAR kServiceFullName[] = L"Aspia Desktop Session Launcher";

HostSessionLauncherService::HostSessionLauncherService(
    const std::wstring& service_id)
    : Service(ServiceManager::CreateUniqueServiceName(
        kServiceShortName, service_id))
{
    // Nothing
}

// static
bool HostSessionLauncherService::CreateStarted(uint32_t session_id,
                                               const std::wstring& channel_id)
{
    std::wstring service_id =
        ServiceManager::GenerateUniqueServiceId();

    std::wstring unique_short_name =
        ServiceManager::CreateUniqueServiceName(kServiceShortName, service_id);

    std::wstring unique_full_name =
        ServiceManager::CreateUniqueServiceName(kServiceFullName, service_id);

    FilePath path;

    if (!GetBasePath(BasePathKey::FILE_EXE, path))
        return false;

    std::wstring command_line;

    command_line.assign(path);
    command_line.append(L" --channel_id=");
    command_line.append(channel_id);
    command_line.append(L" --run_mode=");
    command_line.append(kDesktopSessionLauncherSwitch);
    command_line.append(L" --session_id=");
    command_line.append(std::to_wstring(session_id));
    command_line.append(L" --service_id=");
    command_line.append(service_id);

    // Install the service in the system.
    std::unique_ptr<ServiceManager> manager =
        ServiceManager::Create(command_line,
                               unique_full_name,
                               unique_short_name);

    // If the service was not installed.
    if (!manager)
        return false;

    return manager->Start();
}

void HostSessionLauncherService::Worker()
{
    LaunchSessionProcessFromService(kDesktopSessionSwitch,
                                    session_id_,
                                    channel_id_);
}

void HostSessionLauncherService::OnStop()
{
    // Nothing
}

void HostSessionLauncherService::RunLauncher(uint32_t session_id,
                                             const std::wstring& channel_id)
{
    session_id_ = session_id;
    channel_id_ = channel_id;

    Run();

    ServiceManager(ServiceName()).Remove();
}

} // namespace aspia
