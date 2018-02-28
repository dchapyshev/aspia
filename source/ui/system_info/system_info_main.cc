//
// PROJECT:         Aspia
// FILE:            ui/system_info/system_info_main.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/system_info/system_info_main.h"

#include "base/logging.h"
#include "host/host_local_system_info.h"

namespace aspia {

void SystemInfoMain()
{
    LoggingSettings settings;

    settings.logging_dest = LOG_TO_ALL;
    settings.lock_log = LOCK_LOG_FILE;

    InitLogging(settings);

    HostLocalSystemInfo().Run();

    ShutdownLogging();
}

} // namespace aspia
