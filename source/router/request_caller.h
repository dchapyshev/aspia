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

#ifndef ROUTER_REQUEST_CALLER_H
#define ROUTER_REQUEST_CALLER_H

#include <QString>

// The authenticated session a request came from, as the request handlers see it. The identity is
// established by the authenticator and is not taken from the request itself.
struct RequestCaller
{
    qint64 user_id = 0;
    QString name; // Audit log only.
};

#endif // ROUTER_REQUEST_CALLER_H
