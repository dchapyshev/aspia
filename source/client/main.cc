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

#include "client/main.h"

#include <QCommandLineParser>
#include <QSysInfo>

#include "base/logging.h"
#include "base/sys_info.h"
#include "build/build_config.h"
#include "build/version.h"
#include "client/config_factory.h"
#include "client/ui/settings.h"
#include "common/ui/msg_box.h"
#include "client/ui/application.h"
#include "client/ui/main_window.h"
#include "client/ui/chat/chat_session_window.h"
#include "client/ui/desktop/desktop_session_window.h"
#include "client/ui/file_transfer/file_transfer_session_window.h"
#include "client/ui/sys_info/system_info_session_window.h"

//--------------------------------------------------------------------------------------------------
void onInvalidValue(const QString& arg, const QString& values)
{
    common::MsgBox::warning(nullptr,
        QApplication::translate("Client", "Incorrect value for \"%1\". Possible values: %2.").arg(arg, values));
}

//--------------------------------------------------------------------------------------------------
bool parseAudioValue(const QString& value, proto::control::Config& config)
{
    if (!value.isEmpty())
    {
        if (value == "0")
        {
            config.set_audio(false);
        }
        else if (value == "1")
        {
            config.set_audio(true);
        }
        else
        {
            onInvalidValue("audio", "0, 1");
            return false;
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool parseCursorShapeValue(const QString& value, proto::control::Config& config)
{
    if (!value.isEmpty())
    {
        if (value == "0")
        {
            config.set_cursor_shape(false);
        }
        else if (value == "1")
        {
            config.set_cursor_shape(true);
        }
        else
        {
            onInvalidValue("cursor-shape", "0, 1");
            return false;
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool parseCursorPositionValue(const QString& value, proto::control::Config& config)
{
    if (!value.isEmpty())
    {
        if (value == "0")
        {
            config.set_cursor_position(false);
        }
        else if (value == "1")
        {
            config.set_cursor_position(true);
        }
        else
        {
            onInvalidValue("cursor-position", "0, 1");
            return false;
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool parseClipboardValue(const QString& value, proto::control::Config& config)
{
    if (!value.isEmpty())
    {
        if (value == "0")
        {
            config.set_clipboard(false);
        }
        else if (value == "1")
        {
            config.set_clipboard(true);
        }
        else
        {
            onInvalidValue("clipboard", "0, 1");
            return false;
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool parseDesktopEffectsValue(const QString& value, proto::control::Config& config)
{
    if (!value.isEmpty())
    {
        if (value == "0")
        {
            config.set_effects(false);
        }
        else if (value == "1")
        {
            config.set_effects(true);
        }
        else
        {
            onInvalidValue("desktop-effects", "0, 1");
            return false;
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool parseDesktopWallpaperValue(const QString& value, proto::control::Config& config)
{
    if (!value.isEmpty())
    {
        if (value == "0")
        {
            config.set_wallpaper(false);
        }
        else if (value == "1")
        {
            config.set_wallpaper(true);
        }
        else
        {
            onInvalidValue("desktop-wallpaper", "0, 1");
            return false;
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool parseLockAtDisconnectValue(const QString& value, proto::control::Config& config)
{
    if (!value.isEmpty())
    {
        if (value == "0")
        {
            config.set_lock_at_disconnect(false);
        }
        else if (value == "1")
        {
            config.set_lock_at_disconnect(true);
        }
        else
        {
            onInvalidValue("lock-at-disconnect", "0, 1");
            return false;
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool parseBlockRemoteInputValue(const QString& value, proto::control::Config& config)
{
    if (!value.isEmpty())
    {
        if (value == "0")
        {
            config.set_block_input(false);
        }
        else if (value == "1")
        {
            config.set_block_input(true);
        }
        else
        {
            onInvalidValue("block-remote-input", "0, 1");
            return false;
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
int clientMain(int argc, char* argv[])
{
    Q_INIT_RESOURCE(common);
    Q_INIT_RESOURCE(common_translations);

    base::LoggingSettings logging_settings;
    logging_settings.min_log_level = base::LOG_INFO;

    base::ScopedLogging scoped_logging(logging_settings);

    client::Application::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    client::Application application(argc, argv);

    LOG(INFO) << "Version:" << ASPIA_VERSION_STRING << "(arch:" << QSysInfo::buildCpuArchitecture() << ")";
#if defined(GIT_CURRENT_BRANCH) && defined(GIT_COMMIT_HASH)
    LOG(INFO) << "Git branch:" << GIT_CURRENT_BRANCH;
    LOG(INFO) << "Git commit:" << GIT_COMMIT_HASH;
#endif
    LOG(INFO) << "OS:" << base::SysInfo::operatingSystemName()
              << "(version:" << base::SysInfo::operatingSystemVersion()
              <<  "arch:" << base::SysInfo::operatingSystemArchitecture() << ")";
    LOG(INFO) << "Qt version:" << QT_VERSION_STR;
    LOG(INFO) << "Command line:" << application.arguments();

    QCommandLineOption address_option("address",
        QApplication::translate("Client", "Remote computer address."),
        "address");

    QCommandLineOption port_option("port",
        QApplication::translate("Client", "Remote computer port."),
        "port",
        QString::number(DEFAULT_HOST_TCP_PORT));

    QCommandLineOption name_option("name",
        QApplication::translate("Client", "Name of host."),
        "name");

    QCommandLineOption username_option("username",
        QApplication::translate("Client", "Name of user."),
        "username");

    QCommandLineOption password_option("password",
        QApplication::translate("Client", "Password of user."),
        "password");

    QCommandLineOption display_name_option("display-name",
        QApplication::translate("Client", "Display name when connected"),
        "display-name");

    QCommandLineOption session_type_option("session-type",
        QApplication::translate("Client", "Session type. Possible values: desktop, "
                                "file-transfer, system-info, text-chat."),
        "desktop-manage");

    QCommandLineOption audio_option("audio",
        QApplication::translate("Client", "Enable or disable audio. Possible values: 0 or 1."),
        "audio");

    QCommandLineOption cursor_shape_option("cursor-shape",
        QApplication::translate("Client", "Enable or disable cursor shape. Possible values: 0 or 1."),
        "cursor-shape");

    QCommandLineOption cursor_position_option("cursor-position",
        QApplication::translate("Client", "Enable or disable cursor position. Possible values: 0 or 1."),
        "cursor-position");

    QCommandLineOption clipboard_option("clipboard",
        QApplication::translate("Client", "Enable or disable clipboard. Possible values: 0 or 1."),
        "clipboard");

    QCommandLineOption desktop_effects_option("desktop-effects",
        QApplication::translate("Client", "Enable or disable desktop effects. Possible values: 0 or 1."),
        "desktop-effects");

    QCommandLineOption desktop_wallpaper_option("desktop-wallpaper",
        QApplication::translate("Client", "Enable or disable desktop wallpaper. Possible values: 0 or 1."),
        "desktop-wallpaper");

    QCommandLineOption lock_at_disconnect_option("lock-at-disconnect",
        QApplication::translate("Client", "Lock computer at disconnect. Possible values: 0 or 1."),
        "lock-at-disconnect");

    QCommandLineOption block_remote_input_option("block-remote-input",
        QApplication::translate("Client", "Block remote input. Possible values: 0 or 1."),
        "block-remote-input");

   QCommandLineOption router_address_option("router-address",
        QApplication::translate("Client", "Router address."),
        "router-address");

    QCommandLineOption router_port_option("router-port",
        QApplication::translate("Client", "Router port."),
        "router-port",
        QString::number(8060));

    QCommandLineOption router_username_option("router-username",
        QApplication::translate("Client", "Router name of user."),
        "router-username");

    QCommandLineOption router_password_option("router-password",
        QApplication::translate("Client", "Router password of user."),
        "router-password");

    QCommandLineParser parser;
    parser.setApplicationDescription(QApplication::translate("Client", "Aspia Client"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(address_option);
    parser.addOption(port_option);
    parser.addOption(name_option);
    parser.addOption(username_option);
    parser.addOption(password_option);
    parser.addOption(display_name_option);
    parser.addOption(session_type_option);
    parser.addOption(audio_option);
    parser.addOption(cursor_shape_option);
    parser.addOption(cursor_position_option);
    parser.addOption(clipboard_option);
    parser.addOption(desktop_effects_option);
    parser.addOption(desktop_wallpaper_option);
    parser.addOption(lock_at_disconnect_option);
    parser.addOption(block_remote_input_option);
    parser.addOption(router_address_option);
    parser.addOption(router_port_option);
    parser.addOption(router_username_option);
    parser.addOption(router_password_option);
    parser.process(application);

    std::unique_ptr<client::MainWindow> main_window;

    if (parser.isSet(address_option))
    {
        LOG(INFO) << "Command line start";

        std::optional<proto::control::Config> desktop_config;
        client::Config config;

        config.address_or_id = parser.value(address_option);
        config.port = parser.value(port_option).toUShort();
        config.username = parser.value(username_option);
        config.password = parser.value(password_option);

        if (parser.isSet(display_name_option))
            config.display_name = parser.value(display_name_option);

        if (parser.isSet(name_option))
            config.computer_name = parser.value(name_option);

        QString session_type = parser.value(session_type_option);

        if (session_type == "desktop")
        {
            config.session_type = proto::peer::SESSION_TYPE_DESKTOP;
            desktop_config = client::ConfigFactory::defaultDesktopConfig();
        }
        else if (session_type == "file-transfer")
        {
            config.session_type = proto::peer::SESSION_TYPE_FILE_TRANSFER;
        }
        else if (session_type == "system-info")
        {
            config.session_type = proto::peer::SESSION_TYPE_SYSTEM_INFO;
        }
        else if (session_type == "text-chat")
        {
            config.session_type = proto::peer::SESSION_TYPE_TEXT_CHAT;
        }
        else
        {
            LOG(ERROR) << "Unknown session type specified:" << session_type;
            onInvalidValue("session-type", "desktop, file-transfer, system-info, text-chat");
            return 1;
        }

        if (desktop_config.has_value())
        {
            if (!parseAudioValue(parser.value(audio_option), *desktop_config))
            {
                LOG(ERROR) << "Unable to parse audio value";
                return 1;
            }

            if (!parseCursorShapeValue(parser.value(cursor_shape_option), *desktop_config))
            {
                LOG(ERROR) << "Unable to parse cursor shape value";
                return 1;
            }

            if (!parseCursorPositionValue(parser.value(cursor_position_option), *desktop_config))
            {
                LOG(ERROR) << "Unable to parse cursor position value";
                return 1;
            }

            if (!parseClipboardValue(parser.value(clipboard_option), *desktop_config))
            {
                LOG(ERROR) << "Unable to parse clipboard value";
                return 1;
            }

            if (!parseDesktopEffectsValue(parser.value(desktop_effects_option), *desktop_config))
            {
                LOG(ERROR) << "Unable to parse desktop effects value";
                return 1;
            }

            if (!parseDesktopWallpaperValue(parser.value(desktop_wallpaper_option), *desktop_config))
            {
                LOG(ERROR) << "Unable to parse desktop wallpaper value";
                return 1;
            }

            if (!parseLockAtDisconnectValue(parser.value(lock_at_disconnect_option), *desktop_config))
            {
                LOG(ERROR) << "Unable to parse lock at disconnect value";
                return 1;
            }

            if (!parseBlockRemoteInputValue(parser.value(block_remote_input_option), *desktop_config))
            {
                LOG(ERROR) << "Unable to parse block remote input value";
                return 1;
            }
        }

        if (base::isHostId(config.address_or_id))
        {
            LOG(INFO) << "Relay connection selected";

            client::RouterConfig router_config;
            client::RouterConfigList router_configs = client::Settings().routerConfigs();
            if (!router_configs.isEmpty())
                router_config = router_configs.first();

            if (parser.isSet(router_address_option))
            {
                LOG(INFO) << "Router address option specified";

                router_config.address = parser.value(router_address_option);
                router_config.port = parser.value(router_port_option).toUShort();
                router_config.username = parser.value(router_username_option);
                router_config.password = parser.value(router_password_option);
            }
            else
            {
                LOG(INFO) << "Router address option not specified";
            }

            if (!router_config.isValid())
            {
                QString message = QApplication::translate("Client",
                    "A host ID was entered, but the router was not configured. You need to "
                    "configure your router before connecting.");
                common::MsgBox::warning(nullptr, message);
                return 1;
            }

            config.router_config = router_config;
        }
        else
        {
            LOG(INFO) << "Direct connection selected";
        }

        client::SessionWindow* session_window = nullptr;

        switch (config.session_type)
        {
            case proto::peer::SESSION_TYPE_DESKTOP:
                session_window = new client::DesktopSessionWindow(*desktop_config);
                break;

            case proto::peer::SESSION_TYPE_FILE_TRANSFER:
                session_window = new client::FileTransferSessionWindow();
                break;

            case proto::peer::SESSION_TYPE_SYSTEM_INFO:
                session_window = new client::SystemInfoSessionWindow();
                break;

            case proto::peer::SESSION_TYPE_TEXT_CHAT:
                session_window = new client::ChatSessionWindow();
                break;

            default:
                NOTREACHED();
                break;
        }

        if (!session_window)
        {
            LOG(ERROR) << "Session window not created";
            return 1;
        }

        session_window->setAttribute(Qt::WA_DeleteOnClose);
        if (!session_window->connectToHost(config))
        {
            LOG(ERROR) << "Unable to connect to host";
            return 0;
        }
    }
    else
    {
        LOG(INFO) << "Normal start";

        main_window.reset(new client::MainWindow());
        main_window->show();
        main_window->activateWindow();
    }

    return application.exec();
}
