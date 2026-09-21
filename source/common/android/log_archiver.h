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

#ifndef COMMON_ANDROID_LOG_ARCHIVER_H
#define COMMON_ANDROID_LOG_ARCHIVER_H

#include <QString>

// Packs the log files of the application into one archive the user can pass on.
class LogArchiver
{
public:
    enum class Result
    {
        SUCCESS,  // The archive is written; the place it went to is reported.
        NO_LOGS,  // The application has no log files yet.
        FAILED    // The archive could not be written.
    };

    // Writes the archive to the downloads of the device and names in |location| where it went.
    static Result saveToDownloads(QString* location);

private:
    Q_DISABLE_COPY_MOVE(LogArchiver)
};

#endif // COMMON_ANDROID_LOG_ARCHIVER_H
