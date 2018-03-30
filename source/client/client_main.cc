//
// PROJECT:         Aspia
// FILE:            client/client_main.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_main.h"

#include "base/logging.h"
#include "client/ui/client_dialog.h"
#include "version.h"

namespace aspia {

int ClientMain(int argc, char *argv[])
{
    LoggingSettings settings;
    settings.logging_dest = LOG_TO_ALL;
    settings.lock_log = LOCK_LOG_FILE;

    InitLogging(settings);

    QApplication application(argc, argv);
    application.setOrganizationName(QStringLiteral("Aspia"));
    application.setApplicationName(QStringLiteral("Client"));
    application.setApplicationVersion(QStringLiteral(ASPIA_VERSION_STRING));

    ClientDialog dialog;
    dialog.show();
    dialog.activateWindow();

    int ret = application.exec();

    ShutdownLogging();
    return ret;
}

} // namespace aspia
