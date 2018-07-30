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

#ifndef ASPIA_HOST__FILE_PLATFORM_UTIL_H_
#define ASPIA_HOST__FILE_PLATFORM_UTIL_H_

#include <QIcon>
#include <QPair>
#include <QString>

#include "base/macros_magic.h"
#include "protocol/file_transfer_session.pb.h"

namespace aspia {

class FilePlatformUtil
{
public:
    // Returns a pair of icons for the file type and a description of the file type.
    static QPair<QIcon, QString> fileTypeInfo(const QString& file_name);

    // The methods below return the appropriate icons.
    static QIcon computerIcon();
    static QIcon directoryIcon();

    static QIcon driveIcon(proto::file_transfer::DriveList::Item::Type type);
    static proto::file_transfer::DriveList::Item::Type driveType(const QString& drive_path);

private:
    DISALLOW_COPY_AND_ASSIGN(FilePlatformUtil);
};

} // namespace aspia

#endif // ASPIA_HOST__FILE_PLATFORM_UTIL_H_
