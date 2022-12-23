//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "client/ui/qt_system_info_window.h"
#include "client/ui/qt_text_chat_window.h"
#include "qt_base/scoped_qt_logging.h"

#if defined(OS_WIN)
#include "base/win/mini_dump_writer.h"
#endif

#include <QCommandLineParser>
#include <QMessageBox>

void serializePixelFormat(const base::PixelFormat& from, proto::PixelFormat* to)
{
    to->set_bits_per_pixel(from.bitsPerPixel());

    to->set_red_max(from.redMax());
    to->set_green_max(from.greenMax());
    to->set_blue_max(from.blueMax());

    to->set_red_shift(from.redShift());
    to->set_green_shift(from.greenShift());
    to->set_blue_shift(from.blueShift());
}

void onInvalidValue(const QString& arg, const QString& values)
{
    QMessageBox::warning(
        nullptr,
        QApplication::translate("Client", "Warning"),
        QApplication::translate("Client", "Incorrect value for \"%1\". Possible values: %2.").arg(arg, values),
        QMessageBox::Ok);
}

bool parseCodecValue(const QString& value, proto::DesktopConfig& config)
{
    if (!value.isEmpty())
    {
        if (value == QLatin1String("vp8"))
        {
            config.set_video_encoding(proto::VIDEO_ENCODING_VP8);
        }
        else if (value == QLatin1String("vp9"))
        {
            config.set_video_encoding(proto::VIDEO_ENCODING_VP9);
        }
        else if (value == QLatin1String("zstd"))
        {
            config.set_video_encoding(proto::VIDEO_ENCODING_ZSTD);
        }
        else
        {
            onInvalidValue(QStringLiteral("codec"), QStringLiteral("vp8, vp9, zstd"));
            return false;
        }
    }

    return true;
}

bool parseColorDepthValue(const QString& value, proto::DesktopConfig& config)
{
    if (!value.isEmpty())
    {
        if (value == QLatin1String("32"))
        {
            serializePixelFormat(base::PixelFormat::ARGB(), config.mutable_pixel_format());
        }
        else if (value == QLatin1String("16"))
        {
            serializePixelFormat(base::PixelFormat::RGB565(), config.mutable_pixel_format());
        }
        else if (value == QLatin1String("8"))
        {
            serializePixelFormat(base::PixelFormat::RGB332(), config.mutable_pixel_format());
        }
        else if (value == QLatin1String("6"))
        {
            serializePixelFormat(base::PixelFormat::RGB222(), config.mutable_pixel_format());
        }
        else if (value == QLatin1String("3"))
        {
            serializePixelFormat(base::PixelFormat::RGB111(), config.mutable_pixel_format());
        }
        else
        {
            onInvalidValue(QStringLiteral("color-depth"), QStringLiteral("3, 6, 8, 16, 32"));
            return false;
        }
    }

    return true;
}

bool parseCompressRatioValue(const QString& value, proto::DesktopConfig& config)
{
    if (!value.isEmpty())
    {
        uint32_t compress_ratio = value.toUInt();
        if (compress_ratio >= 1 && compress_ratio <= 22)
        {
            config.set_compress_ratio(compress_ratio);
        }
        else
        {
            onInvalidValue(QStringLiteral("compress-ratio"), QStringLiteral("1-22"));
            return false;
        }
    }

    return true;
}

bool parseAudioValue(const QString& value, proto::DesktopConfig& config)
{
    if (!value.isEmpty())
    {
        if (value == QLatin1String("0"))
        {
            config.set_audio_encoding(proto::AUDIO_ENCODING_UNKNOWN);
        }
        else if (value == QLatin1String("1"))
        {
            config.set_audio_encoding(proto::AUDIO_ENCODING_OPUS);
        }
        else
        {
            onInvalidValue(QStringLiteral("audio"), QStringLiteral("0, 1"));
            return false;
        }
    }

    return true;
}

