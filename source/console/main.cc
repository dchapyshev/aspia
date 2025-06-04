//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include <QSysInfo>

#include "base/logging.h"
#include "base/sys_info.h"
#include "build/version.h"
#include "console/application.h"
#include "console/main_window.h"
#include "proto/meta_types.h"

//--------------------------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
#if !defined(I18L_DISABLED)
    Q_INIT_RESOURCE(client);
    Q_INIT_RESOURCE(client_translations);
    Q_INIT_RESOURCE(common);
    Q_INIT_RESOURCE(common_translations);
#endif

    base::LoggingSettings logging_settings;
    logging_settings.min_log_level = base::LOG_LS_INFO;

    base::ScopedLogging scoped_logging(logging_settings);

    proto::registerMetaTypes();

    console::Application::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    console::Application::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
    console::Application::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    console::Application application(argc, argv);

    LOG(LS_INFO) << "Version: " << ASPIA_VERSION_STRING
                 << " (arch: " << QSysInfo::buildCpuArchitecture() << ")";
#if defined(GIT_CURRENT_BRANCH) && defined(GIT_COMMIT_HASH)
    LOG(LS_INFO) << "Git branch: " << GIT_CURRENT_BRANCH;
    LOG(LS_INFO) << "Git commit: " << GIT_COMMIT_HASH;
#endif

    LOG(LS_INFO) << "OS: " << base::SysInfo::operatingSystemName()
                 << " (version: " << base::SysInfo::operatingSystemVersion()
                 <<  " arch: " << base::SysInfo::operatingSystemArchitecture() << ")";
    LOG(LS_INFO) << "Qt version: " << QT_VERSION_STR;
    LOG(LS_INFO) << "Command line: " << application.arguments();

    QCommandLineParser parser;
    parser.setApplicationDescription(QApplication::translate("Console", "Aspia Console"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QApplication::translate("Console", "file"),
        QApplication::translate("Console", "The file to open."));
    parser.process(application);

    std::unique_ptr<console::MainWindow> console_window;
    QStringList arguments = parser.positionalArguments();

    QString file_path;
    if (!arguments.isEmpty())
        file_path = arguments.front();

    if (application.isRunning())
    {
        LOG(LS_INFO) << "Application already running";

        if (file_path.isEmpty())
        {
            LOG(LS_INFO) << "File path is empty. Activate window";
            application.activateWindow();
        }
        else
        {
            LOG(LS_INFO) << "Open file: " << file_path;
            application.openFile(file_path);
        }

        return 0;
    }
    else
    {
        LOG(LS_INFO) << "Application not running yet";

        console_window.reset(new console::MainWindow(file_path));

        QObject::connect(&application, &console::Application::sig_windowActivated,
            console_window.get(), &console::MainWindow::showConsole);

        QObject::connect(&application, &console::Application::sig_fileOpened,
            console_window.get(), &console::MainWindow::openAddressBook);

        console_window->show();
        console_window->activateWindow();
    }

    return application.exec();
}
