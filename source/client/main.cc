//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include <QCommandLineParser>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSysInfo>

#include "base/logging.h"
#include "base/sys_info.h"
#include "base/peer/host_id.h"
#include "build/version.h"
#include "client/config_factory.h"
#include "client/master_password.h"
#include "common/ui/msg_box.h"
#include "common/ui/status_dialog.h"
#include "client/router.h"
#include "client/ui/application.h"
#include "client/ui/main_window.h"
#include "client/ui/master_password_dialog.h"
#include "client/ui/unlock_dialog.h"
#include "client/ui/chat/chat_window.h"
#include "client/ui/desktop/desktop_window.h"
#include "client/ui/file_transfer/file_transfer_window.h"
#include "client/ui/sys_info/system_info_window.h"
#include "proto/desktop_control.h"

//--------------------------------------------------------------------------------------------------
bool startSession(const ComputerConfig& computer,
                  proto::peer::SessionType session_type,
                  const QString& display_name,
                  const proto::control::Config& desktop_config)
{
    ClientWindow* client_window = nullptr;

    switch (session_type)
    {
        case proto::peer::SESSION_TYPE_DESKTOP:
            client_window = new DesktopWindow(desktop_config);
            break;

        case proto::peer::SESSION_TYPE_FILE_TRANSFER:
            client_window = new FileTransferWindow();
            break;

        case proto::peer::SESSION_TYPE_SYSTEM_INFO:
            client_window = new SystemInfoWindow();
            break;

        case proto::peer::SESSION_TYPE_TEXT_CHAT:
            client_window = new ChatWindow();
            break;

        default:
            NOTREACHED();
            break;
    }

    if (!client_window)
    {
        LOG(ERROR) << "Session window not created";
        return false;
    }

    client_window->setAttribute(Qt::WA_DeleteOnClose);

    QObject::connect(client_window, &ClientWindow::sig_stop, qApp, &QApplication::quit);

    if (!client_window->connectToHost(computer, display_name))
        LOG(ERROR) << "Unable to connect to host";

    return true;
}

//--------------------------------------------------------------------------------------------------
void startRouterSession(const ComputerConfig& computer,
                        proto::peer::SessionType session_type,
                        const QString& display_name,
                        const RouterConfig& router_config,
                        const proto::control::Config& desktop_config)
{
    QPointer<StatusDialog> status_dialog = new StatusDialog();
    status_dialog->setAttribute(Qt::WA_DeleteOnClose);

    QPointer<Router> router = new Router(router_config);
    router->moveToThread(GuiApplication::ioThread());

    QObject::connect(router, &Router::sig_statusChanged, qApp,
        [status_dialog, router, computer, session_type, display_name, desktop_config](qint64, Router::Status status)
    {
        if (!router || !status_dialog)
            return;

        const QString& address = router->config().address;

        switch (status)
        {
            case Router::Status::CONNECTING:
                status_dialog->addMessage(
                    QApplication::translate("Client", "Connecting to router %1...").arg(address));
                break;

            case Router::Status::ONLINE:
                status_dialog->addMessage(
                    QApplication::translate("Client", "Connection to router %1 established.").arg(address));
                router->disconnect(qApp);
                status_dialog->hide();
                status_dialog->deleteLater();
                startSession(computer, session_type, display_name, desktop_config);
                break;

            case Router::Status::OFFLINE:
                status_dialog->addMessage(
                    QApplication::translate("Client", "Disconnected from router %1.").arg(address));
                break;
        }
    }, Qt::QueuedConnection);

    QObject::connect(router, &Router::sig_errorOccurred, qApp,
        [status_dialog](qint64, TcpChannel::ErrorCode error_code)
    {
        if (!status_dialog)
            return;

        status_dialog->addMessage(
            QApplication::translate("Client", "Network error: %1.").arg(TcpChannel::errorToString(error_code)));
    }, Qt::QueuedConnection);

    status_dialog->show();
    status_dialog->activateWindow();

    QMetaObject::invokeMethod(router, &Router::onConnectToRouter, Qt::QueuedConnection);
}