bool parseCursorShapeValue(const QString& value, proto::DesktopConfig& config)
{
    if (!value.isEmpty())
    {
        if (value == QLatin1String("0"))
        {
            config.set_flags(config.flags() & ~static_cast<uint32_t>(proto::ENABLE_CURSOR_SHAPE));
        }
        else if (value == QLatin1String("1"))
        {
            config.set_flags(config.flags() | proto::ENABLE_CURSOR_SHAPE);
        }
        else
        {
            onInvalidValue(QStringLiteral("cursor-shape"), QStringLiteral("0, 1"));
            return false;
        }
    }

    return true;
}

bool parseCursorPositionValue(const QString& value, proto::DesktopConfig& config)
{
    if (!value.isEmpty())
    {
        if (value == QLatin1String("0"))
        {
            config.set_flags(config.flags() & ~static_cast<uint32_t>(proto::CURSOR_POSITION));
        }
        else if (value == QLatin1String("1"))
        {
            config.set_flags(config.flags() | proto::CURSOR_POSITION);
        }
        else
        {
            onInvalidValue(QStringLiteral("cursor-position"), QStringLiteral("0, 1"));
            return false;
        }
    }

    return true;
}

bool parseClipboardValue(const QString& value, proto::DesktopConfig& config)
{
    if (!value.isEmpty())
    {
        if (value == QLatin1String("0"))
        {
            config.set_flags(config.flags() & ~static_cast<uint32_t>(proto::ENABLE_CLIPBOARD));
        }
        else if (value == QLatin1String("1"))
        {
            config.set_flags(config.flags() | proto::ENABLE_CLIPBOARD);
        }
        else
        {
            onInvalidValue(QStringLiteral("clipboard"), QStringLiteral("0, 1"));
            return false;
        }
    }

    return true;
}

bool parseDesktopEffectsValue(const QString& value, proto::DesktopConfig& config)
{
    if (!value.isEmpty())
    {
        if (value == QLatin1String("0"))
        {
            config.set_flags(config.flags() | proto::DISABLE_DESKTOP_EFFECTS);
        }
        else if (value == QLatin1String("1"))
        {
            config.set_flags(config.flags() & ~static_cast<uint32_t>(proto::DISABLE_DESKTOP_EFFECTS));
        }
        else
        {
            onInvalidValue(QStringLiteral("desktop-effects"), QStringLiteral("0, 1"));
            return false;
        }
    }

    return true;
}

bool parseDesktopWallpaperValue(const QString& value, proto::DesktopConfig& config)
{
    if (!value.isEmpty())
    {
        if (value == QLatin1String("0"))
        {
            config.set_flags(config.flags() | proto::DISABLE_DESKTOP_WALLPAPER);
        }
        else if (value == QLatin1String("1"))
        {
            config.set_flags(config.flags() & ~static_cast<uint32_t>(proto::DISABLE_DESKTOP_WALLPAPER));
        }
        else
        {
            onInvalidValue(QStringLiteral("desktop-wallpaper"), QStringLiteral("0, 1"));
            return false;
        }
    }

    return true;
}

bool parseFontSmoothingValue(const QString& value, proto::DesktopConfig& config)
{
    if (!value.isEmpty())
    {
        if (value == QLatin1String("0"))
        {
            config.set_flags(config.flags() | proto::DISABLE_FONT_SMOOTHING);
        }
        else if (value == QLatin1String("1"))
        {
            config.set_flags(config.flags() & ~static_cast<uint32_t>(proto::DISABLE_FONT_SMOOTHING));
        }
        else
        {
            onInvalidValue(QStringLiteral("font-smoothing"), QStringLiteral("0, 1"));
            return false;
        }
    }

    return true;
}

bool parseClearClipboardValue(const QString& value, proto::DesktopConfig& config)
{
    if (!value.isEmpty())
    {
        if (value == QLatin1String("0"))
        {
            config.set_flags(config.flags() & ~static_cast<uint32_t>(proto::CLEAR_CLIPBOARD));
        }
        else if (value == QLatin1String("1"))
        {
            config.set_flags(config.flags() | proto::CLEAR_CLIPBOARD);
        }
        else
        {
            onInvalidValue(QStringLiteral("clear-clipboard"), QStringLiteral("0, 1"));
            return false;
        }
    }

    return true;
}

