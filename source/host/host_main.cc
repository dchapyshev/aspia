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
#include "host/host_session_system_info.h"
#include "host/host_local_system_info.h"
#include "command_line_switches.h"

namespace aspia {

void RunHostMain(const CommandLine& command_line)
{
    Process::Current().SetPriority(Process::Priority::HIGH);

    CommandLine::StringType run_mode = command_line.GetSwitchValue(kModeSwitch);

    if (run_mode == kSasServiceSwitch)
    {
        DCHECK(command_line.HasSwitch(kServiceIdSwitch));
        SasInjector injector(command_line.GetSwitchValue(kServiceIdSwitch));
        injector.ExecuteService();
    }
    else if (run_mode == kModeHostService)
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
    else if (run_mode == kModeSessionLauncher)
    {
        DCHECK(command_line.HasSwitch(kLauncherModeSwitch) &&
               command_line.HasSwitch(kChannelIdSwitch) &&
               command_line.HasSwitch(kServiceIdSwitch) &&
               command_line.HasSwitch(kSessionIdSwitch));

        HostSessionLauncherService launcher(command_line.GetSwitchValue(kServiceIdSwitch));

        launcher.RunLauncher();
    }
    else if (run_mode == kModeSystemInfo)
    {
        HostLocalSystemInfo().Run();
    }
    else
    {
        DCHECK(command_line.HasSwitch(kChannelIdSwitch));

        std::wstring channel_id = command_line.GetSwitchValue(kChannelIdSwitch);

        if (run_mode == kModeDesktopSession)
        {
            HostSessionDesktop().Run(channel_id);
        }
        else if (run_mode == kModeFileTransferSession)
        {
            HostSessionFileTransfer().Run(channel_id);
        }
        else if (run_mode == kModeSystemInfoSession)
        {
            HostSessionSystemInfo().Run(channel_id);
        }
    }
}

} // namespace aspia
