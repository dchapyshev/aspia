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

#ifndef CLIENT_STORAGE_SYNC_STORE_H
#define CLIENT_STORAGE_SYNC_STORE_H

#include "client/storage/database_store.h"
#include "proto/router_sync.h"

class SyncTransaction final : public DatabaseStore
{
public:
    explicit SyncTransaction(Database& db = Database::instance());

    Database::SyncResult begin(const QByteArray& storage_id);
    bool commit();

    bool eraseRecord(proto::router::SyncDataType type, const QString& record_id);
    bool setRecordSyncRevision(proto::router::SyncDataType type, const QString& record_id, qint64 sync_revision);
    bool confirmSentRecord(proto::router::SyncDataType type, const QString& record_id, qint64 sync_revision,
                           qint64 sync_dirty);
    bool setReceivedRevision(proto::router::SyncDataType type, qint64 revision);
    bool storeSentRecord(proto::router::SyncDataType type, const QString& record_id, const QByteArray& data_hash,
                         qint64 sync_dirty);
    Database::FindResult findSentRecord(proto::router::SyncDataType type, const QString& record_id,
                                        const QByteArray& data_hash, qint64* sync_dirty) const;
    bool eraseSentRecord(proto::router::SyncDataType type, const QString& record_id, const QByteArray& data_hash);
    bool eraseAllSentRecords(proto::router::SyncDataType type, const QString& record_id);

    bool isActive() const;

private:
    std::optional<SqlTransaction> transaction_;

    Q_DISABLE_COPY_MOVE(SyncTransaction)
};

class SyncStore final : public DatabaseStore
{
public:
    explicit SyncStore(Database& db = Database::instance());

    enum SyncState : qint64
    {
        SYNC_STATE_PRESENT = 0, // An ordinary record.
        SYNC_STATE_DELETED = 1, // Removed on the device, the router does not know it yet.
        SYNC_STATE_EMPTY   = 2, // Holds the guid of a record that has not come from the router.
        SYNC_STATE_WAITING = 3  // Came from the router, the group it lies in has not come.
    };

    qint64 receivedRevision(proto::router::SyncDataType type) const;
    bool reset();
    bool discard();
    bool clearReceivedRevisions();

private:
    friend class SyncTransaction;

    bool setReceivedRevision(proto::router::SyncDataType type, qint64 revision);
};

#endif // CLIENT_STORAGE_SYNC_STORE_H