bool parseLockAtDisconnectValue(const QString& value, proto::DesktopConfig& config)
{
    if (!value.isEmpty())
    {
        if (value == QLatin1String("0"))
        {
            config.set_flags(config.flags() & ~static_cast<uint32_t>(proto::LOCK_AT_DISCONNECT));
        }
        else if (value == QLatin1String("1"))
        {
            config.set_flags(config.flags() | proto::LOCK_AT_DISCONNECT);
        }
        else
        {
            onInvalidValue(QStringLiteral("lock-at-disconnect"), QStringLiteral("0, 1"));
            return false;
        }
    }

    return true;
}

bool parseBlockRemoteInputValue(const QString& value, proto::DesktopConfig& config)
{
    if (!value.isEmpty())
    {
        if (value == QLatin1String("0"))
        {
            config.set_flags(config.flags() & ~static_cast<uint32_t>(proto::BLOCK_REMOTE_INPUT));
        }
        else if (value == QLatin1String("1"))
        {
            config.set_flags(config.flags() | proto::BLOCK_REMOTE_INPUT);
        }
        else
        {
            onInvalidValue(QStringLiteral("block-remote-input"), QStringLiteral("0, 1"));
            return false;
        }
    }

    return true;
}

int clientMain(int argc, char* argv[])
{
#if !defined(I18L_DISABLED)
    Q_INIT_RESOURCE(client);
    Q_INIT_RESOURCE(client_translations);
    Q_INIT_RESOURCE(common);
    Q_INIT_RESOURCE(common_translations);
#endif

#if defined(OS_WIN)
    base::installFailureHandler(L"aspia_client");
#endif

    base::LoggingSettings logging_settings;
    logging_settings.min_log_level = base::LOG_LS_INFO;

    qt_base::ScopedQtLogging scoped_logging(logging_settings);

    client::Application::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    client::Application::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
    client::Application::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    client::Application application(argc, argv);

    LOG(LS_INFO) << "Version: " << ASPIA_VERSION_STRING;
#if defined(GIT_CURRENT_BRANCH) && defined(GIT_COMMIT_HASH)
    LOG(LS_INFO) << "Git branch: " << GIT_CURRENT_BRANCH;
    LOG(LS_INFO) << "Git commit: " << GIT_COMMIT_HASH;
#endif
    LOG(LS_INFO) << "Qt version: " << QT_VERSION_STR;
    LOG(LS_INFO) << "Command line: " << application.arguments();

    QCommandLineOption address_option(QStringLiteral("address"),
        QApplication::translate("Client", "Remote computer address."),
        QStringLiteral("address"));

    QCommandLineOption port_option(QStringLiteral("port"),
        QApplication::translate("Client", "Remote computer port."),
        QStringLiteral("port"),
        QString::number(DEFAULT_HOST_TCP_PORT));

    QCommandLineOption name_option(QStringLiteral("name"),
        QApplication::translate("Client", "Name of host."),
        QStringLiteral("name"));

    QCommandLineOption username_option(QStringLiteral("username"),
        QApplication::translate("Client", "Name of user."),
        QStringLiteral("username"));

    QCommandLineOption password_option(QStringLiteral("password"),
        QApplication::translate("Client", "Password of user."),
        QStringLiteral("password"));

    QCommandLineOption session_type_option(QStringLiteral("session-type"),
        QApplication::translate("Client", "Session type. Possible values: desktop-manage, "
                                "desktop-view, file-transfer, system-info, text-chat."),
        QStringLiteral("desktop-manage"));

    QCommandLineOption codec_option(QStringLiteral("codec"),
        QApplication::translate("Client", "Type of codec. Possible values: vp8, vp9, zstd."),
        QStringLiteral("codec"));

    QCommandLineOption color_depth_option(QStringLiteral("color-depth"),
        QApplication::translate("Client", "Color depth. Possible values: 3, 6, 8, 16, 32."),
        QStringLiteral("color-depth"));

    QCommandLineOption compress_ratio_option(QStringLiteral("compress-ratio"),
        QApplication::translate("Client", "Compression ratio. Possible values: 1-22."),
        QStringLiteral("compress-ratio"));

    QCommandLineOption audio_option(QStringLiteral("audio"),
        QApplication::translate("Client", "Enable or disable audio. Possible values: 0 or 1."),
        QStringLiteral("audio"));

    QCommandLineOption cursor_shape_option(QStringLiteral("cursor-shape"),
        QApplication::translate("Client", "Enable or disable cursor shape. Possible values: 0 or 1."),
        QStringLiteral("cursor-shape"));

    QCommandLineOption cursor_position_option(QStringLiteral("cursor-position"),
        QApplication::translate("Client", "Enable or disable cursor position. Possible values: 0 or 1."),
        QStringLiteral("cursor-position"));

    QCommandLineOption clipboard_option(QStringLiteral("clipboard"),
        QApplication::translate("Client", "Enable or disable clipboard. Possible values: 0 or 1."),
        QStringLiteral("clipboard"));

    QCommandLineOption desktop_effects_option(QStringLiteral("desktop-effects"),
        QApplication::translate("Client", "Enable or disable desktop effects. Possible values: 0 or 1."),
        QStringLiteral("desktop-effects"));

    QCommandLineOption desktop_wallpaper_option(QStringLiteral("desktop-wallpaper"),
        QApplication::translate("Client", "Enable or disable desktop wallpaper. Possible values: 0 or 1."),
        QStringLiteral("desktop-wallpaper"));

    QCommandLineOption font_smoothing_option(QStringLiteral("font-smoothing"),
        QApplication::translate("Client", "Enable or disable font smoothing. Possible values: 0 or 1."),
        QStringLiteral("font-smoothing"));

    QCommandLineOption clear_clipboard_option(QStringLiteral("clear-clipboard"),
        QApplication::translate("Client", "Clear clipboard at disconnect. Possible values: 0 or 1."),
        QStringLiteral("clear-clipboard"));

    QCommandLineOption lock_at_disconnect_option(QStringLiteral("lock-at-disconnect"),
        QApplication::translate("Client", "Lock computer at disconnect. Possible values: 0 or 1."),
        QStringLiteral("lock-at-disconnect"));

    QCommandLineOption block_remote_input_option(QStringLiteral("block-remote-input"),
        QApplication::translate("Client", "Block remote input. Possible values: 0 or 1."),
        QStringLiteral("block-remote-input"));

   QCommandLineOption router_address_option(QStringLiteral("router_address"),
        QApplication::translate("Client", "Router address."),
        QStringLiteral("router_address"));

    QCommandLineOption router_port_option(QStringLiteral("router_port"),
        QApplication::translate("Client", "Router port."),
        QStringLiteral("router_port"),
        QString::number(8060));

    QCommandLineOption router_username_option(QStringLiteral("router_username"),
        QApplication::translate("Client", "Router name of user."),
        QStringLiteral("router_username"));

    QCommandLineOption router_password_option(QStringLiteral("router_password"),
        QApplication::translate("Client", "Router password of user."),
        QStringLiteral("router_password"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QApplication::translate("Client", "Aspia Client"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(address_option);
    parser.addOption(port_option);
    parser.addOption(name_option);
    parser.addOption(username_option);
    parser.addOption(password_option);
    parser.addOption(session_type_option);
    parser.addOption(codec_option);
    parser.addOption(color_depth_option);
    parser.addOption(compress_ratio_option);
    parser.addOption(audio_option);
    parser.addOption(cursor_shape_option);
    parser.addOption(cursor_position_option);
    parser.addOption(clipboard_option);
    parser.addOption(desktop_effects_option);
    parser.addOption(desktop_wallpaper_option);
    parser.addOption(font_smoothing_option);
    parser.addOption(clear_clipboard_option);
    parser.addOption(lock_at_disconnect_option);
    parser.addOption(block_remote_input_option);
    parser.addOption(router_address_option);
    parser.addOption(router_port_option);
    parser.addOption(router_username_option);
    parser.addOption(router_password_option);
    parser.process(application);

    std::unique_ptr<client::ClientWindow> client_window;

    if (parser.isSet(address_option))
    {
        std::optional<proto::DesktopConfig> desktop_config;
        client::Config config;

        config.address_or_id = parser.value(address_option).toStdU16String();
        config.port = parser.value(port_option).toUShort();
        config.username = parser.value(username_option).toStdU16String();
        config.password = parser.value(password_option).toStdU16String();

        if (parser.isSet(name_option))
        {
            config.computer_name = parser.value(name_option).toStdU16String();
        }

        QString session_type = parser.value(session_type_option);

        if (session_type == QLatin1String("desktop-manage"))
        {
            config.session_type = proto::SESSION_TYPE_DESKTOP_MANAGE;
            desktop_config = client::ConfigFactory::defaultDesktopManageConfig();
        }
        else if (session_type == QLatin1String("desktop-view"))
        {
            config.session_type = proto::SESSION_TYPE_DESKTOP_VIEW;
            desktop_config = client::ConfigFactory::defaultDesktopViewConfig();
        }
        else if (session_type == QLatin1String("file-transfer"))
        {
            config.session_type = proto::SESSION_TYPE_FILE_TRANSFER;
        }
        else if (session_type == QLatin1String("system-info"))
        {
            config.session_type = proto::SESSION_TYPE_SYSTEM_INFO;
        }
        else if (session_type == QLatin1String("text-chat"))
        {
            config.session_type = proto::SESSION_TYPE_TEXT_CHAT;
        }
        else
        {
            onInvalidValue(QStringLiteral("session-type"),
                           QStringLiteral("desktop-manage, desktop-view, file-transfer, system-info, text-chat"));
            return 1;
        }

        if (desktop_config.has_value())
        {
            if (!parseCodecValue(parser.value(codec_option), *desktop_config))
                return 1;

            if (desktop_config->video_encoding() == proto::VIDEO_ENCODING_ZSTD)
            {
                if (!parseColorDepthValue(parser.value(color_depth_option), *desktop_config))
                    return 1;

                if (!parseCompressRatioValue(parser.value(compress_ratio_option), *desktop_config))
                    return 1;
            }

            if (!parseAudioValue(parser.value(audio_option), *desktop_config))
                return 1;

            if (!parseCursorShapeValue(parser.value(cursor_shape_option), *desktop_config))
                return 1;

            if (!parseCursorPositionValue(parser.value(cursor_position_option), *desktop_config))
                return 1;

            if (!parseClipboardValue(parser.value(clipboard_option), *desktop_config))
                return 1;

            if (!parseDesktopEffectsValue(parser.value(desktop_effects_option), *desktop_config))
                return 1;

            if (!parseDesktopWallpaperValue(parser.value(desktop_wallpaper_option), *desktop_config))
                return 1;

            if (!parseFontSmoothingValue(parser.value(font_smoothing_option), *desktop_config))
                return 1;

            if (!parseClearClipboardValue(parser.value(clear_clipboard_option), *desktop_config))
                return 1;

            if (!parseLockAtDisconnectValue(parser.value(lock_at_disconnect_option), *desktop_config))
                return 1;

            if (!parseBlockRemoteInputValue(parser.value(block_remote_input_option), *desktop_config))
                return 1;
        }

        if (base::isHostId(config.address_or_id))
        {
            LOG(LS_INFO) << "Relay connection selected";

            client::RouterConfig router_config = client::RouterConfigStorage().routerConfig();

            if (parser.isSet(router_address_option))
            {
                router_config.address = parser.value(router_address_option).toStdU16String();
                router_config.port = parser.value(router_port_option).toUShort();
                router_config.username = parser.value(router_username_option).toStdU16String();
                router_config.password = parser.value(router_password_option).toStdU16String();
            }

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
                session_window = new client::QtDesktopWindow(config.session_type, *desktop_config);
                break;

            case proto::SESSION_TYPE_DESKTOP_VIEW:
                session_window = new client::QtDesktopWindow(config.session_type, *desktop_config);
                break;

            case proto::SESSION_TYPE_FILE_TRANSFER:
                session_window = new client::QtFileManagerWindow();
                break;

            case proto::SESSION_TYPE_SYSTEM_INFO:
                session_window = new client::QtSystemInfoWindow();
                break;

            case proto::SESSION_TYPE_TEXT_CHAT:
                session_window = new client::QtTextChatWindow();
                break;

            default:
                NOTREACHED();
                break;
        }

        if (!session_window)
            return 1;

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
