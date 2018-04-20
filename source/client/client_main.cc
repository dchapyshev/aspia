//
// PROJECT:         Aspia
// FILE:            client/client_main.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/client_main.h"

#include "base/file_logger.h"
#include "client/ui/client_dialog.h"
#include "version.h"

namespace aspia {

int ClientMain(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setOrganizationName(QStringLiteral("Aspia"));
    application.setApplicationName(QStringLiteral("Client"));
    application.setApplicationVersion(QStringLiteral(ASPIA_VERSION_STRING));

    FileLogger logger;
    logger.startLogging(application);

    ClientDialog dialog;
    dialog.show();
    dialog.activateWindow();

    return application.exec();
}

} // namespace aspia
