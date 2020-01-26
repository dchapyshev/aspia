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

#include "base/command_line.h"
#include "base/logging.h"
#include "base/win/process_util.h"
#include "base/win/scoped_thread_desktop.h"
#include "host/system_settings.h"
#include "host/ui/host_application.h"
#include "host/ui/host_main_window.h"
#include "host/ui/settings_util.h"
#include "updater/update_dialog.h"

#include <QMessageBox>

namespace {

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

} // namespace

int hostMain(int argc, char* argv[])
{
    Q_INIT_RESOURCE(qt_translations);
    Q_INIT_RESOURCE(common);
    Q_INIT_RESOURCE(common_translations);
    Q_INIT_RESOURCE(updater);
    Q_INIT_RESOURCE(updater_translations);

    base::CommandLine command_line(argc, argv);

    bool is_hidden = command_line.hasSwitch(u"hidden");
    if (!is_hidden)
    {
        if (!base::win::isProcessElevated())
        {
            if (base::win::createProcess(command_line, base::win::ProcessExecuteMode::ELEVATE))
                return 0;
        }
    }
    else
    {
        if (!waitForValidInputDesktop())
            return 1;
    }

    host::Application application(argc, argv);

    if (command_line.hasSwitch(u"import") && command_line.hasSwitch(u"export"))
    {
        if (!command_line.hasSwitch(u"silent"))
        {
            QMessageBox::warning(
                nullptr,
                QApplication::translate("Host", "Warning"),
                QApplication::translate("Host", "Export and import parameters can not be specified together."),
                QMessageBox::Ok);
        }

        return 1;
    }
    else if (command_line.hasSwitch(u"import"))
    {
        if (!host::SettingsUtil::importFromFile(command_line.switchValuePath(u"import"),
                                                command_line.hasSwitch(u"silent")))
        {
            return 1;
        }
    }
    else if (command_line.hasSwitch(u"export"))
    {
        if (!host::SettingsUtil::exportToFile(command_line.switchValuePath(u"export"),
                                              command_line.hasSwitch(u"silent")))
        {
            return 1;
        }
    }
    else if (command_line.hasSwitch(u"update"))
    {
        updater::UpdateDialog dialog(
            QString::fromStdString(host::SystemSettings().updateServer()), "host");
        dialog.show();
        dialog.activateWindow();

        return application.exec();
    }
    else if (command_line.hasSwitch(u"help"))
    {
        // TODO
    }
    else
    {
        if (application.isRunning())
        {
            application.activate();
        }
        else
        {
            host::MainWindow window;

            QObject::connect(&application, &host::Application::activated,
                             &window, &host::MainWindow::activateHost);

            if (is_hidden)
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
