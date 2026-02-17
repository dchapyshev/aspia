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

#ifndef COMMON_FILE_PLATFORM_UTIL_H
#define COMMON_FILE_PLATFORM_UTIL_H

#include <QIcon>
#include <QString>

#include <utility>

#include "proto/file_transfer.h"

namespace common {

class FilePlatformUtil
{
public:
    // Returns a pair of icons for the file type and a description of the file type.
    static std::pair<QIcon, QString> fileTypeInfo(const QString& file_name);

    // The methods below return the appropriate icons.
    static QIcon computerIcon();
    static QIcon directoryIcon();

    static QIcon driveIcon(proto::file_transfer::DriveList::Item::Type type);

    static const QList<QChar>& invalidFileNameCharacters();
    static const QList<QChar>& invalidPathCharacters();

    static bool isValidPath(const QString& path);
    static bool isValidFileName(const QString& file_name);

private:
    Q_DISABLE_COPY(FilePlatformUtil)
};

} // namespace common

#endif // COMMON_FILE_PLATFORM_UTIL_H
