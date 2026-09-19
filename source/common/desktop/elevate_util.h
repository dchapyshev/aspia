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

#ifndef COMMON_DESKTOP_ELEVATE_UTIL_H
#define COMMON_DESKTOP_ELEVATE_UTIL_H

#include <QObject>
#include <QStringList>

#include <functional>

#include "base/scoped_qpointer.h"

// Re-launches the application with elevated privileges so that it can do what the current process has
// no rights for. Hides the platform-specific mechanism (Windows UAC / Linux pkexec).
class ElevateUtil : public QObject
{
    Q_OBJECT

public:
    explicit ElevateUtil(QObject* parent = nullptr);
    ~ElevateUtil() override;

    // Reported instead of an exit code when the elevated process never ran or its code could not be
    // read. On macOS the mechanism reports completion without a code, so it is always this.
    static constexpr int kNoExitCode = -1;

    // Creates the implementation for the current platform, or nullptr if elevation is not supported.
    static ScopedQPointer<ElevateUtil> create(QObject* parent = nullptr);

    // True when this process already has the privileges elevation would ask for.
    static bool isPrivileged();

    // Re-launches the application elevated with |arguments| (e.g. { "--config" }), parented to the
    // |parent_window| native handle, and returns true. Returns false when the process is already
    // privileged or the relaunch could not be started, and then |on_finished| is never called.
    // Otherwise |on_finished| is called exactly once with the exit code of the elevated process, and
    // it may be called before this function returns.
    virtual bool runElevated(const QStringList& arguments, quintptr parent_window,
                             std::function<void(int)> on_finished) = 0;

private:
    Q_DISABLE_COPY_MOVE(ElevateUtil)
};

#endif // COMMON_DESKTOP_ELEVATE_UTIL_H
