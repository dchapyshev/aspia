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

#ifndef CLIENT_BACKUP_H
#define CLIENT_BACKUP_H

#include <QString>

#include "base/crypto/secure_string.h"

class Database;

// A copy of the whole address book in a single sealed file. An export writes the book as it is, and
// an import puts that book back in place of the current one. The file is sealed with the master
// password of the book it was saved from, so an export asks nothing of the user.
class Backup
{
public:
    enum class Result
    {
        SUCCESS,
        DATABASE_UNAVAILABLE,
        FILE_ERROR,           // The file could not be opened, read or written.
        INVALID_FORMAT,       // Not a valid/recognized backup file.
        UNSUPPORTED_VERSION,
        WRONG_PASSWORD,
        NOTHING_EXPORTED,
        NOTHING_IMPORTED,
        INTERNAL_ERROR,
    };

    // How many records of each kind went through.
    struct Report
    {
        int routers = 0;
        int local_groups = 0;
        int local_hosts = 0;
        int router_hosts = 0;

        int total() const { return routers + local_groups + local_hosts + router_hosts; }
    };

    // Writes the address book of |db| to |file_path|. |report| (optional) receives what was
    // written, and is left alone unless the file was.
    static Result exportToFile(Database& db, const QString& file_path, Report* report = nullptr);

    // Puts the address book of the file in place of the one |db| holds. What the book had is
    // deleted. An empty |password| means the file is expected to open with the key the book is
    // already open with, and a file saved from another book answers WRONG_PASSWORD. |report|
    // (optional) receives what arrived, and is left alone unless the book took it.
    static Result importFromFile(
        Database& db, const QString& file_path, const SecureString& password,
        Report* report = nullptr);

private:
    Q_DISABLE_COPY_MOVE(Backup)
};

#endif // CLIENT_BACKUP_H
