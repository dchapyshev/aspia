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

#ifndef CLIENT_STORAGE_LOCAL_HOST_STORE_H
#define CLIENT_STORAGE_LOCAL_HOST_STORE_H

#include "client/storage/database_store.h"

class LocalHostStore final : public DatabaseStore
{
public:
    explicit LocalHostStore(Database& db = Database::instance());

    Database::ReadResult list(qint64 group_id, QList<LocalHostConfig>* hosts) const;
    Database::ReadResult all(QList<LocalHostConfig>* hosts) const;
    bool add(LocalHostConfig& host);
    bool modify(LocalHostConfig& host);
    bool move(qint64 entry_id, qint64 new_group_id);
    bool remove(qint64 entry_id);
    bool setConnectTime(qint64 entry_id, qint64 connect_time);
    bool setOsCredentialId(qint64 entry_id, qint64 credential_id);
    Database::FindResult find(qint64 entry_id, LocalHostConfig* host) const;
    std::optional<std::pair<QString, SecureString>> credentials(qint64 entry_id) const;
    Database::ReadResult search(const QString& query, QList<LocalHostConfig>* hosts) const;
};

#endif // CLIENT_STORAGE_LOCAL_HOST_STORE_H
