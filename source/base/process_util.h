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

#ifndef BASE_PROCESS_UTIL_H
#define BASE_PROCESS_UTIL_H

#include <QObject>

class ProcessUtil
{
    Q_GADGET

public:
    // Returns the PID of the current process.
    static quint32 currentProcessId();

    // Returns the PID of the parent process of |pid|, or 0 on failure.
    static quint32 parentProcessId(quint32 pid);

    // Returns the absolute path to the executable file of process |pid|, or empty string on
    // failure.
    static QString filePath(quint32 pid);

#if defined(Q_OS_WINDOWS)
    static bool isProcessElevated();

    enum class ExecuteMode { NORMAL, ELEVATE };
    Q_ENUM(ExecuteMode)

    static bool createProcess(const QString& program, const QString& arguments,
                              ExecuteMode mode = ExecuteMode::NORMAL);
#endif // defined(Q_OS_WINDOWS)

private:
    Q_DISABLE_COPY_MOVE(ProcessUtil)
};

#endif // BASE_PROCESS_UTIL_H
