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

#ifndef CLIENT_SYNC_SYNC_ROUTER_HOSTS_H
#define CLIENT_SYNC_SYNC_ROUTER_HOSTS_H

#include "client/storage/router_host_store.h"
#include "client/sync/sync_records.h"

struct SyncRouterHostTraits
{
    using Config = RouterHostConfig;
    using Unsynced = RouterHostStore::Unsynced;

    static constexpr proto::router::SyncDataType kType = proto::router::SYNC_DATA_TYPE_ROUTER_HOSTS;

    static const Config& config(const Unsynced& unsynced) { return unsynced.host; }
    static QString recordId(const Config& config) { return QString::number(config.hostId()); }
    static bool isReadable(const Unsynced& unsynced) { return !unsynced.damaged && unsynced.host.isValid(); }
    static Database::ReadResult readUnsynced(Database& db, QList<Unsynced>* unsynced);

    static std::optional<QByteArray> seal(Database& db, const SecureByteArray& key, const Config& config);
    static SyncRecords::OpenResult openReceived(
        Database& db, const SecureByteArray& key, const proto::router::SyncRecord& record, Config* config);
    static bool storeReceived(Database& db, SyncTransaction& transaction, Config config, qint64 sync_revision);
};

extern template class SyncRecordsOf<SyncRouterHostTraits>;
using SyncRouterHosts = SyncRecordsOf<SyncRouterHostTraits>;

#endif // CLIENT_SYNC_SYNC_ROUTER_HOSTS_H
