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
#include <QSysInfo>

#include "base/logging.h"
#include "base/sys_info.h"
#include "base/crypto/secure_string.h"
#include "build/version.h"
#include "client/master_password.h"
#include "common/ui/msg_box.h"
#include "common/ui/credentials_dialog.h"
#include "client/desktop/application.h"
#include "client/desktop/main_window.h"

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

    QCommandLineParser parser;
    parser.setApplicationDescription(QApplication::translate("Client", "Aspia Client"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(application);

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
            CredentialsDialog dialog(CredentialsDialog::Type::ENTER_PASSWORD, nullptr);
            dialog.setWindowTitle(QApplication::translate("Client", "Unlock"));
            dialog.setHeaderIcon(":/img/lock.svg");
            dialog.setHeaderText(QApplication::translate(
                "Client", "Enter the master password to unlock the application."));
            dialog.setShowPasswordButtonVisible(true);

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

        CredentialsDialog dialog(CredentialsDialog::Type::SET_PASSWORD, nullptr);
        dialog.setWindowTitle(QApplication::translate("Client", "Set Master Password"));
        dialog.setHeaderIcon(":/img/lock.svg");
        dialog.setHeaderText(QApplication::translate(
            "Client", "Set a master password required to unlock the application."));
        dialog.setValidator([](CredentialsDialog* d) -> bool
        {
            SecureString new_password = d->password();

            if (!MasterPassword::isSafePassword(new_password))
            {
                QString unsafe = QApplication::translate(
                    "Client", "Password you entered does not meet the security requirements!");
                QString safe = QApplication::translate("Client",
                    "The password must contain lowercase and uppercase characters, "
                    "numbers and should not be shorter than %n characters.",
                    "", MasterPassword::kSafePasswordLength);
                QString question = QApplication::translate(
                    "Client", "Do you want to enter a different password?");

                if (MsgBox::warning(d, QString("<b>%1</b><br/>%2<br/>%3").arg(unsafe, safe, question),
                                    MsgBox::Yes | MsgBox::No) == MsgBox::Yes)
                    return false;
            }

            if (!MasterPassword::setNew(new_password))
            {
                MsgBox::warning(d, QApplication::translate(
                    "Client", "Unable to set master password."));
                return false;
            }
            return true;
        });

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
