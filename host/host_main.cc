//
// PROJECT:         Aspia Remote Desktop
// FILE:            host/host_main.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_main.h"
#include "host/desktop_session_launcher.h"
#include "host/host_service.h"
#include "host/sas_injector.h"
#include "host/desktop_session_client.h"
#include "base/unicode.h"

#include <gflags/gflags.h>

namespace aspia {

DEFINE_uint32(session_id, 0xFFFFFFFF, "Session Id");
DEFINE_string(input_channel_id, "", "Input channel Id");
DEFINE_string(output_channel_id, "", "Output channel Id");
DEFINE_string(service_id, "", "Service Id");

void RunHostMain(const std::wstring& run_mode)
{
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
    else if (run_mode == kDesktopSessionLauncherSwitch)
    {
        std::wstring input_channel_id;
        CHECK(ANSItoUNICODE(FLAGS_input_channel_id, input_channel_id));

        std::wstring output_channel_id;
        CHECK(ANSItoUNICODE(FLAGS_output_channel_id, output_channel_id));

        std::wstring service_id;
        CHECK(ANSItoUNICODE(FLAGS_service_id, service_id));

        DesktopSessionLauncher launcher(service_id);

        launcher.ExecuteService(FLAGS_session_id, input_channel_id, output_channel_id);
    }
    else if (run_mode == kDesktopSessionSwitch)
    {
        std::wstring input_channel_id;
        CHECK(ANSItoUNICODE(FLAGS_input_channel_id, input_channel_id));

        std::wstring output_channel_id;
        CHECK(ANSItoUNICODE(FLAGS_output_channel_id, output_channel_id));

        DesktopSessionClient().Run(input_channel_id, output_channel_id);
    }
}

} // namespace aspia
