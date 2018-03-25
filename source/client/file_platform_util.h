//
// PROJECT:         Aspia
// FILE:            client/file_platform_util.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CLIENT__FILE_PLATFORM_UTIL_H
#define _ASPIA_CLIENT__FILE_PLATFORM_UTIL_H

#include <QIcon>
#include <QPair>
#include <QString>

#include "base/macros.h"
#include "proto/file_transfer_session.pb.h"

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

#endif // _ASPIA_CLIENT__FILE_PLATFORM_UTIL_H
