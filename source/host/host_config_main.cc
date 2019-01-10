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

#include "host/host_config_main.h"

#include <QCommandLineParser>
#include <QMessageBox>

#include "base/logging.h"
#include "build/build_config.h"
#include "build/version.h"
#include "crypto/scoped_crypto_initializer.h"
#include "host/ui/host_config_dialog.h"
#include "host/host_settings.h"
#include "updater/update_dialog.h"

using namespace aspia;

int hostConfigMain(int argc, char *argv[])
{
    Q_INIT_RESOURCE(common);
    Q_INIT_RESOURCE(common_translations);
    Q_INIT_RESOURCE(updater);
    Q_INIT_RESOURCE(updater_translations);

    LoggingSettings settings;
    settings.logging_dest = LOG_TO_ALL;

    ScopedLogging logging(settings);

    ScopedCryptoInitializer crypto_initializer;
    CHECK(crypto_initializer.isSucceeded());

    QApplication application(argc, argv);
    application.setOrganizationName("Aspia");
    application.setApplicationName("Host");
    application.setApplicationVersion(ASPIA_VERSION_STRING);
    application.setAttribute(Qt::AA_DisableWindowContextHelpButton, true);

    HostSettings host_settings;
    LocaleLoader locale_loader;

    QString current_locale = host_settings.locale();
    if (!locale_loader.contains(current_locale))
        host_settings.setLocale(DEFAULT_LOCALE);

    locale_loader.installTranslators(current_locale);

    QCommandLineOption import_option(
        "import", QApplication::translate("HostConfig", "The path to the file to import."), "file");

    QCommandLineOption export_option(
        "export", QApplication::translate("HostConfig", "The path to the file to export."), "file");

    QCommandLineOption silent_option("silent",
        QApplication::translate("HostConfig", "Enables silent mode when exporting and importing."));

    QCommandLineOption update_option("update",
        QApplication::translate("HostConfig", "Run application update."));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(import_option);
    parser.addOption(export_option);
    parser.addOption(silent_option);
    parser.addOption(update_option);
    parser.process(application);

    if (parser.isSet(import_option) && parser.isSet(export_option))
    {
        if (!parser.isSet(silent_option))
        {
            QMessageBox::warning(
                nullptr,
                QApplication::translate("HostConfig", "Warning"),
                QApplication::translate("HostConfig", "Export and import parameters can not be specified together."),
                QMessageBox::Ok);
        }

        return 1;
    }
    else if (parser.isSet(import_option))
    {
        if (!HostConfigDialog::importSettings(parser.value(import_option), parser.isSet(silent_option)))
            return 1;
    }
    else if (parser.isSet(export_option))
    {
        if (!HostConfigDialog::exportSettings(parser.value(export_option), parser.isSet(silent_option)))
            return 1;
    }
    else if (parser.isSet(update_option))
    {
        UpdateDialog dialog(host_settings.updateServer(), QLatin1String("host"));
        dialog.show();
        dialog.activateWindow();

        return application.exec();
    }
    else
    {
        HostConfigDialog dialog(locale_loader);
        dialog.show();
        dialog.activateWindow();

        return application.exec();
    }

    return 0;
}
