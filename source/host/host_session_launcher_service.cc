//
// PROJECT:         Aspia
// FILE:            host/host_session_launcher_service.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_session_launcher_service.h"
#include "base/files/base_paths.h"
#include "base/command_line.h"
#include "base/service_manager.h"
#include "base/logging.h"
#include "host/host_session_launcher.h"

namespace aspia {

namespace {

constexpr WCHAR kServiceShortName[] = L"aspia-session-launcher";
constexpr WCHAR kServiceFullName[] = L"Aspia Session Launcher";

const wchar_t kSessionTypeSwitch[] = L"session-type";
const wchar_t kChannelIdSwitch[] = L"channel-id";
const wchar_t kServiceIdSwitch[] = L"service-id";
const wchar_t kSessionIdSwitch[] = L"session-id";

const wchar_t kSessionTypeDesktop[] = L"desktop";
const wchar_t kSessionTypeFileTransfer[] = L"file-transfer";
const wchar_t kSessionTypeSystemInfo[] = L"system-info";

} // namespace

HostSessionLauncherService::HostSessionLauncherService()
    : Service(ServiceManager::CreateUniqueServiceName(
        kServiceShortName,
        CommandLine::ForCurrentProcess().GetSwitchValue(kServiceIdSwitch)))
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

    std::experimental::filesystem::path program_path;

    if (!GetBasePath(BasePathKey::FILE_EXE, program_path))
        return false;

    CommandLine command_line(program_path);

    command_line.AppendSwitch(kChannelIdSwitch, channel_id);
    //command_line.AppendSwitch(kModeSwitch, kModeSessionLauncher);
    command_line.AppendSwitch(kSessionTypeSwitch, launcher_mode);
    command_line.AppendSwitch(kSessionIdSwitch, std::to_wstring(session_id));
    command_line.AppendSwitch(kServiceIdSwitch, service_id);

    // Install the service in the system.
    std::unique_ptr<ServiceManager> manager =
        ServiceManager::Create(command_line,
                               unique_full_name,
                               unique_short_name,
                               kServiceFullName);

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
    const CommandLine& command_line = CommandLine::ForCurrentProcess();

    LaunchSessionProcessFromService(
        command_line.GetSwitchValue(kSessionTypeSwitch),
        std::stoul(command_line.GetSwitchValue(kSessionIdSwitch)),
        command_line.GetSwitchValue(kChannelIdSwitch));
}

void HostSessionLauncherService::OnStop()
{
    // Nothing
}

void HostSessionLauncherService::RunLauncher()
{
    Run();

    ServiceManager manager(ServiceName());
    manager.Stop();
    manager.Remove();
}

} // namespace aspia
