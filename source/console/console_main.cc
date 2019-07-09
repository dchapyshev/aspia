//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "base/base_paths.h"
#include "base/qt_logging.h"
#include "build/version.h"
#include "client/ui/client_window.h"
#include "console/console_main_window.h"
#include "console/console_single_application.h"
#include "crypto/scoped_crypto_initializer.h"

#if defined(USE_TBB)
#include <tbb/tbbmalloc_proxy.h>
#endif // defined(USE_TBB)

#include <QCommandLineParser>
#include <QMessageBox>

#if defined(QT_STATIC)

#include <QtPlugin>

#if defined(Q_OS_WIN)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
Q_IMPORT_PLUGIN(QWindowsVistaStylePlugin);
Q_IMPORT_PLUGIN(QWindowsPrinterSupportPlugin);
#else
#error Platform support needed
#endif // defined(Q_OS_WIN)
#endif // defined(QT_STATIC)

namespace {

std::filesystem::path loggingDir()
{
    std::filesystem::path path;

    if (!base::BasePaths::userAppData(&path))
        return std::filesystem::path();

    path.append("aspia/logs");
    return path;
}

void tbbStatusToLog()
{
#if defined(USE_TBB)
    char** func_replacement_log;
    int func_replacement_status = TBB_malloc_replacement_log(&func_replacement_log);

    if (func_replacement_status != 0)
    {
        LOG(LS_WARNING) << "tbbmalloc_proxy cannot replace memory allocation routines";

        for (char** log_string = func_replacement_log; *log_string != 0; ++log_string)
        {
            LOG(LS_WARNING) << *log_string;
        }
    }
    else
    {
        LOG(LS_INFO) << "tbbmalloc_proxy successfully initialized";
    }
#else
    DLOG(LS_INFO) << "tbbmalloc_proxy is disabled";
#endif
}

int runApplication(int argc, char *argv[])
{
    console::SingleApplication application(argc, argv);

    console::Settings console_settings;
    QString current_locale = console_settings.locale();

    common::LocaleLoader locale_loader;
    if (!locale_loader.contains(current_locale))
        console_settings.setLocale(QStringLiteral(DEFAULT_LOCALE));

    locale_loader.installTranslators(current_locale);

    application.setOrganizationName(QStringLiteral("Aspia"));
    application.setApplicationName(QStringLiteral("Console"));
    application.setApplicationVersion(QStringLiteral(ASPIA_VERSION_STRING));
    application.setAttribute(Qt::AA_DisableWindowContextHelpButton, true);

    QCommandLineOption address_option(
        QStringLiteral("address"),
        QApplication::translate("Console", "Remote computer address."),
        QApplication::translate("Console", "address"));

    QCommandLineOption port_option(
        QStringLiteral("port"),
        QApplication::translate("Console", "Remote computer port."),
        QApplication::translate("Console", "port"),
        QString::number(DEFAULT_HOST_TCP_PORT));

    QCommandLineOption username_option(
        QStringLiteral("username"),
        QApplication::translate("Console", "Name of user."),
        QApplication::translate("Console", "username"));

    QCommandLineOption session_type_option(
        QStringLiteral("session-type"),
        QApplication::translate("Console", "Session type. Possible values: desktop-manage, "
                                "desktop-view, file-transfer."),
        QStringLiteral("desktop-manage"));

    QCommandLineOption client_option(
        QStringLiteral("client"),
        QApplication::translate("Console", "Open the client to connect to the computer."));

    QCommandLineParser parser;
    parser.setApplicationDescription(QApplication::translate("Console", "Aspia Console"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QApplication::translate("Console", "file"),
                                 QApplication::translate("Console", "The file to open."));
    parser.addOption(address_option);
    parser.addOption(port_option);
    parser.addOption(username_option);
    parser.addOption(session_type_option);
    parser.addOption(client_option);
    parser.process(application);

    QScopedPointer<console::MainWindow> console_window;

    if (parser.isSet(client_option))
    {
        if (!client::ClientWindow::connectToHost())
            return 0;
    }
    else if (parser.isSet(address_option))
    {
        client::ConnectData connect_data;
        connect_data.address = parser.value(address_option);
        connect_data.port = parser.value(port_option).toUShort();
        connect_data.username = parser.value(username_option);

        QString session_type = parser.value(session_type_option);

        if (session_type == QLatin1String("desktop-manage"))
        {
            connect_data.session_type = proto::SESSION_TYPE_DESKTOP_MANAGE;
        }
        else if (session_type == QLatin1String("desktop-view"))
        {
            connect_data.session_type = proto::SESSION_TYPE_DESKTOP_VIEW;
        }
        else if (session_type == QLatin1String("file-transfer"))
        {
            connect_data.session_type = proto::SESSION_TYPE_FILE_TRANSFER;
        }
        else
        {
            QMessageBox::warning(
                nullptr,
                QApplication::translate("Console", "Warning"),
                QApplication::translate("Console", "Incorrect session type entered."),
                QMessageBox::Ok);
            return 1;
        }

        if (!client::ClientWindow::connectToHost(&connect_data))
            return 0;
    }
    else
    {
        QStringList arguments = parser.positionalArguments();

        QString file_path;
        if (!arguments.isEmpty())
            file_path = arguments.front();

        if (application.isRunning())
        {
            if (file_path.isEmpty())
                application.activateWindow();
            else
                application.openFile(file_path);

            return 0;
        }
        else
        {
            console_window.reset(
                new console::MainWindow(console_settings, locale_loader, file_path));

            QObject::connect(&application, &console::SingleApplication::windowActivated,
                             console_window.get(), &console::MainWindow::showConsole);

            QObject::connect(&application, &console::SingleApplication::fileOpened,
                             console_window.get(), &console::MainWindow::openAddressBook);

            console_window->show();
            console_window->activateWindow();
        }
    }

    return application.exec();
}

} // namespace

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(qt_translations);
    Q_INIT_RESOURCE(client);
    Q_INIT_RESOURCE(client_translations);
    Q_INIT_RESOURCE(common);
    Q_INIT_RESOURCE(common_translations);
    Q_INIT_RESOURCE(updater);
    Q_INIT_RESOURCE(updater_translations);

    base::LoggingSettings settings;
    settings.destination = base::LOG_TO_FILE;
    settings.log_dir = loggingDir();

    base::initLogging(settings);
    base::initQtLogging();

    crypto::ScopedCryptoInitializer crypto_initializer;
    CHECK(crypto_initializer.isSucceeded());

    tbbStatusToLog();

    int result = runApplication(argc, argv);

    base::shutdownLogging();
    return result;
}
