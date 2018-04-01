//
// PROJECT:         Aspia
// FILE:            host/file_platform_util.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__FILE_PLATFORM_UTIL_H
#define _ASPIA_HOST__FILE_PLATFORM_UTIL_H

#include <QIcon>
#include <QPair>
#include <QString>

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
    Q_DISABLE_COPY(FilePlatformUtil)
};

} // namespace aspia

#endif // _ASPIA_HOST__FILE_PLATFORM_UTIL_H
