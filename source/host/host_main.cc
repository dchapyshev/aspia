//
// PROJECT:         Aspia
// FILE:            host/host_main.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_main.h"

#include "base/command_line.h"
#include "base/file_logger.h"
#include "host/host_session_desktop.h"
#include "host/host_session_file_transfer.h"
#include "host/host_switches.h"
#include "version.h"

namespace aspia {

int HostMain(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    application.setOrganizationName(QStringLiteral("Aspia"));
    application.setApplicationName(QStringLiteral("Host Service"));
    application.setApplicationVersion(QStringLiteral(ASPIA_VERSION_STRING));

    FileLogger logger;
    logger.startLogging(application);

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

    return 0;
}

} // namespace aspia
