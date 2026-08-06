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

#ifndef ROUTER_HANDLERS_REQUEST_RESULT_H
#define ROUTER_HANDLERS_REQUEST_RESULT_H

#include <string>
#include <vector>

// The error code of a command reply and everything the session must do after sending it. Shared by
// every database-backed command handler; the fields a command cannot produce keep their defaults.
struct RequestResult
{
    std::string error_code;

    // Id of the created entry (add commands only).
    qint64 entry_id = 0;

    // ClientWorker::NOTIFY_* bits the sessions must be told about (0 - nothing changed).
    quint32 notify_flags = 0;

    // Live sessions of this user must be dropped (0 - nothing to drop): the credentials they
    // authenticated with are gone (password rotation, OTP reset, token revocation) or the account
    // itself is gone or disabled.
    qint64 stop_user_id = 0;

    // Restricts the drop to the sessions holding these device tokens; empty means every session of
    // |stop_user_id|.
    std::vector<qint64> stop_token_ids;
};

#endif // ROUTER_HANDLERS_REQUEST_RESULT_H
