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

#ifndef CLIENT_SYNC_SYNC_CREDENTIALS_H
#define CLIENT_SYNC_SYNC_CREDENTIALS_H

#include "client/storage/credential_store.h"
#include "client/sync/sync_records.h"

struct SyncCredentialTraits
{
    using Config = CredentialConfig;
    using Unsynced = CredentialStore::Unsynced;

    static constexpr proto::router::SyncDataType kType = proto::router::SYNC_DATA_TYPE_CREDENTIALS;

    static const Config& config(const Unsynced& unsynced) { return unsynced.credential; }
    static bool isReadable(const Unsynced& unsynced) { return unsynced.credential.isValid(); }
    static Database::ReadResult readUnsynced(Database& db, QList<Unsynced>* unsynced);

    static std::optional<QByteArray> seal(const SecureByteArray& key, const Config& config);
    static bool openReceived(const SecureByteArray& key, const proto::router::SyncRecord& record, Config* config);
    static bool hasSameContent(const Config& first, const Config& second);
    static bool storeReceived(Database& db, Database::SyncTransaction& transaction, Config config,
                              qint64 sync_revision);
    static bool storeUnknownData(Database& db, Database::SyncTransaction& transaction, const Config& config);
};

extern template class SyncRecordsOf<SyncCredentialTraits>;
using SyncCredentials = SyncRecordsOf<SyncCredentialTraits>;

#endif // CLIENT_SYNC_SYNC_CREDENTIALS_H
