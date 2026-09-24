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

#include <QEvent>
#include <QFileDialog>
#include <QThread>
#include <QTimer>

#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "client/application.h"
#include "client/master_password.h"
#include "client/desktop/main_window.h"
#include "common/desktop/credentials_dialog.h"
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
AppLock::Result AppLock::unlock()
{
    while (true)
    {
        CredentialsDialog dialog(CredentialsDialog::Type::ENTER_PASSWORD, nullptr);
        dialog.setWindowTitle(QApplication::translate("Client", "Unlock"));
        dialog.setHeaderIcon(":/img/lock.svg");
        dialog.setHeaderText(QApplication::translate(
            "Client", "Enter the master password to unlock the application."));
        dialog.setShowPasswordButtonVisible(true);

        // A second start of the application activates its window, and until the unlock that window
        // is this dialog.
        connect(Application::instance(), &Application::sig_windowActivated, &dialog, [&dialog]()
        {
            dialog.raise();
            dialog.activateWindow();
        });

        if (dialog.exec() != QDialog::Accepted)
        {
            LOG(INFO) << "Master password unlock cancelled by user";
            return Result::CANCELLED;
        }

        const MasterPassword::Result unlocked = MasterPassword::unlock(dialog.password());
        if (unlocked == MasterPassword::Result::SUCCESS)
        {
            LOG(INFO) << "Master password accepted";
            return Result::UNLOCKED;
        }

        if (unlocked != MasterPassword::Result::INVALID_PASSWORD)
        {
            LOG(ERROR) << "Unable to unlock the database";
            MsgBox::warning(nullptr, QApplication::translate(
                "Client", "Unable to unlock the database."));
            return Result::FAILED;
        }

        MsgBox::warning(nullptr, QApplication::translate("Client", "Invalid master password."));
    }
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
    if (!main_window_ || main_window_->hasSessions())
        return false;

    // exec() of a dialog, a message box or a menu runs a nested event loop.
    if (QThread::currentThread()->loopLevel() > 1)
        return false;

    if (QApplication::activePopupWidget())
        return false;

    const QWidgetList widgets = QApplication::topLevelWidgets();
    for (const QWidget* widget : widgets)
    {
        if (widget->isVisible() && qobject_cast<const QDialog*>(widget))
            return false;

        // A native file dialog is invisible to Qt and runs its own loop of the system, where the
        // timers of Qt keep firing.
        if (qobject_cast<const QFileDialog*>(widget))
            return false;
    }

    return true;
}
