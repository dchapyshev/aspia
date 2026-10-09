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

#include "base/os_account.h"

namespace {

constexpr qsizetype kMaxNamePartLength = 255;

//--------------------------------------------------------------------------------------------------
bool isValidNamePart(const QString& part)
{
    static const QString kInvalidChars("\"/\\[]:;|=,+*?<>");

    if (part.size() > kMaxNamePartLength)
        return false;

    for (const QChar c : part)
    {
        if (c.unicode() < 0x20 || kInvalidChars.contains(c))
            return false;
    }

    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool OsAccount::isValidUserName(const QString& username)
{
    QString domain;
    QString user = username;

    const qsizetype separator = username.indexOf('\\');
    if (separator >= 0)
    {
        domain = username.left(separator);
        user = username.mid(separator + 1);
    }

    return !user.isEmpty() && isValidNamePart(user) && isValidNamePart(domain);
}
