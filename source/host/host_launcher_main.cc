//
// PROJECT:         Aspia
// FILE:            host/host_launcher_main.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_launcher_main.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "host/host_session_launcher_service.h"

namespace aspia {

void HostLauncherMain()
{
    LoggingSettings settings;

    settings.logging_dest = LOG_TO_ALL;
    settings.lock_log = LOCK_LOG_FILE;

    InitLogging(settings);

    HostSessionLauncherService().RunLauncher();

    ShutdownLogging();
}

} // namespace aspia
