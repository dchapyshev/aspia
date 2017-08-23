//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_main.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_main.h"
#include "host/host_service.h"
#include "host/sas_injector.h"
#include "host/host_session_launcher_service.h"
#include "host/host_session_launcher.h"
#include "host/host_session_desktop.h"
#include "host/host_session_file_transfer.h"
#include "host/host_session_power.h"
#include "host/host_session_system_info.h"
#include "base/strings/unicode.h"
#include "base/process/process.h"

#include <gflags/gflags.h>

namespace aspia {

DEFINE_uint32(session_id, 0xFFFFFFFF, "Session Id");
DEFINE_string(channel_id, "", "Channel Id");
DEFINE_string(service_id, "", "Service Id");
DEFINE_string(launcher_mode, "", "Launcher Mode");

void RunHostMain(const std::wstring& run_mode)
{
    Process::Current().SetPriority(Process::Priority::HIGH);

    if (run_mode == kSasServiceSwitch)
    {
        std::wstring service_id;
        CHECK(ANSItoUNICODE(FLAGS_service_id, service_id));

        SasInjector injector(service_id);
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
        std::wstring launcher_mode;
        CHECK(ANSItoUNICODE(FLAGS_launcher_mode, launcher_mode));

        std::wstring channel_id;
        CHECK(ANSItoUNICODE(FLAGS_channel_id, channel_id));

        std::wstring service_id;
        CHECK(ANSItoUNICODE(FLAGS_service_id, service_id));

        HostSessionLauncherService launcher(service_id);
        launcher.RunLauncher(launcher_mode, FLAGS_session_id, channel_id);
    }
    else
    {
        std::wstring channel_id;
        CHECK(ANSItoUNICODE(FLAGS_channel_id, channel_id));

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
