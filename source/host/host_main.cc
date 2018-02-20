//
// PROJECT:         Aspia
// FILE:            host/host_main.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_main.h"

#include "base/command_line.h"
#include "base/process/process.h"
#include "base/logging.h"
#include "host/host_session_desktop.h"
#include "host/host_session_file_transfer.h"
#include "host/host_session_system_info.h"
#include "host/host_switches.h"

namespace aspia {

void HostMain()
{
    LoggingSettings settings;

    settings.logging_dest = LOG_TO_ALL;
    settings.lock_log = LOCK_LOG_FILE;

    InitLogging(settings);

    Process::Current().SetPriority(Process::Priority::HIGH);

    std::wstring channel_id =
        CommandLine::ForCurrentProcess().GetSwitchValue(kChannelIdSwitch);
    std::wstring session_type =
        CommandLine::ForCurrentProcess().GetSwitchValue(kSessionTypeSwitch);

    if (session_type == kSessionTypeDesktop)
    {
        HostSessionDesktop().Run(channel_id);
    }
    else if (session_type == kSessionTypeFileTransfer)
    {
        HostSessionFileTransfer().Run(channel_id);
    }
    else if (session_type == kSessionTypeSystemInfo)
    {
        HostSessionSystemInfo().Run(channel_id);
    }

    ShutdownLogging();
}

} // namespace aspia
