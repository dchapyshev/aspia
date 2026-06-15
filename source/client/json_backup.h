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

#ifndef CLIENT_JSON_BACKUP_H
#define CLIENT_JSON_BACKUP_H

#include <QString>

#include "base/crypto/secure_string.h"

class JsonBackup
{
public:
    enum class Result
    {
        SUCCESS,
        DATABASE_UNAVAILABLE,
        FILE_ERROR,           // The file could not be opened, read or written.
        INVALID_FORMAT,       // Not a valid/recognized address book file.
        UNSUPPORTED_VERSION,
        WRONG_PASSWORD,
        NOTHING_IMPORTED,
        INTERNAL_ERROR,
    };

    struct ExportCounts
    {
        int routers = 0;
        int groups = 0;
        int hosts = 0;
    };

    struct ImportCounts
    {
        int routers = 0;
        int routers_skipped = 0;
        int groups = 0;
        int groups_skipped = 0;
        int hosts = 0;
        int hosts_skipped = 0;

        int total() const { return routers + groups + hosts; }
    };

    // Exports the local address book (routers, groups, hosts) to |file_path|, encrypted with
    // |password|. |counts| (optional) receives the number of exported entries.
    static Result exportToFile(const QString& file_path, const SecureString& password,
                               ExportCounts* counts = nullptr);

    // Imports the address book from |file_path|, decrypting it with |password|, into the local
    // database. |counts| (optional) receives the added/skipped tallies.
    static Result importFromFile(const QString& file_path, const SecureString& password,
                                 ImportCounts* counts = nullptr);

private:
    Q_DISABLE_COPY_MOVE(JsonBackup)
};

#endif // CLIENT_JSON_BACKUP_H
