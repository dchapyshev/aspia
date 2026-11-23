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

#ifndef CLIENT_SYNC_SYNC_LINKS_H
#define CLIENT_SYNC_SYNC_LINKS_H

#include <QString>

#include <optional>
#include <string>

class Database;

// A record names the records it links by guid, and the database by id.
class SyncLinks
{
public:
    static std::optional<QString> credentialGuid(Database& db, qint64 credential_id);
    static std::optional<QString> profileGuid(Database& db, qint64 profile_id);
    static std::optional<QString> groupGuid(Database& db, qint64 group_id);

    // A guid unknown to the device gets an empty row, which keeps the link until the record comes.
    static std::optional<qint64> credentialId(Database& db, const std::string& guid);
    static std::optional<qint64> profileId(Database& db, const std::string& guid);
    static std::optional<qint64> groupId(Database& db, const std::string& guid);

private:
    Q_DISABLE_COPY_MOVE(SyncLinks)
};

#endif // CLIENT_SYNC_SYNC_LINKS_H
