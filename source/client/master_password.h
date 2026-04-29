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

#ifndef CLIENT_MASTER_PASSWORD_H
#define CLIENT_MASTER_PASSWORD_H

#include <QString>

namespace client {

class MasterPassword
{
public:
    static const int kSafePasswordLength = 8;

    static bool isSafePassword(const QString& password);
    static bool isSet();

    static bool unlock(const QString& password);

    static bool setNew(const QString& new_password);
    static bool change(const QString& current_password, const QString& new_password);
    static bool clear(const QString& current_password);

private:
    MasterPassword() = delete;
    ~MasterPassword() = delete;
};

} // namespace client

#endif // CLIENT_MASTER_PASSWORD_H
