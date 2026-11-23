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

#include "client/storage/local_host_store.h"

#include <QDateTime>
#include <QUuid>

#include "base/logging.h"
#include "base/sql/sql_query.h"
#include "client/storage/credential_store.h"
#include "client/storage/sync_store.h"

namespace {

//--------------------------------------------------------------------------------------------------
Database::ReadResult readLocalHost(const SqlQuery& query, LocalHostConfig* host)
{
    host->setId(query.columnInt64(0));
    host->setGroupId(query.columnInt64(1));
    host->setName(query.columnText(2));
    host->setComment(query.columnText(3));
    host->setCreateTime(query.columnInt64(5));
    host->setModifyTime(query.columnInt64(6));
    host->setConnectTime(query.columnInt64(7));
    host->setGuid(query.columnText(8));
    host->setCredentialId(query.columnInt64(9));
    host->setOsCredentialId(query.columnInt64(10));
    host->setProfileId(query.columnInt64(11));

    if (!host->setEncryptedData(query.columnBlob(4)))
    {
        LOG(ERROR) << "Unable to read encrypted data of host" << host->id();
        return Database::ReadResult::INCOMPLETE;
    }

    return Database::ReadResult::OK;
}

//--------------------------------------------------------------------------------------------------
bool removeHost(SqlDatabase& db, qint64 entry_id)
{
    SqlQuery remove_query(db, "DELETE FROM local_hosts WHERE id=? AND sync_revision=0");
    remove_query.addInt64(entry_id);

    if (!remove_query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db.lastError();
        return false;
    }

    SqlQuery mark_query(db, "UPDATE local_hosts SET group_id=NULL, name='', comment='', data=X'', "
                             "credential_id=NULL, os_credential_id=NULL, profile_id=NULL, modify_time=?, "
                             "sync_dirty=sync_dirty+1, sync_state=? "
                             "WHERE id=? AND sync_revision!=0 AND sync_state=0");
    mark_query.addInt64(QDateTime::currentSecsSinceEpoch());
    mark_query.addInt64(SyncStore::SYNC_STATE_DELETED);
    mark_query.addInt64(entry_id);

    if (!mark_query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db.lastError();
        return false;
    }

    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
LocalHostStore::LocalHostStore(Database& db)
    : DatabaseStore(db)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
Database::ReadResult LocalHostStore::list(qint64 group_id, QList<LocalHostConfig>* hosts) const
{
    CHECK(hosts);
    hosts->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return Database::ReadResult::FAILED;
    }

    SqlQuery query(db(), "SELECT id, IFNULL(group_id, 0), name, comment, data, "
                          "create_time, modify_time, connect_time, guid, IFNULL(credential_id, 0), "
                          "IFNULL(os_credential_id, 0), IFNULL(profile_id, 0) "
                          "FROM local_hosts WHERE group_id IS NULLIF(?, 0) AND sync_state=0 "
                          "ORDER BY casefold(name)");
    query.addInt64(group_id);

    Database::ReadResult result = Database::ReadResult::OK;

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            // A failed step must not pass for the end of the rows: a caller may treat what is
            // missing from the list as deleted.
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            return Database::ReadResult::FAILED;
        }
        if (step == SqlQuery::StepResult::DONE)
            break;

        LocalHostConfig host;
        if (readLocalHost(query, &host) != Database::ReadResult::OK)
            result = Database::ReadResult::INCOMPLETE;

        hosts->append(host);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
Database::ReadResult LocalHostStore::all(Scope scope, QList<LocalHostConfig>* hosts) const
{
    CHECK(hosts);
    hosts->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return Database::ReadResult::FAILED;
    }

    SqlQuery query(db(), "SELECT id, IFNULL(group_id, 0), name, comment, data, "
                          "create_time, modify_time, connect_time, guid, IFNULL(credential_id, 0), "
                          "IFNULL(os_credential_id, 0), IFNULL(profile_id, 0) "
                          "FROM local_hosts WHERE sync_state=0 OR sync_state=?");
    query.addInt64(scope == Scope::WITH_WAITING ? SyncStore::SYNC_STATE_WAITING : SyncStore::SYNC_STATE_PRESENT);

