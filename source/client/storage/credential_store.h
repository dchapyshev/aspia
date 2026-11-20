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

#ifndef CLIENT_STORAGE_CREDENTIAL_STORE_H
#define CLIENT_STORAGE_CREDENTIAL_STORE_H

#include "client/storage/database_store.h"

class SyncTransaction;

class CredentialStore final : public DatabaseStore
{
public:
    explicit CredentialStore(Database& db = Database::instance());

    struct Unsynced
    {
        CredentialConfig credential;
        bool deleted = false;
        qint64 sync_revision = 0;
        qint64 sync_dirty = 0;
    };

    Database::ReadResult list(QList<CredentialConfig>* credentials,
                              std::optional<CredentialConfig::Type> type = std::nullopt) const;
    bool add(CredentialConfig& credential);
    bool modify(const CredentialConfig& credential);
    bool remove(qint64 credential_id);
    Database::FindResult find(qint64 credential_id, CredentialConfig* credential) const;
    Database::FindResult findByGuid(const QString& guid, CredentialConfig* credential) const;
    Database::ReadResult unsynced(QList<Unsynced>* credentials) const;

    bool storeReceived(SyncTransaction& transaction, const CredentialConfig& credential, qint64 sync_revision);
    bool setUnknownData(SyncTransaction& transaction, const QString& guid, const QByteArray& unknown_data);
};

#endif // CLIENT_STORAGE_CREDENTIAL_STORE_H
