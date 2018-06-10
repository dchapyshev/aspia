//
// PROJECT:         Aspia
// FILE:            console/console_main.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "console/console_main.h"

#include <QCommandLineParser>
#include <QFileInfo>

#include "base/file_logger.h"
#include "console/console_window.h"
#include "version.h"

namespace aspia {

int consoleMain(int argc, char *argv[])
{
    FileLogger logger;
    logger.startLogging(QFileInfo(argv[0]).fileName());

    QApplication application(argc, argv);

    application.setOrganizationName(QStringLiteral("Aspia"));
    application.setApplicationName(QStringLiteral("Console"));
    application.setApplicationVersion(QStringLiteral(ASPIA_VERSION_STRING));
    application.setAttribute(Qt::AA_DisableWindowContextHelpButton, true);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Aspia Console"));
    parser.addHelpOption();
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("The file to open."));
    parser.process(application);

    QStringList arguments = parser.positionalArguments();

    QString file_path;
    if (arguments.size())
        file_path = arguments.front();

    ConsoleWindow window(file_path);
    window.show();
    window.activateWindow();

    return application.exec();
}

} // namespace aspia
