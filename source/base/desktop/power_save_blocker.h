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

#ifndef BASE_DESKTOP_POWER_SAVE_BLOCKER_H
#define BASE_DESKTOP_POWER_SAVE_BLOCKER_H

#include <QtClassHelperMacros>
#include <QtSystemDetection>
#include <QtTypes>

#if defined(Q_OS_WINDOWS)
#include "base/win/scoped_object.h"
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#include <QDBusConnection>
#endif // defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)

class PowerSaveBlocker
{
public:
    PowerSaveBlocker();
    ~PowerSaveBlocker();

private:
#if defined(Q_OS_WINDOWS)
    ScopedHandle handle_;
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    // Screen-saver inhibit on the active session user's bus (the root agent reaches it via SessionDBus).
    QDBusConnection bus_;
    quint32 cookie_ = 0;
#endif // defined(Q_OS_*)

    Q_DISABLE_COPY_MOVE(PowerSaveBlocker)
};

#endif // BASE_DESKTOP_WIN_POWER_SAVE_BLOCKER_H
