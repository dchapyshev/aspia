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

#ifndef CLIENT_STORAGE_LOCAL_GROUP_STORE_H
#define CLIENT_STORAGE_LOCAL_GROUP_STORE_H

#include "client/storage/database_store.h"

class SyncTransaction;

class LocalGroupStore final : public DatabaseStore
{
public:
    explicit LocalGroupStore(Database& db = Database::instance());

    struct Unsynced
    {
        LocalGroupConfig group;
        bool deleted = false;
        bool damaged = false;
        qint64 sync_revision = 0;
        qint64 sync_dirty = 0;
    };

    bool list(qint64 parent_id, QList<LocalGroupConfig>* groups) const;
    bool all(QList<LocalGroupConfig>* groups) const;
    bool add(LocalGroupConfig& group);
    bool modify(const LocalGroupConfig& group);
    bool move(qint64 group_id, qint64 new_parent_id);
    bool remove(qint64 group_id);
    Database::FindResult find(qint64 group_id, LocalGroupConfig* group) const;
    Database::FindResult findRowIdByGuid(const QString& guid, qint64* group_id) const;
    Database::FindResult findGuidByRowId(qint64 group_id, QString* guid) const;
    Database::ReadResult unsynced(QList<Unsynced>* groups) const;

    bool storeReceived(SyncTransaction& transaction, const LocalGroupConfig& group, qint64 sync_revision);
    bool eraseReceived(SyncTransaction& transaction, const QString& guid);
    bool addEmptyRow(const QString& guid, qint64* group_id);
};

#endif // CLIENT_STORAGE_LOCAL_GROUP_STORE_H
