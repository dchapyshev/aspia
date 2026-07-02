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

#ifndef BASE_POWER_SAVE_BLOCKER_H
#define BASE_POWER_SAVE_BLOCKER_H

#include <QObject>

#if defined(Q_OS_WINDOWS)
#include "base/win/scoped_object.h"
#endif // defined(Q_OS_WINDOWS)

class QTimer;

// Keeps the display and system awake for the lifetime of the instance, so a remote session is not
// interrupted by the screen blanking (which on Wayland breaks the screen-cast stream and leaves KMS
// with no scan-out to read).
class PowerSaveBlocker final : public QObject
{
    Q_OBJECT

public:
    explicit PowerSaveBlocker(QObject* parent = nullptr);
    ~PowerSaveBlocker() final;

private:
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    // Feeds a net-zero nudge through the wake device; driven by the wake timer.
    void onWakeUp();
#endif // defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)

#if defined(Q_OS_WINDOWS)
    ScopedHandle handle_;
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    // Persistent virtual pointer that keeps the display awake (and wakes it when already off) by feeding
    // net-zero motion through the kernel input subsystem. It must outlive the create-nudge call: a
    // throwaway device is destroyed before the compositor's input stack enumerates it, so its events are
    // lost. |wake_timer_| (a child of this object) nudges it once the device is enumerated and then
    // periodically, so the display never blanks for the duration of the session.
    int uinput_fd_ = -1;
    QTimer* wake_timer_ = nullptr;
#endif // defined(Q_OS_*)

    Q_DISABLE_COPY_MOVE(PowerSaveBlocker)
};

#endif // BASE_POWER_SAVE_BLOCKER_H
