//
// PROJECT:         Aspia
// FILE:            address_book/address_book_main.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "address_book/address_book_main.h"

#include <QtCore/QCommandLineParser>

#include "base/logging.h"
#include "address_book/address_book_window.h"

namespace aspia {

int AddressBookMain(int argc, char *argv[])
{
    LoggingSettings settings;
    settings.logging_dest = LOG_TO_ALL;
    settings.lock_log = LOCK_LOG_FILE;

    InitLogging(settings);

    QApplication application(argc, argv);
    application.setOrganizationName("Aspia");
    application.setApplicationName("Address Book");
    application.setApplicationVersion("1.0.0");

    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::tr("Aspia Address Book"));
    parser.addHelpOption();
    parser.addPositionalArgument("file", QCoreApplication::tr("The file to open."));
    parser.process(application);

    QStringList arguments = parser.positionalArguments();

    QString file_path;
    if (arguments.size())
        file_path = arguments.front();

    AddressBookWindow window(file_path);
    window.show();
    window.activateWindow();

    int ret = application.exec();

    ShutdownLogging();
    return ret;
}

} // namespace aspia
