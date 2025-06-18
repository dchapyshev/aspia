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

//--------------------------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(client);
    Q_INIT_RESOURCE(common);

    base::LoggingSettings logging_settings;
    logging_settings.min_log_level = base::LOG_INFO;

    base::ScopedLogging scoped_logging(logging_settings);

    console::Application::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    console::Application application(argc, argv);

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
        LOG(INFO) << "Application already running";

        if (file_path.isEmpty())
        {
            LOG(INFO) << "File path is empty. Activate window";
            application.activateWindow();
        }
        else
        {
            LOG(INFO) << "Open file:" << file_path;
            application.openFile(file_path);
        }

        return 0;
    }
    else
    {
        LOG(INFO) << "Application not running yet";

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
