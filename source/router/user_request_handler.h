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

#ifndef ROUTER_USER_REQUEST_HANDLER_H
#define ROUTER_USER_REQUEST_HANDLER_H

#include <QList>
#include <QString>

#include <string>

namespace proto::router {
class UserRequest;
} // namespace proto::router

class Database;

// Handling of the user commands of the admin channel (add, modify, delete, OTP reset, token
// revocation). Everything these commands need is the database, so the logic lives here instead of
// inside the session: ClientAdmin stays a thin adapter that sends the reply and turns the returned
// side effects into signals, and the whole surface is testable against a temporary database.
class UserRequestHandler
{
public:
    // The administrator that sent the request.
    struct Caller
    {
        qint64 user_id = 0;
        QString name; // Audit log only.
    };

    // The error code of the reply and everything the session must do after sending it.
    struct Result
    {
        std::string error_code;

        // ClientWorker::NOTIFY_* bits the sessions must be told about (0 - nothing changed).
        quint32 notify_flags = 0;

        // Live sessions of this user must be dropped (0 - nothing to drop): the credentials they
        // authenticated with are gone (password rotation, OTP reset, token revocation) or the
        // account itself is gone or disabled.
        qint64 stop_user_id = 0;

        // Restricts the drop to the sessions holding these device tokens; empty means every
        // session of |stop_user_id|.
        QList<qint64> stop_token_ids;
    };

    static Result handle(Database& database, const Caller& caller,
                         const proto::router::UserRequest& request);
};

#endif // ROUTER_USER_REQUEST_HANDLER_H
