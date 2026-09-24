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

#include <QSysInfo>

#include "version.h"
#include "base/logging.h"
#include "base/sys_info.h"
#include "client/application.h"

#if defined(Q_OS_MACOS)
#include "base/process_util.h"
#endif // defined(Q_OS_MACOS)

#if defined(Q_OS_ANDROID)
#include <QCoreApplication>
#include <QJniObject>

#include "client/android/main_window.h"
#else
#include <QCommandLineParser>
#include <QDir>
#include <QTimer>

#include "base/crypto/secure_string.h"
#include "client/database.h"
#include "client/host_url.h"
#include "client/master_password.h"
#include "client/desktop/main_window.h"
#include "common/desktop/credentials_dialog.h"
#include "common/desktop/msg_box.h"
#include "common/desktop/update_dialog.h"
#endif // defined(Q_OS_ANDROID)

#if !defined(Q_OS_ANDROID)
namespace {

//--------------------------------------------------------------------------------------------------
void backupOnStartup()
{
    Database& db = Database::instance();
    if (!db.isBackupOnStartupEnabled())
        return;

    const QString directory = db.backupPath();

    switch (AutoBackup::run(db, directory, db.backupRetention()))
    {
        case Backup::Result::SUCCESS:
        case Backup::Result::NOTHING_EXPORTED:
            break;

        case Backup::Result::FILE_ERROR:
            MsgBox::warning(nullptr, QApplication::translate(
                "Client", "Unable to create a backup in the directory \"%1\".")
                    .arg(QDir::toNativeSeparators(directory)));
            break;

        case Backup::Result::UNREADABLE_RECORD:
            MsgBox::warning(nullptr, QApplication::translate(
                "Client", "Unable to create a backup. Some records of the database could not be read. "
                          "Edit them to enter their data again."));
            break;

        default:
            MsgBox::warning(nullptr, QApplication::translate("Client", "Unable to create a backup."));
            break;
    }
}

} // namespace
#endif // !defined(Q_OS_ANDROID)

//--------------------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
#if defined(Q_OS_ANDROID)
    // When the Qt event loop ends, Qt's Android platform calls exit(), which runs the C++ runtime's
    // global destructors (__cxa_finalize). Those tear down std::thread/std::mutex statics while
    // Android's own threads (notably the HWUI render threads "hwuiTask") are still running, so a
    // render thread locks an already-destroyed mutex and the process aborts with
    // "FORTIFY: pthread_mutex_lock called on a destroyed mutex" (SIGABRT). This is a long-standing
    // Qt-on-Android shutdown race. QT_ANDROID_NO_EXIT_CALL makes Qt skip that exit() call, so the
    // global destructors are not run and the race cannot occur (Android reclaims the process).
    // Refs:
    //   https://bugreports.qt.io/browse/QTBUG-85449
    //   https://bugreports.qt.io/browse/QTBUG-82617
    //   https://developernote.com/2022/03/crash-at-std-thread-and-std-mutex-destructors-on-android/
    qputenv("QT_ANDROID_NO_EXIT_CALL", "1");

    // Force the software (raster) widget backing store. The app uses no OpenGL widgets, so the RHI
    // compositor is only pulled in for translucent top-level surfaces (dialogs, bottom sheets, menus).
    // Its QOpenGLContext::makeCurrent aborts with a qFatal (SIGABRT) when the surface is momentarily
    // invalid - e.g. while the on-screen keyboard is shown during a repaint. Raster avoids OpenGL
    // entirely and removes that crash.
    qputenv("QT_WIDGETS_RHI", "0");
#endif // defined(Q_OS_ANDROID)

    Q_INIT_RESOURCE(common);
    Q_INIT_RESOURCE(common_translations);

    LoggingSettings logging_settings;
    logging_settings.min_log_level = LOG_INFO;

#if defined(Q_OS_ANDROID)
    // Android defaults to logcat only (LOG_TO_STDOUT). Also write a log file so it can be pulled from
    // devices whose logcat is restricted (e.g. Huawei), then point that file at the external app files
    // dir (/sdcard/Android/data/<package>/files/log) so adb can pull it without root.
    logging_settings.destination = LOG_TO_ALL;

    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (context.isValid())
    {
        QJniObject files_dir = context.callObjectMethod(
            "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;", nullptr);
        if (files_dir.isValid())
        {
            logging_settings.log_dir =
                files_dir.callObjectMethod<jstring>("getAbsolutePath").toString() + "/log";
        }
    }
