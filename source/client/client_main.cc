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

#include "build/version.h"
#include "client/config_factory.h"
#include "client/router_config_storage.h"
#include "client/ui/application.h"
#include "client/ui/client_settings.h"
#include "client/ui/client_window.h"
#include "client/ui/qt_desktop_window.h"
#include "client/ui/qt_file_manager_window.h"
#include "qt_base/scoped_qt_logging.h"

#if defined(OS_WIN)
#include "base/win/mini_dump_writer.h"
#endif

#include <QCommandLineParser>
#include <QMessageBox>

int clientMain(int argc, char* argv[])
{
    Q_INIT_RESOURCE(qt_translations);
    Q_INIT_RESOURCE(client);
    Q_INIT_RESOURCE(client_translations);
    Q_INIT_RESOURCE(common);
    Q_INIT_RESOURCE(common_translations);

#if defined(OS_WIN)
    base::installFailureHandler(L"aspia_client");
#endif

    qt_base::ScopedQtLogging scoped_logging;

    client::Application::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    client::Application::setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    client::Application application(argc, argv);

    LOG(LS_INFO) << "Version: " << ASPIA_VERSION_STRING;
#if defined(GIT_CURRENT_BRANCH) && defined(GIT_COMMIT_HASH)
    LOG(LS_INFO) << "Git branch: " << GIT_CURRENT_BRANCH;
    LOG(LS_INFO) << "Git commit: " << GIT_COMMIT_HASH;
#endif
    LOG(LS_INFO) << "Command line: " << application.arguments();

    QCommandLineOption address_option("address",
        QApplication::translate("Client", "Remote computer address."), "address");

    QCommandLineOption port_option("port",
        QApplication::translate("Client", "Remote computer port."), "port",
        QString::number(DEFAULT_HOST_TCP_PORT));

    QCommandLineOption username_option("username",
        QApplication::translate("Client", "Name of user."), "username");

    QCommandLineOption session_type_option("session-type",
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

    QScopedPointer<client::ClientWindow> client_window;

    if (parser.isSet(address_option))
    {
        client::RouterConfig router_config = client::RouterConfigStorage().routerConfig();
        client::Config config;

        config.address_or_id = parser.value(address_option).toStdU16String();
        config.port = parser.value(port_option).toUShort();
        config.username = parser.value(username_option).toStdU16String();

        QString session_type = parser.value(session_type_option);

        if (session_type == "desktop-manage")
        {
            config.session_type = proto::SESSION_TYPE_DESKTOP_MANAGE;
        }
        else if (session_type == "desktop-view")
        {
            config.session_type = proto::SESSION_TYPE_DESKTOP_VIEW;
        }
        else if (session_type == "file-transfer")
        {
            config.session_type = proto::SESSION_TYPE_FILE_TRANSFER;
        }
        else
        {
            QMessageBox::warning(
                nullptr,
                QApplication::translate("Client", "Warning"),
                QApplication::translate("Client", "Incorrect session type entered."),
                QMessageBox::Ok);
            return 1;
        }

        if (base::isHostId(config.address_or_id))
        {
            LOG(LS_INFO) << "Relay connection selected";

            if (!router_config.isValid())
            {
                QString title = QApplication::translate("Client", "Warning");
                QString message = QApplication::translate("Client",
                    "A host ID was entered, but the router was not configured. You need to "
                    "configure your router before connecting.");
                QMessageBox::warning(nullptr, title, message, QMessageBox::Ok);
                return 1;
            }

            config.router_config = router_config;
        }
        else
        {
            LOG(LS_INFO) << "Direct connection selected";
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
        client_window.reset(new client::ClientWindow());
        client_window->show();
        client_window->activateWindow();
    }

    return application.exec();
}
