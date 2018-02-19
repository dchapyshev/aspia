//
// PROJECT:         Aspia
// FILE:            host/host_main.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_main.h"

#include "base/command_line.h"
#include "base/process/process.h"
//#include "host/sas_injector.h"
//#include "host/host_session_launcher.h"
#include "host/host_session_desktop.h"
#include "host/host_session_file_transfer.h"
#include "host/host_session_system_info.h"
//#include "host/host_local_system_info.h"

#include "base/logging.h"

namespace aspia {

namespace {

const wchar_t kSessionTypeSwitch[] = L"session-type";
const wchar_t kChannelIdSwitch[] = L"channel-id";

const wchar_t kSessionTypeDesktop[] = L"desktop";
const wchar_t kSessionTypeFileTransfer[] = L"file-transfer";
const wchar_t kSessionTypeSystemInfo[] = L"system-info";

} // namespace

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