#endif // defined(Q_OS_ANDROID)

    ScopedLogging scoped_logging(logging_settings);

    Application::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

#if defined(Q_OS_MACOS)
    // The update is installed by an instance launched through Authorization Services, and without the
    // identity of root the installation is refused.
    ProcessUtil::restartAsRoot(argv);
#endif // defined(Q_OS_MACOS)

#if !defined(Q_OS_ANDROID)
    if (argc == 3)
    {
        auto option_value = [](const char* argument, const char* option)
        {
            size_t length = qstrlen(option);
            if (qstrncmp(argument, option, length) != 0)
                return QString();
            return QString::fromLocal8Bit(argument + length);
        };

        QString channel = option_value(argv[1], "--channel=");
        QString locale = option_value(argv[2], "--locale=");

        if (!channel.isEmpty() && !locale.isEmpty())
        {
#if defined(Q_OS_LINUX)
            qputenv("QT_QPA_PLATFORM", "xcb");
#endif // defined(Q_OS_LINUX)

            GuiApplication application(argc, argv);
            application.setLocale(locale);

            UpdateDialog dialog(channel, "client", UpdateDialog::Action::INSTALL);

            return dialog.exec() == QDialog::Accepted ?
                UpdateDialog::kInstalledExitCode : UpdateDialog::kClosedExitCode;
        }
    }
#endif // !defined(Q_OS_ANDROID)

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

#if defined(Q_OS_ANDROID)
    AndroidMainWindow main_window;
    main_window.show();

    return application.exec();
#else
    QCommandLineParser parser;
    parser.setApplicationDescription(QApplication::translate("Client", "Aspia Client"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("url",
        QApplication::translate("Client", "An aspia:// link to connect to a host."), "[url]");
    parser.process(application);

    QString start_url;
    QStringList positional_arguments = parser.positionalArguments();
    if (!positional_arguments.isEmpty() && HostUrl::isHostUrl(positional_arguments.front()))
        start_url = positional_arguments.front();

    // A link can arrive before the main window exists: forwarded by a second instance or
    // delivered by a URL open event while the master password dialog is still on the screen.
    // Remember it and open it after the main window is created.
    QMetaObject::Connection pending_url_connection = QObject::connect(
        &application, &Application::sig_urlOpened, &application, [&start_url](const QString& url)
    {
        start_url = url;
    });

    if (application.isRunning())
    {
        if (!start_url.isEmpty())
        {
            LOG(INFO) << "Another instance is already running, forwarding URL to it";
            application.openUrl(start_url);
        }
        else
        {
            LOG(INFO) << "Another instance is already running, activating its window";
            application.activateWindow();
        }
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

            const MasterPassword::Result unlocked = MasterPassword::unlock(dialog.password());
            if (unlocked == MasterPassword::Result::SUCCESS)
            {
                LOG(INFO) << "Master password accepted";
                break;
            }

            if (unlocked != MasterPassword::Result::INVALID_PASSWORD)
            {
                LOG(ERROR) << "Unable to unlock the database";
                MsgBox::warning(nullptr, QApplication::translate(
                    "Client", "Unable to unlock the database."));
                return 1;
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

            if (new_password.size() < MasterPassword::kMinPasswordLength)
            {
                MsgBox::warning(d, QApplication::translate(
                    "Client", "The password can not be shorter than %n characters.",
                    "", MasterPassword::kMinPasswordLength));
                return false;
            }

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

            if (MasterPassword::setNew(new_password) != MasterPassword::Result::SUCCESS)
            {
                MsgBox::warning(d, QApplication::translate("Client", "Unable to set master password."));
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

    backupOnStartup();

    std::unique_ptr<MainWindow> main_window = std::make_unique<MainWindow>();

    QObject::disconnect(pending_url_connection);

    QObject::connect(&application, &Application::sig_windowActivated,
                     main_window.get(), &MainWindow::showAndActivate);
    QObject::connect(&application, &Application::sig_urlOpened,
                     main_window.get(), &MainWindow::connectToUrl);

    main_window->show();
    main_window->activateWindow();

    if (!start_url.isEmpty())
    {
        // Start the connection once the event loop is up and the window is shown.
        QTimer::singleShot(MilliSeconds(0), main_window.get(), [window = main_window.get(), start_url]()
        {
            window->connectToUrl(start_url);
        });
    }

    return application.exec();
#endif // defined(Q_OS_ANDROID)
}
