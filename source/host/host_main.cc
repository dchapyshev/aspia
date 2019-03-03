//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/host_main.h"

#include <QAbstractEventDispatcher>
#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>

#include "base/base_paths.h"
#include "base/qt_logging.h"
#include "base/win/scoped_thread_desktop.h"
#include "base/win/process_util.h"
#include "build/version.h"
#include "crypto/scoped_crypto_initializer.h"
#include "host/ui/host_main_window.h"
#include "host/host_settings.h"
#include "host/host_single_application.h"
#include "updater/update_dialog.h"

namespace {

class EventFilter : public QAbstractNativeEventFilter
{
public:
    ~EventFilter() = default;

    static EventFilter* instance();

    // QAbstractNativeEventFilter implementation.
    bool nativeEventFilter(const QByteArray& event_type, void* message, long* result) override;

private:
    EventFilter() = default;

    DISALLOW_COPY_AND_ASSIGN(EventFilter);
};

// static
EventFilter* EventFilter::instance()
{
    static EventFilter event_filter;
    return &event_filter;
}

bool EventFilter::nativeEventFilter(const QByteArray& event_type, void* message, long* result)
{
    MSG* native_message = reinterpret_cast<MSG*>(message);

    if (native_message->message == WM_QUERYENDSESSION || native_message->message == WM_ENDSESSION)
    {
        *result = TRUE;
        return true;
    }

    return false;
}

std::filesystem::path loggingDir()
{
    std::filesystem::path path;

    if (base::win::isProcessElevated())
    {
        if (!base::BasePaths::commonAppData(&path))
            return std::filesystem::path();
    }
    else
    {
        if (!base::BasePaths::userAppData(&path))
            return std::filesystem::path();
    }

    path.append("aspia/logs");
    return path;
}

bool waitForValidInputDesktop()
{
    int max_attempt_count = 600;

    do
    {
        base::Desktop input_desktop(base::Desktop::inputDesktop());
        if (input_desktop.isValid())
        {
            if (input_desktop.setThreadDesktop())
                break;
        }

        Sleep(100);
    }
    while (--max_attempt_count > 0);

    if (max_attempt_count == 0)
    {
        LOG(LS_WARNING) << "Exceeded the number of attempts";
        return false;
    }

    return true;
}

int runApplication(int argc, char* argv[])
{
    bool is_started_from_service = base::win::isProcessStartedFromService();

    if (!is_started_from_service)
    {
        if (!base::win::isProcessElevated())
        {
            QStringList arguments;
            QString program;

            for (int i = 0; i < argc; ++i)
            {
                QString argument = QString::fromLocal8Bit(argv[i]);

                if (i == 0)
                    program = argument;
                else
                    arguments.append(argument);
            }

            if (base::win::executeProcess(program, arguments,
                                          base::win::ProcessExecuteMode::ELEVATE))
            {
                return 0;
            }
        }
    }
    else
    {
        if (!waitForValidInputDesktop())
            return 1;
    }

    host::SingleApplication application(argc, argv);
    application.setOrganizationName(QStringLiteral("Aspia"));
    application.setApplicationName(QStringLiteral("Host"));
    application.setApplicationVersion(QStringLiteral(ASPIA_VERSION_STRING));
    application.setAttribute(Qt::AA_DisableWindowContextHelpButton, true);

    host::Settings host_settings;
    common::LocaleLoader locale_loader;

    QString current_locale = host_settings.locale();
    if (!locale_loader.contains(current_locale))
        host_settings.setLocale(QStringLiteral(DEFAULT_LOCALE));

    locale_loader.installTranslators(current_locale);

    QCommandLineOption import_option(QStringLiteral("import"),
        QApplication::translate("Host", "The path to the file to import."),
        QStringLiteral("file"));

    QCommandLineOption export_option(QStringLiteral("export"),
        QApplication::translate("Host", "The path to the file to export."),
        QStringLiteral("file"));

    QCommandLineOption silent_option(QStringLiteral("silent"),
        QApplication::translate("Host", "Enables silent mode when exporting and importing."));

    QCommandLineOption update_option(QStringLiteral("update"),
        QApplication::translate("Host", "Run application update."));

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
                QApplication::translate("Host", "Warning"),
                QApplication::translate("Host", "Export and import parameters can not be specified together."),
                QMessageBox::Ok);
        }

        return 1;
    }
    else if (parser.isSet(import_option))
    {
        if (!host::Settings::importFromFile(parser.value(import_option), parser.isSet(silent_option)))
            return 1;
    }
    else if (parser.isSet(export_option))
    {
        if (!host::Settings::exportToFile(parser.value(export_option), parser.isSet(silent_option)))
            return 1;
    }
    else if (parser.isSet(update_option))
    {
        updater::UpdateDialog dialog(host_settings.updateServer(), QLatin1String("host"));
        dialog.show();
        dialog.activateWindow();

        return application.exec();
    }
    else
    {
        if (application.isRunning())
        {
            application.activate();
        }
        else
        {
            QAbstractEventDispatcher::instance()->installNativeEventFilter(
                EventFilter::instance());

            host::MainWindow window(host_settings, locale_loader);

            QObject::connect(&application, &host::SingleApplication::activated,
                             &window, &host::MainWindow::activateHost);

            if (is_started_from_service)
            {
                window.hideToTray();
            }
            else
            {
                window.show();
                window.activateWindow();
            }

            window.connectToService();

            return application.exec();
        }
    }

    return 0;
}

} // namespace

int hostMain(int argc, char* argv[])
{
    Q_INIT_RESOURCE(qt_translations);
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

    int result = runApplication(argc, argv);

    base::shutdownLogging();
    return result;
}
