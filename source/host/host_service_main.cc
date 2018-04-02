//
// PROJECT:         Aspia
// FILE:            host/host_service_main.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_service_main.h"

#include "base/command_line.h"
#include "base/file_logger.h"
#include "host/host_service.h"
#include "version.h"

namespace aspia {

namespace {

const wchar_t kInstallSwitch[] = L"install";
const wchar_t kRemoveSwitch[] = L"remove";

} // namespace

int HostServiceMain(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    application.setOrganizationName(QStringLiteral("Aspia"));
    application.setApplicationName(QStringLiteral("Host Service"));
    application.setApplicationVersion(QStringLiteral(ASPIA_VERSION_STRING));

    FileLogger logger;
    logger.startLogging(application);

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

    return 0;
}

} // namespace aspia
