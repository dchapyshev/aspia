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

#include "client/file_status.h"

#include <QCoreApplication>

namespace aspia {

QString fileStatusToString(proto::file_transfer::Status status)
{
    switch (status)
    {
        case proto::file_transfer::STATUS_SUCCESS:
            return QCoreApplication::tr("Successfully completed", "FileStatus");

        case proto::file_transfer::STATUS_INVALID_REQUEST:
            return QCoreApplication::tr("Invalid request", "FileStatus");

        case proto::file_transfer::STATUS_INVALID_PATH_NAME:
            return QCoreApplication::tr("Invalid directory or file name", "FileStatus");

        case proto::file_transfer::STATUS_PATH_NOT_FOUND:
            return QCoreApplication::tr("Path not found", "FileStatus");

        case proto::file_transfer::STATUS_PATH_ALREADY_EXISTS:
            return QCoreApplication::tr("Path already exists", "FileStatus");

        case proto::file_transfer::STATUS_NO_DRIVES_FOUND:
            return QCoreApplication::tr("No drives found", "FileStatus");

        case proto::file_transfer::STATUS_DISK_FULL:
            return QCoreApplication::tr("Disk full", "FileStatus");

        case proto::file_transfer::STATUS_ACCESS_DENIED:
            return QCoreApplication::tr("Access denied", "FileStatus");

        case proto::file_transfer::STATUS_FILE_OPEN_ERROR:
            return QCoreApplication::tr("Could not open file for reading", "FileStatus");

        case proto::file_transfer::STATUS_FILE_CREATE_ERROR:
            return QCoreApplication::tr("Could not create or replace file", "FileStatus");

        case proto::file_transfer::STATUS_FILE_WRITE_ERROR:
            return QCoreApplication::tr("Could not write to file", "FileStatus");

        case proto::file_transfer::STATUS_FILE_READ_ERROR:
            return QCoreApplication::tr("Could not read file", "FileStatus");

        case proto::file_transfer::STATUS_DISK_NOT_READY:
            return QCoreApplication::tr("Drive not ready", "FileStatus");

        default:
            return QCoreApplication::tr("Unknown status code", "FileStatus");
    }
}

} // namespace aspia
