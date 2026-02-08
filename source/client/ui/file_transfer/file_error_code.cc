//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/ui/file_transfer/file_error_code.h"

#include <QCoreApplication>

namespace client {

//--------------------------------------------------------------------------------------------------
QString fileErrorToString(proto::file_transfer::ErrorCode error_code)
{
    const char* message;

    switch (error_code)
    {
        case proto::file_transfer::ERROR_CODE_SUCCESS:
            message = QT_TRANSLATE_NOOP("FileError", "Successfully completed");
            break;

        case proto::file_transfer::ERROR_CODE_INVALID_REQUEST:
            message = QT_TRANSLATE_NOOP("FileError", "Invalid request");
            break;

        case proto::file_transfer::ERROR_CODE_INVALID_PATH_NAME:
            message = QT_TRANSLATE_NOOP("FileError", "Invalid directory or file name");
            break;

        case proto::file_transfer::ERROR_CODE_PATH_NOT_FOUND:
            message = QT_TRANSLATE_NOOP("FileError", "Path not found");
            break;

        case proto::file_transfer::ERROR_CODE_PATH_ALREADY_EXISTS:
            message = QT_TRANSLATE_NOOP("FileError", "Path already exists");
            break;

        case proto::file_transfer::ERROR_CODE_NO_DRIVES_FOUND:
            message = QT_TRANSLATE_NOOP("FileError", "No drives found");
            break;

        case proto::file_transfer::ERROR_CODE_DISK_FULL:
            message = QT_TRANSLATE_NOOP("FileError", "Disk full");
            break;

        case proto::file_transfer::ERROR_CODE_ACCESS_DENIED:
            message = QT_TRANSLATE_NOOP("FileError", "Access denied");
            break;

        case proto::file_transfer::ERROR_CODE_FILE_OPEN_ERROR:
            message = QT_TRANSLATE_NOOP("FileError", "Could not open file for reading");
            break;

        case proto::file_transfer::ERROR_CODE_FILE_CREATE_ERROR:
            message = QT_TRANSLATE_NOOP("FileError", "Could not create or replace file");
            break;

        case proto::file_transfer::ERROR_CODE_FILE_WRITE_ERROR:
            message = QT_TRANSLATE_NOOP("FileError", "Could not write to file");
            break;

        case proto::file_transfer::ERROR_CODE_FILE_READ_ERROR:
            message = QT_TRANSLATE_NOOP("FileError", "Could not read file");
            break;

        case proto::file_transfer::ERROR_CODE_DISK_NOT_READY:
            message = QT_TRANSLATE_NOOP("FileError", "Drive not ready");
            break;

        case proto::file_transfer::ERROR_CODE_NO_LOGGED_ON_USER:
            message = QT_TRANSLATE_NOOP("FileError", "No logged in user");
            break;

        default:
            message = QT_TRANSLATE_NOOP("FileError", "Unknown error code");
            break;
    }

    return QCoreApplication::translate("FileError", message);
}

} // namespace client
