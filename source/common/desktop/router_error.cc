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

#include "common/desktop/router_error.h"

#include <QCoreApplication>

#include "proto/router_constants.h"

//--------------------------------------------------------------------------------------------------
QString routerErrorText(std::string_view error_code)
{
    const char* message;

    if (error_code == proto::router::kErrorOk)
        return QString();
    else if (error_code == proto::router::kErrorInvalidRequest)
        message = QT_TRANSLATE_NOOP("RouterError", "The router rejected the request.");
    else if (error_code == proto::router::kErrorInternalError)
        message = QT_TRANSLATE_NOOP("RouterError", "Unknown internal error.");
    else if (error_code == proto::router::kErrorInvalidEntryId)
        message = QT_TRANSLATE_NOOP("RouterError", "Invalid entry id.");
    else if (error_code == proto::router::kErrorInvalidData)
        message = QT_TRANSLATE_NOOP("RouterError", "Invalid data was passed.");
    else if (error_code == proto::router::kErrorAlreadyExists)
        message = QT_TRANSLATE_NOOP("RouterError", "A record with the specified name already exists.");
    else if (error_code == proto::router::kErrorNotFound)
        message = QT_TRANSLATE_NOOP("RouterError", "Record not found. The list may be out of date.");
    else if (error_code == proto::router::kErrorAccessDenied)
        message = QT_TRANSLATE_NOOP("RouterError", "Access denied.");
    else if (error_code == proto::router::kErrorConflict)
        message = QT_TRANSLATE_NOOP(
            "RouterError", "The data has changed on the router. Refresh the list and try again.");
    else if (error_code == proto::router::kErrorLostConnection)
        message = QT_TRANSLATE_NOOP("RouterError", "Connection to the router lost.");
    else
        message = QT_TRANSLATE_NOOP("RouterError", "Unknown error type.");

    return QCoreApplication::translate("RouterError", message);
}
