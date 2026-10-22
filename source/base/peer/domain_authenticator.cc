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

#include "base/peer/domain_authenticator.h"

//--------------------------------------------------------------------------------------------------
// static
void DomainAuthenticator::splitAccountName(const QString& account, QString* domain, QString* user)
{
    if (!domain || !user)
        return;

    const qsizetype separator = account.indexOf(QLatin1Char('\\'));
    if (separator <= 0)
    {
        domain->clear();
        *user = account;
        return;
    }

    *domain = account.left(separator);
    *user = account.mid(separator + 1);
}

//--------------------------------------------------------------------------------------------------
// static
const char* DomainAuthenticator::statusToString(Status status)
{
    switch (status)
    {
        case Status::SUCCESS:             return "SUCCESS";
        case Status::INVALID_CREDENTIALS: return "INVALID_CREDENTIALS";
        case Status::ACCOUNT_DISABLED:    return "ACCOUNT_DISABLED";
        case Status::ACCOUNT_LOCKED:      return "ACCOUNT_LOCKED";
        case Status::ACCOUNT_EXPIRED:     return "ACCOUNT_EXPIRED";
        case Status::CREDENTIALS_EXPIRED:    return "CREDENTIALS_EXPIRED";
        case Status::LOGON_DENIED:        return "LOGON_DENIED";
        case Status::NO_PERMISSIONS:      return "NO_PERMISSIONS";
        case Status::UNSUPPORTED:         return "UNSUPPORTED";
        case Status::INTERNAL_ERROR:      return "INTERNAL_ERROR";
    }

    return "UNKNOWN";
}