    Database::ReadResult result = Database::ReadResult::OK;

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            // A failed step must not pass for the end of the rows: a caller may treat what is
            // missing from the list as deleted.
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            return Database::ReadResult::FAILED;
        }
        if (step == SqlQuery::StepResult::DONE)
            break;

        LocalHostConfig host;
        if (readLocalHost(query, &host) != Database::ReadResult::OK)
            result = Database::ReadResult::INCOMPLETE;

        hosts->append(host);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
bool LocalHostStore::add(LocalHostConfig& host)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (!host.isValid())
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    std::optional<QByteArray> encrypted_data = host.encryptedData();
    if (!encrypted_data.has_value())
        return false;

    const qint64 current_time = QDateTime::currentSecsSinceEpoch();
    host.setCreateTime(current_time);
    host.setModifyTime(current_time);
    host.setConnectTime(0);

    if (host.guid().isEmpty())
        host.setGuid(QUuid::createUuid().toString(QUuid::WithoutBraces));

    SqlQuery query(db(), "INSERT INTO local_hosts (id, group_id, name, comment, data, create_time, "
                          "modify_time, connect_time, guid, credential_id, os_credential_id, profile_id, "
                          "sync_dirty) VALUES (NULL, NULLIF(?, 0), ?, ?, ?, ?, ?, ?, ?, NULLIF(?, 0), "
                          "NULLIF(?, 0), NULLIF(?, 0), 1)");
    query.addInt64(host.groupId());
    query.addText(host.name());
    query.addText(host.comment());
    query.addBlob(*encrypted_data);
    query.addInt64(host.createTime());
    query.addInt64(host.modifyTime());
    query.addInt64(host.connectTime());
    query.addText(host.guid());
    query.addInt64(host.credentialId());
    query.addInt64(host.osCredentialId());
    query.addInt64(host.profileId());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    host.setId(db().lastInsertRowId());
    return true;
}

