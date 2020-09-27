//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/files/base_paths.h"
#include "client/config_factory.h"
#include "client/ui/client_dialog.h"
#include "client/ui/qt_desktop_window.h"
#include "client/ui/qt_file_manager_window.h"
#include "console/application.h"
#include "console/main_window.h"
#include "qt_base/qt_logging.h"

#include <QCommandLineParser>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(qt_translations);
    Q_INIT_RESOURCE(client);
    Q_INIT_RESOURCE(client_translations);
    Q_INIT_RESOURCE(common);
    Q_INIT_RESOURCE(common_translations);

    console::Application::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    console::Application::setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    console::Application application(argc, argv);

    QCommandLineOption address_option(
        "address",
        QApplication::translate("Console", "Remote computer address."),
        QApplication::translate("Console", "address"));

    QCommandLineOption port_option(
        "port",
        QApplication::translate("Console", "Remote computer port."),
        QApplication::translate("Console", "port"),
        QString::number(DEFAULT_HOST_TCP_PORT));

    QCommandLineOption username_option(
        "username",
        QApplication::translate("Console", "Name of user."),
        QApplication::translate("Console", "username"));

    QCommandLineOption session_type_option(
        "session-type",
        QApplication::translate("Console", "Session type. Possible values: desktop-manage, "
            "desktop-view, file-transfer."),
        "desktop-manage");

    QCommandLineOption client_option(
        "client",
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
        client::ClientDialog().exec();
    }
    else if (parser.isSet(address_option))
    {
        client::Config config;
        config.address_or_id = parser.value(address_option).toStdU16String();
        config.port          = parser.value(port_option).toUShort();
        config.username      = parser.value(username_option).toStdU16String();

        QString session_type = parser.value(session_type_option);

        if (session_type == QLatin1String("desktop-manage"))
        {
            config.session_type = proto::SESSION_TYPE_DESKTOP_MANAGE;
        }
        else if (session_type == QLatin1String("desktop-view"))
        {
            config.session_type = proto::SESSION_TYPE_DESKTOP_VIEW;
        }
        else if (session_type == QLatin1String("file-transfer"))
        {
            config.session_type = proto::SESSION_TYPE_FILE_TRANSFER;
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

        client::ClientWindow* client_window = nullptr;

        switch (config.session_type)
        {
            case proto::SESSION_TYPE_DESKTOP_MANAGE:
            {
                client_window = new client::QtDesktopWindow(
                    config.session_type, client::ConfigFactory::defaultDesktopManageConfig());
            }
            break;

            case proto::SESSION_TYPE_DESKTOP_VIEW:
            {
                client_window = new client::QtDesktopWindow(
                    config.session_type, client::ConfigFactory::defaultDesktopViewConfig());
            }
            break;

            case proto::SESSION_TYPE_FILE_TRANSFER:
                client_window = new client::QtFileManagerWindow();
                break;

            default:
                NOTREACHED();
                break;
        }

        if (!client_window)
            return 0;

        client_window->setAttribute(Qt::WA_DeleteOnClose);
        if (!client_window->connectToHost(config))
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
            console_window.reset(new console::MainWindow(file_path));

            QObject::connect(&application, &console::Application::windowActivated,
                console_window.get(), &console::MainWindow::showConsole);

            QObject::connect(&application, &console::Application::fileOpened,
                console_window.get(), &console::MainWindow::openAddressBook);

            console_window->show();
            console_window->activateWindow();
        }
    }

    return application.exec();
}
