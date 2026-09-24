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

#ifndef CLIENT_DESKTOP_APP_LOCK_H
#define CLIENT_DESKTOP_APP_LOCK_H

#include <QObject>
#include <QPointer>

#include "base/time_types.h"

class MainWindow;
class QTimer;

// Asks for the lock of the application when the user has not touched it for the time set in the
// settings.
class AppLock final : public QObject
{
    Q_OBJECT

public:
    enum class Result { UNLOCKED, CANCELLED, FAILED };

    explicit AppLock(MainWindow* main_window);
    ~AppLock() final;

    // Asks the master password until the user enters the right one or gives up.
    static Result unlock();

    // A dialog, a message box, a menu or a file dialog is on the screen. The window can not be
    // destroyed under them.
    static bool hasOpenDialogs();

    // A zero |timeout| turns the lock off.
    void setTimeout(Minutes timeout);

signals:
    void sig_lockRequested();

protected:
    // QObject implementation.
    bool eventFilter(QObject* watched, QEvent* event) final;

private slots:
    void onCheck();

private:
    bool canLock() const;

    QPointer<MainWindow> main_window_;
    QTimer* timer_ = nullptr;
    Minutes timeout_ = Minutes::zero();
    TimePoint last_input_;

    Q_DISABLE_COPY_MOVE(AppLock)
};

#endif // CLIENT_DESKTOP_APP_LOCK_H
