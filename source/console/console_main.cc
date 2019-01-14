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

#include <QCommandLineParser>

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

#include "base/logging.h"
#include "build/version.h"
#include "client/ui/client_window.h"
#include "console/console_window.h"
#include "crypto/scoped_crypto_initializer.h"

using namespace aspia;

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(client);
    Q_INIT_RESOURCE(client_translations);
    Q_INIT_RESOURCE(common);
    Q_INIT_RESOURCE(common_translations);
    Q_INIT_RESOURCE(updater);
    Q_INIT_RESOURCE(updater_translations);

    base::LoggingSettings logging_settings;
    logging_settings.logging_dest = base::LOG_TO_ALL;

    base::ScopedLogging logging(logging_settings);

    crypto::ScopedCryptoInitializer crypto_initializer;
    CHECK(crypto_initializer.isSucceeded());

    QApplication application(argc, argv);

    ConsoleSettings console_settings;
    QString current_locale = console_settings.locale();

    common::LocaleLoader locale_loader;
    if (!locale_loader.contains(current_locale))
        console_settings.setLocale(DEFAULT_LOCALE);

    locale_loader.installTranslators(current_locale);

    application.setOrganizationName(QStringLiteral("Aspia"));
    application.setApplicationName(QApplication::translate("Console", "Console"));
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

    QCommandLineOption simple_ui_option(
        QStringLiteral("simple-ui"),
        QApplication::translate("Console", "Run the program with a simplified user interface."));

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
    parser.addOption(simple_ui_option);
    parser.process(application);

    QScopedPointer<ConsoleWindow> console_window;

    if (parser.isSet(simple_ui_option))
    {
        if (!ClientWindow::connectToHost())
            return 0;
    }
    else if (parser.isSet(address_option))
    {
        ConnectData connect_data;
        connect_data.address  = parser.value(address_option);
        connect_data.port     = parser.value(port_option).toUShort();
        connect_data.username = parser.value(username_option);

        QString session_type = parser.value(session_type_option);

        if (session_type == "desktop-manage")
            connect_data.session_type = proto::SESSION_TYPE_DESKTOP_MANAGE;
        else if (session_type == "desktop-view")
            connect_data.session_type = proto::SESSION_TYPE_DESKTOP_VIEW;
        else if (session_type == "file-transfer")
            connect_data.session_type = proto::SESSION_TYPE_FILE_TRANSFER;

        if (!ClientWindow::connectToHost(&connect_data))
            return 0;
    }
    else
    {
        QStringList arguments = parser.positionalArguments();

        QString file_path;
        if (!arguments.isEmpty())
            file_path = arguments.front();

        console_window.reset(new ConsoleWindow(console_settings, locale_loader, file_path));
        console_window->show();
        console_window->activateWindow();
    }

    return application.exec();
}
