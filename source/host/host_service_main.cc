//
// PROJECT:         Aspia
// FILE:            host/host_service_main.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_service_main.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "host/host_service.h"

namespace aspia {

namespace {

const wchar_t kInstallSwitch[] = L"install";
const wchar_t kRemoveSwitch[] = L"remove";

} // namespace

void HostServiceMain()
{
    LoggingSettings settings;

    settings.logging_dest = LOG_TO_ALL;
    settings.lock_log = LOCK_LOG_FILE;

    InitLogging(settings);

    const CommandLine& command_line = CommandLine::ForCurrentProcess();

    if (command_line.HasSwitch(kInstallSwitch))
    {
        HostService::Install();
    }
    else if (command_line.HasSwitch(kRemoveSwitch))
    {
        HostService::Remove();
    }
    else
    {
        Process::Current().SetPriority(Process::Priority::HIGH);
        HostService().Run();
    }

    ShutdownLogging();
}

} // namespace aspia
