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

#ifndef ASPIA_BASE__FILE_LOGGER_H_
#define ASPIA_BASE__FILE_LOGGER_H_

#include <QFile>
#include <QScopedPointer>

#include "base/macros_magic.h"

namespace aspia {

class FileLogger
{
public:
    FileLogger();
    ~FileLogger();

    bool startLogging(const QString& prefix);

private:
    static void messageHandler(QtMsgType type,
                               const QMessageLogContext& context,
                               const QString& msg);

    static QScopedPointer<QFile> file_;

    DISALLOW_COPY_AND_ASSIGN(FileLogger);
};

} // namespace aspia

#endif // ASPIA_BASE__FILE_LOGGER_H_
