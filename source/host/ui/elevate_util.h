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

#ifndef HOST_UI_ELEVATE_UTIL_H
#define HOST_UI_ELEVATE_UTIL_H

#include <QObject>

#include <functional>
#include <memory>

class QString;

// Re-launches the host application with elevated privileges so its settings / security-log dialog can
// edit the system configuration. Hides the platform-specific mechanism (Windows UAC / Linux pkexec).
class ElevateUtil : public QObject
{
    Q_OBJECT

public:
    explicit ElevateUtil(QObject* parent = nullptr);
    ~ElevateUtil() override;

    // Creates the implementation for the current platform, or nullptr if elevation is not supported.
    static std::unique_ptr<ElevateUtil> create(QObject* parent = nullptr);

    // If the process lacks the privileges to edit the system configuration, re-launches the application
    // elevated with |argument| (e.g. "--config"), parented to the |parent_window| native handle, and
    // returns true; |on_finished| is invoked when the elevated process exits. If the process is already
    // privileged, or the relaunch could not be started, returns false and the caller should perform the
    // action in-process.
    virtual bool runElevated(const QString& argument, quintptr parent_window,
                             std::function<void()> on_finished) = 0;

private:
    Q_DISABLE_COPY_MOVE(ElevateUtil)
};

#endif // HOST_UI_ELEVATE_UTIL_H
