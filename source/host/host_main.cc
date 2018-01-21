//
// PROJECT:         Aspia
// FILE:            host/host_main.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/process/process.h"
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

const CommandLine::CharType kRunModeSwitch[] = L"run_mode";
const CommandLine::CharType kSessionIdSwitch[] = L"session_id";
const CommandLine::CharType kChannelIdSwitch[] = L"channel_id";
const CommandLine::CharType kServiceIdSwitch[] = L"service_id";
const CommandLine::CharType kLauncherModeSwitch[] = L"launcher_mode";

const CommandLine::CharType kRunModeSessionLauncher[] = L"session-launcher";
const CommandLine::CharType kRunModeDesktopSession[] = L"desktop-session";
const CommandLine::CharType kRunModeFileTransferSession[] = L"file-transfer-session";
const CommandLine::CharType kRunModePowerManageSession[] = L"power-manage-session";
const CommandLine::CharType kRunModeSystemInfoSession[] = L"system-info-session";
const CommandLine::CharType kRunModeSystemInfo[] = L"system-info";
const CommandLine::CharType kRunModeHostService[] = L"host-service";

const CommandLine::CharType kInstallHostServiceSwitch[] = L"install-host-service";
const CommandLine::CharType kRemoveHostServiceSwitch[] = L"remove-host-service";

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
    else if (run_mode == kRunModeHostService)
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
    else if (run_mode == kRunModeSessionLauncher)
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
    else if (run_mode == kRunModeSystemInfo)
    {
        HostLocalSystemInfo().Run();
    }
    else
    {
        DCHECK(command_line.HasSwitch(kChannelIdSwitch));

        std::wstring channel_id = command_line.GetSwitchValue(kChannelIdSwitch);

        if (run_mode == kRunModeDesktopSession)
        {
            HostSessionDesktop().Run(channel_id);
        }
        else if (run_mode == kRunModeFileTransferSession)
        {
            HostSessionFileTransfer().Run(channel_id);
        }
        else if (run_mode == kRunModePowerManageSession)
        {
            HostSessionPower().Run(channel_id);
        }
        else if (run_mode == kRunModeSystemInfoSession)
        {
            HostSessionSystemInfo().Run(channel_id);
        }
    }
}

} // namespace aspia
