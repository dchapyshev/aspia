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

#include "client/client_main.h"

#include "base/logging.h"
#include "build/version.h"
#include "client/config_factory.h"
#include "client/ui/client_dialog.h"
#include "client/ui/qt_desktop_window.h"
#include "client/ui/qt_file_manager_window.h"
#include "qt_base/application.h"

#include <QCommandLineParser>
#include <QMessageBox>

int clientMain(int argc, char* argv[])
{
    Q_INIT_RESOURCE(qt_translations);
    Q_INIT_RESOURCE(client);
    Q_INIT_RESOURCE(client_translations);
    Q_INIT_RESOURCE(common);
    Q_INIT_RESOURCE(common_translations);

    qt_base::Application::setOrganizationName("Aspia");
    qt_base::Application::setApplicationName("Host");
    qt_base::Application::setApplicationVersion(ASPIA_VERSION_STRING);
    qt_base::Application::setAttribute(Qt::AA_DisableWindowContextHelpButton, true);
    qt_base::Application::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    qt_base::Application::setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    qt_base::Application application(argc, argv);

    QCommandLineOption address_option(
        "address",
        QApplication::translate("Client", "Remote computer address."),
        QApplication::translate("Client", "address"));

    QCommandLineOption port_option(
        "port",
        QApplication::translate("Client", "Remote computer port."),
        QApplication::translate("Client", "port"),
        QString::number(DEFAULT_HOST_TCP_PORT));

    QCommandLineOption username_option(
        "username",
        QApplication::translate("Client", "Name of user."),
        QApplication::translate("Client", "username"));

    QCommandLineOption session_type_option(
        "session-type",
        QApplication::translate("Client", "Session type. Possible values: desktop-manage, "
                                "desktop-view, file-transfer."),
        "desktop-manage");

    QCommandLineParser parser;
    parser.setApplicationDescription(QApplication::translate("Client", "Aspia Client"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(address_option);
    parser.addOption(port_option);
    parser.addOption(username_option);
    parser.addOption(session_type_option);
    parser.process(application);

    QScopedPointer<client::ClientDialog> client_dialog;

    if (parser.isSet(address_option))
    {
        client::Config config;
        config.address_or_id = parser.value(address_option).toStdU16String();
        config.port = parser.value(port_option).toUShort();
        config.username = parser.value(username_option).toStdU16String();

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

        client::SessionWindow* session_window = nullptr;

        switch (config.session_type)
        {
            case proto::SESSION_TYPE_DESKTOP_MANAGE:
            {
                session_window = new client::QtDesktopWindow(
                    config.session_type, client::ConfigFactory::defaultDesktopManageConfig());
            }
            break;

            case proto::SESSION_TYPE_DESKTOP_VIEW:
            {
                session_window = new client::QtDesktopWindow(
                    config.session_type, client::ConfigFactory::defaultDesktopViewConfig());
            }
            break;

            case proto::SESSION_TYPE_FILE_TRANSFER:
                session_window = new client::QtFileManagerWindow();
                break;

            default:
                NOTREACHED();
                break;
        }

        if (!session_window)
            return 0;

        session_window->setAttribute(Qt::WA_DeleteOnClose);
        if (!session_window->connectToHost(config))
            return 0;
    }
    else
    {
        client_dialog.reset(new client::ClientDialog());
        client_dialog->show();
        client_dialog->activateWindow();
    }

    return application.exec();
}