//--------------------------------------------------------------------------------------------------
// Reads a one-time JSON connection config from stdin until EOF and starts a session.
// The aggregator that launches the client is expected to hold the JSON in memory and pipe it
// directly to the child's stdin, without touching disk.
//
// Example invocations from a parent process:
//
//     PowerShell:
//          $config = @{
//              session_type = "desktop"
//              display_name = "Admin"
//              computer = @{
//                  name     = "Office PC"
//                  address  = "192.168.1.10"
//                  username = "user"
//                  password = "1"
//              }
//              desktop = @{
//                  audio              = $true
//                  cursor_shape       = $true
//                  cursor_position    = $true
//                  clipboard          = $true
//                  effects            = $true
//                  wallpaper          = $false
//                  lock_at_disconnect = $false
//                  block_input        = $false
//              }
//          } | ConvertTo-Json -Depth 5
//          $config | & "C:\Test\aspia_client.exe" --connect
//
//     bash (here-doc):
//         aspia_client --connect <<'EOF'
//         {
//             "session_type": "desktop",
//             "display_name": "Admin",
//             "computer": {
//                 "name": "Office PC",
//                 "address": "192.168.1.10",
//                 "username": "user",
//                 "password": "1"
//             },
//             "desktop": {
//                 "audio":              true,
//                 "cursor_shape":       true,
//                 "cursor_position":    true,
//                 "clipboard":          true,
//                 "effects":            true,
//                 "wallpaper":          false,
//                 "lock_at_disconnect": false,
//                 "block_input":        false
//             }
//         }
//         EOF
//
//     Python:
//         import json, subprocess
//         config = {
//             "session_type": "desktop",
//             "display_name": "Admin",
//             "computer": {
//                 "name":     "Office PC",
//                 "address":  "192.168.1.10",
//                 "username": "user",
//                 "password": "1"
//             },
//             "desktop": {
//                 "audio":              True,
//                 "cursor_shape":       True,
//                 "cursor_position":    True,
//                 "clipboard":          True,
//                 "effects":            True,
//                 "wallpaper":          False,
//                 "lock_at_disconnect": False,
//                 "block_input":        False
//             }
//         }
//         proc = subprocess.Popen(['/usr/bin/aspia_client', '--connect'], stdin=subprocess.PIPE)
//         proc.communicate(input=json.dumps(config).encode('utf-8'))
//
//     Win32 (C/C++):
//         HANDLE read_end, write_end;
//         SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
//         CreatePipe(&read_end, &write_end, &sa, 0);
//         SetHandleInformation(write_end, HANDLE_FLAG_INHERIT, 0);
//
//         STARTUPINFOW si = { sizeof(si) };
//         si.dwFlags = STARTF_USESTDHANDLES;
//         si.hStdInput = read_end;
//
//         PROCESS_INFORMATION pi = {};
//         WCHAR cmd[] = L"\"C:\\Test\\aspia_client.exe\" --connect";
//         CreateProcessW(nullptr, cmd, nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi);
//         CloseHandle(read_end);
//
//         const char* json =
//             "{"
//                 "\"session_type\":\"desktop\","
//                 "\"display_name\":\"Admin\","
//                 "\"computer\":{"
//                     "\"name\":\"Office PC\","
//                     "\"address\":\"192.168.1.10\","
//                     "\"username\":\"user\","
//                     "\"password\":\"1\""
//                 "},"
//                 "\"desktop\":{"
//                     "\"audio\":true,"
//                     "\"cursor_shape\":true,"
//                     "\"cursor_position\":true,"
//                     "\"clipboard\":true,"
//                     "\"effects\":true,"
//                     "\"wallpaper\":false,"
//                     "\"lock_at_disconnect\":false,"
//                     "\"block_input\":false"
//                 "}"
//             "}";
//         DWORD written;
//         WriteFile(write_end, json, (DWORD)strlen(json), &written, nullptr);
//         CloseHandle(write_end);
//         CloseHandle(pi.hProcess);
//         CloseHandle(pi.hThread);
//
// Required: "computer.address".
// Optional: "session_type" (defaults to "desktop" if missing or unknown), "display_name",
//           "computer.name", "computer.username", "computer.password",
//           "desktop" (used only for "session_type": "desktop").
// The "router" object is required when "computer.address" is a host ID; in that case
// "router.address", "router.username" and "router.password" are all required.
// Possible "session_type" values: "desktop", "file-transfer", "system-info", "chat".
//--------------------------------------------------------------------------------------------------
bool handleConnect()
{
    QFile stdin_file;
    if (!stdin_file.open(stdin, QIODevice::ReadOnly))
    {
        LOG(ERROR) << "Unable to open stdin for reading";
        MsgBox::warning(nullptr, QApplication::translate("Client",
            "Unable to read connection config from stdin."));
        return false;
    }

    QByteArray config_data = stdin_file.readAll();
    stdin_file.close();

    if (config_data.isEmpty())
    {
        LOG(ERROR) << "Empty connection config from stdin";
        MsgBox::warning(nullptr, QApplication::translate("Client",
            "Empty connection config from stdin."));
        return false;
    }

    QJsonParseError parse_error;
    QJsonDocument doc = QJsonDocument::fromJson(config_data, &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !doc.isObject())
    {
        LOG(ERROR) << "Invalid JSON in connection config:" << parse_error.errorString();
        MsgBox::warning(nullptr, QApplication::translate("Client",
            "Invalid JSON in connection config: %1").arg(parse_error.errorString()));
        return false;
    }

    QJsonObject root = doc.object();

    QString session_type_value = root.value("session_type").toString();
    proto::peer::SessionType session_type = proto::peer::SESSION_TYPE_DESKTOP;

    if (session_type_value == "desktop")
        session_type = proto::peer::SESSION_TYPE_DESKTOP;
    else if (session_type_value == "file-transfer")
        session_type = proto::peer::SESSION_TYPE_FILE_TRANSFER;
    else if (session_type_value == "system-info")
        session_type = proto::peer::SESSION_TYPE_SYSTEM_INFO;
    else if (session_type_value == "chat")
        session_type = proto::peer::SESSION_TYPE_TEXT_CHAT;

    QJsonValue computer_value = root.value("computer");
    if (!computer_value.isObject())
    {
        LOG(ERROR) << "Missing or invalid \"computer\" object in connection config";
        MsgBox::warning(nullptr, QApplication::translate("Client",
            "Missing or invalid \"computer\" object in connection config."));
        return false;
    }

    QJsonObject computer_object = computer_value.toObject();

    ComputerConfig computer;
    computer.address = computer_object.value("address").toString();
    computer.username = computer_object.value("username").toString();
    computer.password = computer_object.value("password").toString();
    computer.name = computer_object.value("name").toString();

    if (computer.address.isEmpty())
    {
        LOG(ERROR) << "Missing required computer field (address)";
        MsgBox::warning(nullptr, QApplication::translate("Client",
            "Missing required computer field: address."));
        return false;
    }

    QString display_name = root.value("display_name").toString();

    proto::control::Config desktop_config = ConfigFactory::defaultDesktopConfig();

    if (session_type == proto::peer::SESSION_TYPE_DESKTOP && root.contains("desktop"))
    {
        QJsonValue desktop_value = root.value("desktop");
        if (!desktop_value.isObject())
        {
            LOG(ERROR) << "Field \"desktop\" must be an object";
            MsgBox::warning(nullptr, QApplication::translate("Client",
                "Field \"desktop\" must be an object."));
            return false;
        }

        QJsonObject desktop_object = desktop_value.toObject();

        auto applyDesktopBool = [&desktop_object, &desktop_config]
            (const char* key, void (proto::control::Config::*setter)(bool)) -> bool
        {
            if (!desktop_object.contains(key))
                return true;

            QJsonValue value = desktop_object.value(key);
            if (!value.isBool())
            {
                LOG(ERROR) << "Field \"desktop." << key << "\" must be boolean";
                MsgBox::warning(nullptr, QApplication::translate("Client",
                    "Field \"desktop.%1\" must be boolean.").arg(QString::fromUtf8(key)));
                return false;
            }

            (desktop_config.*setter)(value.toBool());
            return true;
        };

        if (!applyDesktopBool("audio", &proto::control::Config::set_audio))
            return false;
        if (!applyDesktopBool("cursor_shape", &proto::control::Config::set_cursor_shape))
            return false;
        if (!applyDesktopBool("cursor_position", &proto::control::Config::set_cursor_position))
            return false;
        if (!applyDesktopBool("clipboard", &proto::control::Config::set_clipboard))
            return false;
        if (!applyDesktopBool("effects", &proto::control::Config::set_effects))
            return false;
        if (!applyDesktopBool("wallpaper", &proto::control::Config::set_wallpaper))
            return false;
        if (!applyDesktopBool("lock_at_disconnect", &proto::control::Config::set_lock_at_disconnect))
            return false;
        if (!applyDesktopBool("block_input", &proto::control::Config::set_block_input))
            return false;
    }

    if (isHostId(computer.address))
    {
        LOG(INFO) << "Relay connection selected";

        QJsonValue router_value = root.value("router");
        if (!router_value.isObject())
        {
            LOG(INFO) << "Router object not specified";
            MsgBox::warning(nullptr, QApplication::translate("Client",
                "Connection parameters to the router are not specified."));
            return false;
        }

        QJsonObject router_object = router_value.toObject();

        RouterConfig router_config;
        router_config.router_id = 1;
        router_config.address = router_object.value("address").toString();
        router_config.username = router_object.value("username").toString();
        router_config.password = router_object.value("password").toString();
        router_config.session_type = proto::router::SESSION_TYPE_CLIENT;

        if (!router_config.isValid())
        {
            MsgBox::warning(nullptr, QApplication::translate("Client",
                "Incorrect data for connecting to the router."));
            return false;
        }

        computer.router_id = router_config.router_id;
        startRouterSession(computer, session_type, display_name, router_config, desktop_config);
    }
    else
    {
        LOG(INFO) << "Direct connection selected";

        if (!startSession(computer, session_type, display_name, desktop_config))
            return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    Q_INIT_RESOURCE(common);
    Q_INIT_RESOURCE(common_translations);

    LoggingSettings logging_settings;
    logging_settings.min_log_level = LOG_INFO;

    ScopedLogging scoped_logging(logging_settings);

    Application::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    Application application(argc, argv);

    LOG(INFO) << "Version:" << ASPIA_VERSION_STRING << "(arch:" << QSysInfo::buildCpuArchitecture() << ")";
#if defined(GIT_CURRENT_BRANCH) && defined(GIT_COMMIT_HASH)
    LOG(INFO) << "Git branch:" << GIT_CURRENT_BRANCH;
    LOG(INFO) << "Git commit:" << GIT_COMMIT_HASH;
#endif
    LOG(INFO) << "OS:" << SysInfo::operatingSystemName()
              << "(version:" << SysInfo::operatingSystemVersion()
              <<  "arch:" << SysInfo::operatingSystemArchitecture() << ")";
    LOG(INFO) << "Qt version:" << QT_VERSION_STR;
    LOG(INFO) << "Command line:" << application.arguments();

    QCommandLineOption connect_option("connect",
        QApplication::translate("Client", "Read JSON connection config from stdin until EOF and start a session."));

    QCommandLineParser parser;
    parser.setApplicationDescription(QApplication::translate("Client", "Aspia Client"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(connect_option);
    parser.process(application);

    if (parser.isSet(connect_option))
    {
        if (!handleConnect())
            return 1;
        return application.exec();
    }

    if (application.isRunning())
    {
        LOG(INFO) << "Another instance is already running, activating its window";
        application.activateWindow();
        return 0;
    }

    if (MasterPassword::isSet())
    {
        LOG(INFO) << "Master password is set, prompting user";

        while (true)
        {
            UnlockDialog dialog(nullptr, QString(), QString());
            if (dialog.exec() != QDialog::Accepted)
            {
                LOG(INFO) << "Master password unlock cancelled by user";
                return 0;
            }

            if (MasterPassword::unlock(dialog.password()))
            {
                LOG(INFO) << "Master password accepted";
                break;
            }

            MsgBox::warning(nullptr, QApplication::translate("Client", "Invalid master password."));
        }
    }
    else
    {
        LOG(INFO) << "Master password is not set, prompting user to set one";

        MasterPasswordDialog dialog(MasterPasswordDialog::Mode::SET);
        if (dialog.exec() != QDialog::Accepted)
        {
            LOG(INFO) << "Master password set cancelled by user";
            return 0;
        }

        LOG(INFO) << "Master password set";
    }

    std::unique_ptr<MainWindow> main_window = std::make_unique<MainWindow>();

    QObject::connect(&application, &Application::sig_windowActivated,
                     main_window.get(), &MainWindow::showAndActivate);

    main_window->show();
    main_window->activateWindow();

    return application.exec();
}
