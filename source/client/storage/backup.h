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

#ifndef CLIENT_STORAGE_BACKUP_H
#define CLIENT_STORAGE_BACKUP_H

#include <QString>

#include "base/crypto/secure_string.h"

class Database;

class Backup
{
public:
    enum class Result
    {
        SUCCESS,
        DATABASE_UNAVAILABLE,
        FILE_ERROR,
        INVALID_FORMAT,
        UNSUPPORTED_VERSION,
        WRONG_PASSWORD,
        UNREADABLE_RECORD,
        NOTHING_IMPORTED,
        INTERNAL_ERROR,
    };

    struct Report
    {
        bool router = false;
        int local_groups = 0;
        int local_hosts = 0;
        int router_hosts = 0;
        int credentials = 0;
        int profiles = 0;

        int total() const
        {
            return (router ? 1 : 0) + local_groups + local_hosts + router_hosts + credentials + profiles;
        }
    };

    static Result exportToFile(Database& db, const QString& file_path, Report* report = nullptr);

    static Result importFromFile(
        Database& db, const QString& file_path, const SecureString& password,
        Report* report = nullptr);

private:
    Q_DISABLE_COPY_MOVE(Backup)
};

#endif // CLIENT_STORAGE_BACKUP_H
