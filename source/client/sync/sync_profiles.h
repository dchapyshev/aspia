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

#ifndef CLIENT_SYNC_SYNC_PROFILES_H
#define CLIENT_SYNC_SYNC_PROFILES_H

#include "client/sync/sync_records.h"

struct SyncProfileTraits
{
    using Config = ProfileConfig;
    using Unsynced = Database::UnsyncedProfile;

    static constexpr proto::router::SyncDataType kType = proto::router::SYNC_DATA_TYPE_PROFILES;

    static const Config& config(const Unsynced& unsynced) { return unsynced.profile; }
    static bool isReadable(const Unsynced& unsynced) { return !unsynced.damaged && unsynced.profile.isValid(); }
    static Database::ReadResult readUnsynced(const Database& db, QList<Unsynced>* unsynced);
    static qint64 receivedRevision(const Database& db);

    static std::optional<QByteArray> seal(const SecureByteArray& key, const Config& config);
    static bool openReceived(const SecureByteArray& key, const proto::router::SyncRecord& record, Config* config);
    static bool hasSameContent(const Config& first, const Config& second);
    static bool storeReceived(const Database& db, Database::SyncTransaction& transaction, Config config,
                              qint64 sync_revision);
    static bool storeUnknownData(Database::SyncTransaction& transaction, const Config& config);
    static bool erase(Database::SyncTransaction& transaction, const QString& guid);
    static bool setSyncRevision(Database::SyncTransaction& transaction, const QString& guid, qint64 sync_revision);
    static bool confirmSent(Database::SyncTransaction& transaction, const QString& guid, qint64 sync_revision,
                            qint64 sync_dirty);
    static bool setReceivedRevision(Database::SyncTransaction& transaction, qint64 revision);
};

extern template class SyncRecordsOf<SyncProfileTraits>;
using SyncProfiles = SyncRecordsOf<SyncProfileTraits>;

#endif // CLIENT_SYNC_SYNC_PROFILES_H