//--------------------------------------------------------------------------------------------------
bool LocalHostStore::modify(LocalHostConfig& host)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (!host.isValid())
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    SqlTransaction transaction(db());
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction";
        return false;
    }

    SqlQuery row(db(), "SELECT data FROM local_hosts WHERE id=? AND sync_state=0");
    row.addInt64(host.id());

    const SqlQuery::StepResult step = row.next();
    if (step != SqlQuery::StepResult::ROW)
    {
        if (step == SqlQuery::StepResult::FAILED)
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
        else
            LOG(ERROR) << "Local host" << host.id() << "not found";
        return false;
    }

    LocalHostConfig stored;
    if (!stored.setEncryptedData(row.columnBlob(0)))
        LOG(WARNING) << "Stored data of host" << host.id() << "could not be read and is replaced";

    LocalHostConfig modified = host;
    modified.setUnknownData(stored.unknownData());

    std::optional<QByteArray> encrypted_data = modified.encryptedData();
    if (!encrypted_data.has_value())
        return false;

    host.setModifyTime(QDateTime::currentSecsSinceEpoch());

    SqlQuery query(db(), "UPDATE local_hosts SET group_id=NULLIF(?, 0), name=?, comment=?, "
                          "data=?, modify_time=?, credential_id=NULLIF(?, 0), profile_id=NULLIF(?, 0), "
                          "sync_dirty=sync_dirty+1 WHERE id=? AND sync_state=0");
    query.addInt64(host.groupId());
    query.addText(host.name());
    query.addText(host.comment());
    query.addBlob(*encrypted_data);
    query.addInt64(host.modifyTime());
    query.addInt64(host.credentialId());
    query.addInt64(host.profileId());
    query.addInt64(host.id());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool LocalHostStore::move(qint64 entry_id, qint64 new_group_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db(), "UPDATE local_hosts SET group_id=NULLIF(?, 0), modify_time=?, sync_dirty=sync_dirty+1 "
                          "WHERE id=? AND sync_state=0");
    query.addInt64(new_group_id);
    query.addInt64(QDateTime::currentSecsSinceEpoch());
    query.addInt64(entry_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    if (db().changes() == 0)
    {
        LOG(ERROR) << "Local host" << entry_id << "not found";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool LocalHostStore::remove(qint64 entry_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlTransaction transaction(db());
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction";
        return false;
    }

    if (!removeHost(db(), entry_id))
        return false;

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool LocalHostStore::setConnectTime(qint64 entry_id, qint64 connect_time)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db(), "UPDATE local_hosts SET connect_time=? WHERE id=? AND sync_state=0");
    query.addInt64(connect_time);
    query.addInt64(entry_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool LocalHostStore::setOsCredentialId(qint64 entry_id, qint64 credential_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db(), "UPDATE local_hosts SET os_credential_id=NULLIF(?, 0), sync_dirty=sync_dirty+1 "
                          "WHERE id=? AND sync_state=0");
    query.addInt64(credential_id);
    query.addInt64(entry_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
Database::FindResult LocalHostStore::find(qint64 entry_id, LocalHostConfig* host) const
{
    CHECK(host);
    *host = LocalHostConfig();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return Database::FindResult::FAILED;
    }

    SqlQuery query(db(), "SELECT id, IFNULL(group_id, 0), name, comment, data, "
                          "create_time, modify_time, connect_time, guid, IFNULL(credential_id, 0), "
                          "IFNULL(os_credential_id, 0), IFNULL(profile_id, 0) "
                          "FROM local_hosts WHERE id=? AND sync_state=0");
    query.addInt64(entry_id);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return Database::FindResult::FAILED;
    }

    if (step == SqlQuery::StepResult::DONE)
        return Database::FindResult::NOT_FOUND;

    if (readLocalHost(query, host) != Database::ReadResult::OK)
    {
        LOG(ERROR) << "Unable to read the data of host" << entry_id;
        return Database::FindResult::UNREADABLE;
    }

    return Database::FindResult::FOUND;
}

//--------------------------------------------------------------------------------------------------
std::optional<std::pair<QString, SecureString>> LocalHostStore::credentials(qint64 entry_id) const
{
    LocalHostConfig host;
    if (find(entry_id, &host) != Database::FindResult::FOUND)
        return std::nullopt;

    if (host.credentialId() <= 0)
        return std::make_pair(host.username(), host.password());

    CredentialConfig credential;
    const Database::FindResult found = CredentialStore(database_).find(host.credentialId(), &credential);
    if (found == Database::FindResult::NOT_FOUND)
        return std::make_pair(host.username(), host.password());

    if (found != Database::FindResult::FOUND)
        return std::nullopt;

    return std::make_pair(credential.username(), credential.password());
}

//--------------------------------------------------------------------------------------------------
Database::ReadResult LocalHostStore::search(const QString& query_text, QList<LocalHostConfig>* hosts) const
{
    CHECK(hosts);
    hosts->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return Database::ReadResult::FAILED;
    }

    SqlQuery query(db(), "SELECT id, IFNULL(group_id, 0), name, comment, data, "
                          "create_time, modify_time, connect_time, guid, IFNULL(credential_id, 0), "
                          "IFNULL(os_credential_id, 0), IFNULL(profile_id, 0) "
                          "FROM local_hosts WHERE sync_state=0 ORDER BY casefold(name)");

    Database::ReadResult result = Database::ReadResult::OK;

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            return Database::ReadResult::FAILED;
        }
        if (step == SqlQuery::StepResult::DONE)
            break;

        LocalHostConfig host;
        const Database::ReadResult host_result = readLocalHost(query, &host);

        if (host.name().contains(query_text, Qt::CaseInsensitive) ||
            host.address().contains(query_text, Qt::CaseInsensitive) ||
            host.comment().contains(query_text, Qt::CaseInsensitive))
        {
            if (host_result != Database::ReadResult::OK)
                result = Database::ReadResult::INCOMPLETE;

            hosts->append(host);
        }
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
Database::ReadResult LocalHostStore::unsynced(QList<Unsynced>* hosts) const
{
    CHECK(hosts);
    hosts->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return Database::ReadResult::FAILED;
    }

    SqlQuery query(db(), "SELECT id, IFNULL(group_id, 0), name, comment, data, "
                          "create_time, modify_time, connect_time, guid, IFNULL(credential_id, 0), "
                          "IFNULL(os_credential_id, 0), IFNULL(profile_id, 0), sync_revision, sync_dirty, sync_state "
                          "FROM local_hosts WHERE sync_dirty!=0");

    Database::ReadResult result = Database::ReadResult::OK;

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            return Database::ReadResult::FAILED;
        }
        if (step == SqlQuery::StepResult::DONE)
            break;

        Unsynced& unsynced = hosts->emplaceBack();
        unsynced.deleted = query.columnInt64(14) == SyncStore::SYNC_STATE_DELETED;
        unsynced.sync_revision = query.columnInt64(12);
        unsynced.sync_dirty = query.columnInt64(13);

        if (unsynced.deleted)
        {
            unsynced.host.setId(query.columnInt64(0));
            unsynced.host.setGuid(query.columnText(8));
            unsynced.host.setModifyTime(query.columnInt64(6));
        }
        else if (readLocalHost(query, &unsynced.host) != Database::ReadResult::OK)
        {
            unsynced.damaged = true;
            result = Database::ReadResult::INCOMPLETE;
        }
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
bool LocalHostStore::storeReceived(
    SyncTransaction& transaction, const LocalHostConfig& host, qint64 sync_revision)
{
    if (!transaction.isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    if (!host.isValid() || host.guid().isEmpty() || sync_revision <= 0)
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    std::optional<QByteArray> encrypted_data = host.encryptedData();
    if (!encrypted_data.has_value())
        return false;

    qint64 entry_id = 0;
    qint64 stored_revision = 0;

    {
        SqlQuery row(db(), "SELECT id, sync_revision FROM local_hosts WHERE guid=?");
        row.addText(host.guid());

        const SqlQuery::StepResult step = row.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            return false;
        }

        if (step == SqlQuery::StepResult::ROW)
        {
            entry_id = row.columnInt64(0);
            stored_revision = row.columnInt64(1);
        }
    }

    if (entry_id > 0 && stored_revision == sync_revision)
        return true;

    qint64 group_state = SyncStore::SYNC_STATE_PRESENT;

    if (host.groupId() > 0)
    {
        SqlQuery group_query(db(), "SELECT sync_state FROM local_groups WHERE id=?");
        group_query.addInt64(host.groupId());

        if (group_query.next() != SqlQuery::StepResult::ROW)
        {
            LOG(ERROR) << "Unable to read group" << host.groupId() << "of the received host:" << db().lastError();
            return false;
        }

        group_state = group_query.columnInt64(0);
    }

    // A host whose group has not come, or waits itself, is not in the tree yet.
    const bool waits = group_state == SyncStore::SYNC_STATE_EMPTY || group_state == SyncStore::SYNC_STATE_WAITING;
    const SyncStore::SyncState state = waits ? SyncStore::SYNC_STATE_WAITING : SyncStore::SYNC_STATE_PRESENT;

    if (entry_id <= 0)
    {
        SqlQuery query(db(), "INSERT INTO local_hosts (id, group_id, name, comment, data, create_time, "
                              "modify_time, guid, credential_id, os_credential_id, profile_id, sync_revision, "
                              "sync_state) VALUES (NULL, NULLIF(?, 0), ?, ?, ?, ?, ?, ?, NULLIF(?, 0), "
                              "NULLIF(?, 0), NULLIF(?, 0), ?, ?)");
        query.addInt64(host.groupId());
        query.addText(host.name());
        query.addText(host.comment());
        query.addBlob(*encrypted_data);
        query.addInt64(host.createTime());
        query.addInt64(host.modifyTime());
        query.addText(host.guid());
        query.addInt64(host.credentialId());
        query.addInt64(host.osCredentialId());
        query.addInt64(host.profileId());
        query.addInt64(sync_revision);
        query.addInt64(state);

        if (!query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            return false;
        }

        entry_id = db().lastInsertRowId();
    }
    else
    {
        SqlQuery query(db(), "UPDATE local_hosts SET group_id=NULLIF(?, 0), name=?, comment=?, data=?, "
                              "create_time=?, modify_time=?, credential_id=NULLIF(?, 0), "
                              "os_credential_id=NULLIF(?, 0), profile_id=NULLIF(?, 0), sync_revision=?, "
                              "sync_dirty=0, sync_state=? WHERE id=?");
        query.addInt64(host.groupId());
        query.addText(host.name());
        query.addText(host.comment());
        query.addBlob(*encrypted_data);
        query.addInt64(host.createTime());
        query.addInt64(host.modifyTime());
        query.addInt64(host.credentialId());
        query.addInt64(host.osCredentialId());
        query.addInt64(host.profileId());
        query.addInt64(sync_revision);
        query.addInt64(state);
        query.addInt64(entry_id);

        if (!query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            return false;
        }
    }

    if (group_state == SyncStore::SYNC_STATE_DELETED)
    {
        LOG(INFO) << "Received host" << entry_id << "lies in a group removed on the device and is removed too";
        return removeHost(db(), entry_id);
    }

    if (waits)
        LOG(INFO) << "Received host" << entry_id << "waits for group" << host.groupId();

    return true;
}
