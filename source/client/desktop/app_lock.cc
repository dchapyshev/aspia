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

#include "client/desktop/app_lock.h"

#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QThread>
#include <QTimer>

#include "base/auto_qpointer.h"
#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "client/application.h"
#include "client/database.h"
#include "client/master_password.h"
#include "client/settings.h"
#include "client/desktop/main_window.h"
#include "common/desktop/credentials_dialog.h"
#include "common/desktop/elevate_util.h"
#include "common/desktop/msg_box.h"

namespace {

// The lock comes at most this much later than the timeout.
constexpr Seconds kCheckInterval{ 5 };

} // namespace

//--------------------------------------------------------------------------------------------------
AppLock::AppLock(MainWindow* main_window)
    : QObject(main_window),
      main_window_(main_window),
      timer_(new QTimer(this))
{
    timer_->setInterval(kCheckInterval);
    connect(timer_, &QTimer::timeout, this, &AppLock::onCheck);
}

//--------------------------------------------------------------------------------------------------
AppLock::~AppLock() = default;

//--------------------------------------------------------------------------------------------------
// static
AppLock::Result AppLock::unlock(bool reset_allowed)
{
    const int kResetResult = QDialog::Accepted + 1;

    while (true)
    {
        AutoQPointer<CredentialsDialog> dialog(
            new CredentialsDialog(CredentialsDialog::Type::ENTER_PASSWORD, nullptr));
        dialog->setObjectName("UnlockDialog");
        dialog->setWindowTitle(tr("Unlock"));
        dialog->setHeaderIcon(":/img/lock.svg");
        dialog->setHeaderText(tr("Enter the master password to unlock the application."));
        dialog->setShowPasswordButtonVisible(true);
        if (reset_allowed)
            dialog->setLink(tr("Forgot password?"), kResetResult);

        Settings settings;
        dialog->restoreGeometry(settings.dialogGeometry(dialog->objectName()));

        // A second start of the application activates its window, and until the unlock that window
        // is this dialog.
        connect(Application::instance(), &Application::sig_windowActivated, dialog.get(), [window = dialog.get()]()
        {
            window->raise();
            window->activateWindow();
        });

        const int result = dialog->exec();
        settings.setDialogGeometry(dialog->objectName(), dialog->saveGeometry());

        if (result == kResetResult)
        {
            const QFileInfo file_info(Database::filePath());
            const QString copy_path = QString("%1/%2-%3.%4").arg(file_info.path(), file_info.completeBaseName(),
                QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss"), file_info.suffix());

            const QString question = tr(
                "The master password cannot be recovered. To continue using the application, you need to reset "
                "the database. All records, the master password and the settings will be deleted, but a copy of "
                "the current database will first be saved to the file \"%1\".\nReset the database?")
                    .arg(QDir::toNativeSeparators(copy_path));

            if (MsgBox::importantQuestion(nullptr, question, Seconds(30)) != MsgBox::Yes)
                continue;

            if (!Database::instance().saveCopyAndClearAll(copy_path))
            {
                MsgBox::warning(nullptr, tr("Unable to reset the database."));
                continue;
            }

            LOG(INFO) << "Database reset by user";
            MsgBox::information(nullptr, tr("The database is reset. The previous database is saved to the file \"%1\".")
                .arg(QDir::toNativeSeparators(copy_path)));
            return Result::RESET;
        }

        if (result != QDialog::Accepted)
        {
            LOG(INFO) << "Master password unlock cancelled by user";
            return Result::CANCELLED;
        }

        const MasterPassword::Result unlocked = MasterPassword::unlock(dialog->password());
        if (unlocked == MasterPassword::Result::SUCCESS)
        {
            LOG(INFO) << "Master password accepted";
            return Result::UNLOCKED;
        }

        if (unlocked != MasterPassword::Result::INVALID_PASSWORD)
        {
            LOG(ERROR) << "Unable to unlock the database";
            MsgBox::warning(nullptr, tr("Unable to unlock the database."));
            return Result::FAILED;
        }

        MsgBox::warning(nullptr, tr("Invalid master password."));
    }
}

//--------------------------------------------------------------------------------------------------
// static
bool AppLock::hasOpenDialogs()
{
    // exec() of a dialog, a message box or a menu runs a nested event loop.
    if (QThread::currentThread()->loopLevel() > 1)
        return true;

    if (QApplication::activePopupWidget())
        return true;

    const QWidgetList widgets = QApplication::topLevelWidgets();
    for (const QWidget* widget : widgets)
    {
        if (widget->isVisible() && qobject_cast<const QDialog*>(widget))
            return true;

        // A native file dialog is invisible to Qt and runs its own loop of the system, where the
        // timers of Qt keep firing.
        if (qobject_cast<const QFileDialog*>(widget))
            return true;

        if (widget->findChild<ElevateUtil*>())
            return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
void AppLock::setTimeout(Minutes timeout)
{
    LOG(INFO) << "Lock timeout:" << timeout.count() << "min";

    timeout_ = timeout;
    last_input_ = Clock::now();

    if (timeout_ > Minutes::zero())
    {
        QApplication::instance()->installEventFilter(this);
        timer_->start();
    }
    else
    {
        QApplication::instance()->removeEventFilter(this);
        timer_->stop();
    }
}

//--------------------------------------------------------------------------------------------------
bool AppLock::eventFilter(QObject* watched, QEvent* event)
{
    switch (event->type())
    {
        case QEvent::KeyPress:
        case QEvent::MouseButtonPress:
        case QEvent::MouseMove:
        case QEvent::Wheel:
        case QEvent::TouchBegin:
            last_input_ = Clock::now();
            break;

        default:
            break;
    }

    return QObject::eventFilter(watched, event);
}

//--------------------------------------------------------------------------------------------------
void AppLock::onCheck()
{
    // The time without input counts from the moment the application could be locked, otherwise it
    // would lock right after the user finished with a session or a dialog.
    if (!canLock())
    {
        last_input_ = Clock::now();
        return;
    }

    if (Clock::now() - last_input_ < timeout_)
        return;

    LOG(INFO) << "No input for" << timeout_.count() << "min";
    timer_->stop();
    emit sig_lockRequested();
}

//--------------------------------------------------------------------------------------------------
bool AppLock::canLock() const
{
    return main_window_ && !main_window_->hasSessions() && !hasOpenDialogs();
}
