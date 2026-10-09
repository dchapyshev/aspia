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

#ifndef BASE_OS_ACCOUNT_H
#define BASE_OS_ACCOUNT_H

#include <QString>

class OsAccount
{
public:
    // The name is "user" or "domain\user". The user is not empty, and neither part is longer than
    // 255 characters or contains control characters or any of " / \ [ ] : ; | = , + * ? < >.
    static bool isValidUserName(const QString& username);

private:
    Q_DISABLE_COPY_MOVE(OsAccount)
};

#endif // BASE_OS_ACCOUNT_H
