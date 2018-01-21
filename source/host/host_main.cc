//
// PROJECT:         Aspia
// FILE:            host/host_main.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/process/process.h"
#include "base/command_line.h"
#include "host/host_main.h"
#include "host/host_service.h"
#include "host/sas_injector.h"
#include "host/host_session_launcher_service.h"
#include "host/host_session_launcher.h"
#include "host/host_session_desktop.h"
#include "host/host_session_file_transfer.h"
#include "host/host_session_power.h"
#include "host/host_session_system_info.h"
#include "host/host_local_system_info.h"

namespace aspia {

namespace {

constexpr CommandLine::CharType kRunModeSwitch[] = L"run_mode";
constexpr CommandLine::CharType kSessionIdSwitch[] = L"session_id";
constexpr CommandLine::CharType kChannelIdSwitch[] = L"channel_id";
constexpr CommandLine::CharType kServiceIdSwitch[] = L"service_id";
constexpr CommandLine::CharType kLauncherModeSwitch[] = L"launcher_mode";

} // namespace

void RunHostMain(const CommandLine& command_line)
{
    Process::Current().SetPriority(Process::Priority::HIGH);

    CommandLine::StringType run_mode = command_line.GetSwitchValue(kRunModeSwitch);

    if (run_mode == kSasServiceSwitch)
    {
        DCHECK(command_line.HasSwitch(kServiceIdSwitch));
        SasInjector injector(command_line.GetSwitchValue(kServiceIdSwitch));
        injector.ExecuteService();
    }
    else if (run_mode == kHostServiceSwitch)
    {
        HostService().Run();
    }
    else if (run_mode == kInstallHostServiceSwitch)
    {
        HostService::Install();
    }
    else if (run_mode == kRemoveHostServiceSwitch)
    {
        HostService::Remove();
    }
    else if (run_mode == kSessionLauncherSwitch)
    {
        DCHECK(command_line.HasSwitch(kLauncherModeSwitch) &&
               command_line.HasSwitch(kChannelIdSwitch) &&
               command_line.HasSwitch(kServiceIdSwitch) &&
               command_line.HasSwitch(kSessionIdSwitch));

        HostSessionLauncherService launcher(command_line.GetSwitchValue(kServiceIdSwitch));

        launcher.RunLauncher(command_line.GetSwitchValue(kLauncherModeSwitch),
                             command_line.GetSwitchValue(kSessionIdSwitch),
                             command_line.GetSwitchValue(kChannelIdSwitch));
    }
    else if (run_mode == kSystemInfoSwitch)
    {
        HostLocalSystemInfo().Run();
    }
    else
    {
        DCHECK(command_line.HasSwitch(kChannelIdSwitch));

        std::wstring channel_id = command_line.GetSwitchValue(kChannelIdSwitch);

        if (run_mode == kDesktopSessionSwitch)
        {
            HostSessionDesktop().Run(channel_id);
        }
        else if (run_mode == kFileTransferSessionSwitch)
        {
            HostSessionFileTransfer().Run(channel_id);
        }
        else if (run_mode == kPowerManageSessionSwitch)
        {
            HostSessionPower().Run(channel_id);
        }
        else if (run_mode == kSystemInfoSessionSwitch)
        {
            HostSessionSystemInfo().Run(channel_id);
        }
    }
}

} // namespace aspia
