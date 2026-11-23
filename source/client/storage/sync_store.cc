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

#include "client/storage/sync_store.h"

#include <QDateTime>

#include "base/logging.h"
#include "base/sql/sql_query.h"
#include "client/storage/router_store.h"

namespace {

struct SyncTable
{
    proto::router::SyncDataType type;
    const char* name;
    const char* key;
    const char* received_revision_setting;
};

const SyncTable kSyncTables[] =
{
    { proto::router::SYNC_DATA_TYPE_CREDENTIALS, "credentials", "guid", "received_credentials_revision" },
    { proto::router::SYNC_DATA_TYPE_PROFILES, "profiles", "guid", "received_profiles_revision" },
    { proto::router::SYNC_DATA_TYPE_ROUTER_HOSTS, "router_hosts", "host_id", "received_router_hosts_revision" },
    { proto::router::SYNC_DATA_TYPE_LOCAL_GROUPS, "local_groups", "guid", "received_local_groups_revision" },
    { proto::router::SYNC_DATA_TYPE_LOCAL_HOSTS, "local_hosts", "guid", "received_local_hosts_revision" }
};

//--------------------------------------------------------------------------------------------------
const SyncTable* findSyncTable(proto::router::SyncDataType type)
{
    for (const SyncTable& table : kSyncTables)
    {
        if (table.type == type)
            return &table;
    }

    LOG(ERROR) << "Unknown type of the synchronized records:" << type;
    return nullptr;
}

//--------------------------------------------------------------------------------------------------
std::string tableQuery(const SyncTable& table, const char* query)
{
    return QString(query).arg(table.name, table.key).toStdString();
}

//--------------------------------------------------------------------------------------------------
bool addRecordId(const SyncTable& table, const QString& record_id, SqlQuery* query)
{
    if (table.type != proto::router::SYNC_DATA_TYPE_ROUTER_HOSTS)
    {
        query->addText(record_id);
        return true;
    }

    bool ok = false;
    const HostId host_id = record_id.toULongLong(&ok);
    if (!ok || host_id == kInvalidHostId || isTempHostId(host_id))
    {
        LOG(ERROR) << "Invalid id of a router host record:" << record_id;
        return false;
    }

    query->addUInt64(host_id);
    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
SyncTransaction::SyncTransaction(Database& db)
    : DatabaseStore(db)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
Database::SyncResult SyncTransaction::begin(const QByteArray& storage_id)
{
    transaction_.emplace(db());

    if (!transaction_->begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction";
        transaction_.reset();
        return Database::SyncResult::FAILED;
    }

    RouterConfig router;
    const Database::FindResult found = RouterStore(database_).find(&router);
    if (found != Database::FindResult::FOUND && found != Database::FindResult::NOT_FOUND)
    {
        LOG(ERROR) << "Unable to read the router";
        transaction_.reset();
        return Database::SyncResult::FAILED;
    }

    if (storage_id.isEmpty() || found == Database::FindResult::NOT_FOUND || router.syncStorageId() != storage_id)
    {
        LOG(WARNING) << "The device is not attached to the storage";
        transaction_.reset();
        return Database::SyncResult::DETACHED;
    }

    return Database::SyncResult::DONE;
}

//--------------------------------------------------------------------------------------------------
bool SyncTransaction::commit()
{
    return transaction_.has_value() && transaction_->commit();
}

//--------------------------------------------------------------------------------------------------
bool SyncTransaction::eraseRecord(proto::router::SyncDataType type, const QString& record_id)
{
    if (!isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    const SyncTable* table = findSyncTable(type);
    if (!table)
        return false;

    SqlQuery query(db(), tableQuery(*table, "DELETE FROM %1 WHERE %2=?"));
    if (!addRecordId(*table, record_id, &query))
        return false;

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool SyncTransaction::setRecordSyncRevision(
    proto::router::SyncDataType type, const QString& record_id, qint64 sync_revision)
{
    if (!isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    const SyncTable* table = findSyncTable(type);
    if (!table)
        return false;

    SqlQuery query(db(), tableQuery(*table, "UPDATE %1 SET sync_revision=? WHERE %2=?"));
    query.addInt64(sync_revision);
    if (!addRecordId(*table, record_id, &query))
        return false;

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool SyncTransaction::confirmSentRecord(
    proto::router::SyncDataType type, const QString& record_id, qint64 sync_revision, qint64 sync_dirty)
{
    if (!isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    const SyncTable* table = findSyncTable(type);
    if (!table)
        return false;

    if (record_id.isEmpty() || sync_revision <= 0 || sync_dirty < 0)
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    SqlQuery update_query(db(), tableQuery(*table, "UPDATE %1 SET sync_revision=?, sync_dirty=sync_dirty-? "
                                                    "WHERE %2=?"));
    update_query.addInt64(sync_revision);
    update_query.addInt64(sync_dirty);
    if (!addRecordId(*table, record_id, &update_query))
        return false;

    if (!update_query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    if (db().changes() != 0)
    {
        SqlQuery erase_query(db(), tableQuery(*table, "DELETE FROM %1 WHERE %2=? AND sync_state=? AND sync_dirty=0"));
        if (!addRecordId(*table, record_id, &erase_query))
            return false;
        erase_query.addInt64(SyncStore::SYNC_STATE_DELETED);

        if (!erase_query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            return false;
        }
    }
    else
    {
        SqlQuery mark_query(db(), tableQuery(*table, "INSERT INTO %1 (%2, modify_time, sync_revision, sync_dirty, "
                                                      "sync_state) VALUES (?, ?, ?, 1, ?)"));
        if (!addRecordId(*table, record_id, &mark_query))
            return false;
        mark_query.addInt64(QDateTime::currentSecsSinceEpoch());
        mark_query.addInt64(sync_revision);
        mark_query.addInt64(SyncStore::SYNC_STATE_DELETED);

        if (!mark_query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            return false;
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool SyncTransaction::setReceivedRevision(proto::router::SyncDataType type, qint64 revision)
{
    if (!isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    return SyncStore(database_).setReceivedRevision(type, revision);
}

//--------------------------------------------------------------------------------------------------
bool SyncTransaction::storeSentRecord(
    proto::router::SyncDataType type, const QString& record_id, const QByteArray& data_hash, qint64 sync_dirty)
{
    if (!isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    if (record_id.isEmpty() || data_hash.isEmpty() || sync_dirty <= 0)
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    SqlQuery query(db(), "INSERT OR REPLACE INTO sync_sent_records (type, id, data_hash, sync_dirty) "
                          "VALUES (?, ?, ?, ?)");
    query.addInt64(static_cast<qint64>(type));
    query.addText(record_id);
    query.addBlob(data_hash);
    query.addInt64(sync_dirty);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
Database::FindResult SyncTransaction::findSentRecord(
    proto::router::SyncDataType type, const QString& record_id, const QByteArray& data_hash, qint64* sync_dirty) const
{
    CHECK(sync_dirty);
    *sync_dirty = 0;

    if (!isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return Database::FindResult::FAILED;
    }

    SqlQuery query(db(), "SELECT sync_dirty FROM sync_sent_records WHERE type=? AND id=? AND data_hash=?");
    query.addInt64(static_cast<qint64>(type));
    query.addText(record_id);
    query.addBlob(data_hash);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return Database::FindResult::FAILED;
    }

    if (step == SqlQuery::StepResult::DONE)
        return Database::FindResult::NOT_FOUND;

    *sync_dirty = query.columnInt64(0);
    return Database::FindResult::FOUND;
}

//--------------------------------------------------------------------------------------------------
bool SyncTransaction::eraseSentRecord(
    proto::router::SyncDataType type, const QString& record_id, const QByteArray& data_hash)
{
    if (!isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    SqlQuery query(db(), "DELETE FROM sync_sent_records WHERE type=? AND id=? AND data_hash=?");
    query.addInt64(static_cast<qint64>(type));
    query.addText(record_id);
    query.addBlob(data_hash);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool SyncTransaction::eraseAllSentRecords(proto::router::SyncDataType type, const QString& record_id)
{
    if (!isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    SqlQuery query(db(), "DELETE FROM sync_sent_records WHERE type=? AND id=?");
    query.addInt64(static_cast<qint64>(type));
    query.addText(record_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool SyncTransaction::isActive() const
{
    return transaction_.has_value() && transaction_->isActive();
}

//--------------------------------------------------------------------------------------------------
SyncStore::SyncStore(Database& db)
    : DatabaseStore(db)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
qint64 SyncStore::receivedRevision(proto::router::SyncDataType type) const
{
    const SyncTable* table = findSyncTable(type);
    if (!table)
        return 0;

    return readSetting(table->received_revision_setting).toLongLong();
}

//--------------------------------------------------------------------------------------------------
bool SyncStore::reset()
{
    CHECK(db().isTransactionActive());

    if (!db().exec("DELETE FROM credentials WHERE sync_state!=0") ||
        !db().exec("UPDATE credentials SET sync_revision=0, sync_dirty=1") ||
        !db().exec("DELETE FROM profiles WHERE sync_state!=0"))
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    SqlQuery profiles_query(db(), "UPDATE profiles SET sync_revision=0, sync_dirty=(guid<>? OR modify_time<>0)");
    profiles_query.addText(std::string_view(ProfileConfig::kDefaultGuid));

    if (!profiles_query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    if (!db().exec("DELETE FROM router_hosts WHERE sync_state!=0"))
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    if (!db().exec("UPDATE router_hosts SET sync_revision=0, sync_dirty=1"))
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    if (!db().exec("DELETE FROM local_hosts WHERE sync_state!=0"))
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    if (!db().exec("UPDATE local_hosts SET sync_revision=0, sync_dirty=1"))
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    if (!db().exec("DELETE FROM local_groups WHERE sync_state!=0"))
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    if (!db().exec("UPDATE local_groups SET sync_revision=0, sync_dirty=1"))
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    if (!db().exec("DELETE FROM sync_sent_records"))
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    return clearReceivedRevisions();
}

//--------------------------------------------------------------------------------------------------
bool SyncStore::discard()
{
    CHECK(db().isTransactionActive());

    SqlQuery credentials_query(db(), "UPDATE credentials SET name='', data=X'', sync_revision=0, sync_dirty=0, "
                                      "sync_state=?");
    credentials_query.addInt64(SYNC_STATE_EMPTY);

    if (!credentials_query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    LOG(INFO) << "Credentials discarded:" << db().changes();

    SqlQuery profiles_query(db(), "UPDATE profiles SET name='', data=X'', sync_revision=0, sync_dirty=0, sync_state=? "
                                   "WHERE guid<>?");
    profiles_query.addInt64(SYNC_STATE_EMPTY);
    profiles_query.addText(std::string_view(ProfileConfig::kDefaultGuid));

    if (!profiles_query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    LOG(INFO) << "Profiles discarded:" << db().changes();

    SqlQuery default_query(db(), "UPDATE profiles SET sync_revision=0, sync_dirty=0 WHERE guid=?");
    default_query.addText(std::string_view(ProfileConfig::kDefaultGuid));

    if (!default_query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    if (!db().exec("DELETE FROM router_hosts"))
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    LOG(INFO) << "Router hosts discarded:" << db().changes();

    if (!db().exec("DELETE FROM local_hosts"))
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    LOG(INFO) << "Local hosts discarded:" << db().changes();

    if (!db().exec("DELETE FROM local_groups"))
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    LOG(INFO) << "Local groups discarded:" << db().changes();

    if (!db().exec("DELETE FROM sync_sent_records"))
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    return clearReceivedRevisions();
}

//--------------------------------------------------------------------------------------------------
bool SyncStore::clearReceivedRevisions()
{
    for (const SyncTable& table : kSyncTables)
    {
        if (!setReceivedRevision(table.type, 0))
            return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool SyncStore::setReceivedRevision(proto::router::SyncDataType type, qint64 revision)
{
    CHECK(db().isTransactionActive());

    const SyncTable* table = findSyncTable(type);
    if (!table)
        return false;

    return writeSetting(table->received_revision_setting, QString::number(revision));
}
