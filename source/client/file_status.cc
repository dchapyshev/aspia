//
// PROJECT:         Aspia
// FILE:            client/file_status.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/file_status.h"

#include <QCoreApplication>

namespace aspia {

QString fileStatusToString(proto::file_transfer::Status status)
{
    switch (status)
    {
        case proto::file_transfer::STATUS_SUCCESS:
            return QCoreApplication::tr("Successfully completed");

        case proto::file_transfer::STATUS_INVALID_REQUEST:
            return QCoreApplication::tr("Invalid request");

        case proto::file_transfer::STATUS_INVALID_PATH_NAME:
            return QCoreApplication::tr("Invalid directory or file name");

        case proto::file_transfer::STATUS_PATH_NOT_FOUND:
            return QCoreApplication::tr("Path not found");

        case proto::file_transfer::STATUS_PATH_ALREADY_EXISTS:
            return QCoreApplication::tr("Path already exists");

        case proto::file_transfer::STATUS_NO_DRIVES_FOUND:
            return QCoreApplication::tr("No drives found");

        case proto::file_transfer::STATUS_DISK_FULL:
            return QCoreApplication::tr("Disk full");

        case proto::file_transfer::STATUS_ACCESS_DENIED:
            return QCoreApplication::tr("Access denied");

        case proto::file_transfer::STATUS_FILE_OPEN_ERROR:
            return QCoreApplication::tr("Could not open file for reading");

        case proto::file_transfer::STATUS_FILE_CREATE_ERROR:
            return QCoreApplication::tr("Could not create or replace file");

        case proto::file_transfer::STATUS_FILE_WRITE_ERROR:
            return QCoreApplication::tr("Could not write to file");

        case proto::file_transfer::STATUS_FILE_READ_ERROR:
            return QCoreApplication::tr("Could not read file");

        default:
            return QCoreApplication::tr("Unknown status code");
    }
}

} // namespace aspia
