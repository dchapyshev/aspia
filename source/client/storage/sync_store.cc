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

namespace {

struct SyncTable
{
    proto::router::SyncDataType type;
    const char* name;
    const char* received_revision_setting;
};

const SyncTable kSyncTables[] =
{
    { proto::router::SYNC_DATA_TYPE_CREDENTIALS, "credentials", "received_credentials_revision" },
    { proto::router::SYNC_DATA_TYPE_PROFILES, "profiles", "received_profiles_revision" }
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
    return QString(query).arg(table.name).toStdString();
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
    transaction_.emplace(sql());

    if (!transaction_->begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction";
        transaction_.reset();
        return Database::SyncResult::FAILED;
    }

    RouterConfig router;
    const Database::FindResult found = db_.routerConfig(&router);
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
bool SyncTransaction::eraseRecord(proto::router::SyncDataType type, const QString& guid)
{
    if (!isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    const SyncTable* table = findSyncTable(type);
    if (!table)
        return false;

    SqlQuery query(sql(), tableQuery(*table, "DELETE FROM %1 WHERE guid=?"));
    query.addText(guid);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool SyncTransaction::setRecordSyncRevision(
    proto::router::SyncDataType type, const QString& guid, qint64 sync_revision)
{
    if (!isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    const SyncTable* table = findSyncTable(type);
    if (!table)
        return false;

    SqlQuery query(sql(), tableQuery(*table, "UPDATE %1 SET sync_revision=? WHERE guid=?"));
    query.addInt64(sync_revision);
    query.addText(guid);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool SyncTransaction::confirmSentRecord(
    proto::router::SyncDataType type, const QString& guid, qint64 sync_revision, qint64 sync_dirty)
{
    if (!isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    const SyncTable* table = findSyncTable(type);
    if (!table)
        return false;

    if (guid.isEmpty() || sync_revision <= 0 || sync_dirty < 0)
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    SqlQuery update_query(sql(), tableQuery(*table, "UPDATE %1 SET sync_revision=?, sync_dirty=sync_dirty-? "
                                                    "WHERE guid=?"));
    update_query.addInt64(sync_revision);
    update_query.addInt64(sync_dirty);
    update_query.addText(guid);

    if (!update_query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    if (sql().changes() != 0)
    {
        SqlQuery erase_query(sql(), tableQuery(*table, "DELETE FROM %1 WHERE guid=? AND deleted!=0 AND sync_dirty=0"));
        erase_query.addText(guid);

        if (!erase_query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << sql().lastError();
            return false;
        }
    }
    else
    {
        SqlQuery mark_query(sql(), tableQuery(*table, "INSERT INTO %1 (id, guid, modify_time, sync_revision, "
                                                      "sync_dirty, deleted) VALUES (NULL, ?, ?, ?, 1, 1)"));
        mark_query.addText(guid);
        mark_query.addInt64(QDateTime::currentSecsSinceEpoch());
        mark_query.addInt64(sync_revision);

        if (!mark_query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << sql().lastError();
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

    return SyncStore(db_).setReceivedRevision(type, revision);
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
    CHECK(sql().isTransactionActive());

    if (!sql().exec("DELETE FROM credentials WHERE deleted!=0") ||
        !sql().exec("UPDATE credentials SET sync_revision=0, sync_dirty=1") ||
        !sql().exec("DELETE FROM profiles WHERE deleted!=0"))
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    SqlQuery profiles_query(sql(), "UPDATE profiles SET sync_revision=0, sync_dirty=(guid<>? OR modify_time<>0)");
    profiles_query.addText(std::string_view(ProfileConfig::kDefaultGuid));

    if (!profiles_query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    return clearReceivedRevisions();
}

//--------------------------------------------------------------------------------------------------
bool SyncStore::discard()
{
    CHECK(sql().isTransactionActive());

    if (!sql().exec("UPDATE credentials SET name='', data=X'', sync_revision=0, sync_dirty=0, deleted=1"))
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    LOG(INFO) << "Credentials discarded:" << sql().changes();

    SqlQuery profiles_query(sql(), "UPDATE profiles SET name='', data=X'', sync_revision=0, sync_dirty=0, deleted=1 "
                                   "WHERE guid<>?");
    profiles_query.addText(std::string_view(ProfileConfig::kDefaultGuid));

    if (!profiles_query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    LOG(INFO) << "Profiles discarded:" << sql().changes();

    SqlQuery default_query(sql(), "UPDATE profiles SET sync_revision=0, sync_dirty=0 WHERE guid=?");
    default_query.addText(std::string_view(ProfileConfig::kDefaultGuid));

    if (!default_query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
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
    CHECK(sql().isTransactionActive());

    const SyncTable* table = findSyncTable(type);
    if (!table)
        return false;

    return writeSetting(table->received_revision_setting, QString::number(revision));
}
