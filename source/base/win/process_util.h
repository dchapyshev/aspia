//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_BASE__WIN__PROCESS_UTIL_H
#define ASPIA_BASE__WIN__PROCESS_UTIL_H

#include <QStringList>

namespace aspia {

enum class ProcessExecuteMode { NORMAL, ELEVATE };

bool executeProcess(const QString& program,
                    const QStringList& arguments,
                    ProcessExecuteMode mode = ProcessExecuteMode::NORMAL);

bool executeProcess(const QString& program, ProcessExecuteMode mode = ProcessExecuteMode::NORMAL);

} // namespace aspia

#endif // ASPIA_BASE__WIN__PROCESS_UTIL_H
