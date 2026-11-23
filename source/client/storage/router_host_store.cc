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

#include "client/storage/router_host_store.h"

#include <QDateTime>

#include "base/logging.h"
#include "base/sql/sql_query.h"
#include "client/storage/credential_store.h"
#include "client/storage/sync_store.h"

namespace {

// The rows one pass takes. Every one of them costs a request to the router, so the list is walked
// over as many passes as it needs instead of at once.
const int kMaxRouterHostsToCheck = 10;

// How long the answer of the router is trusted: a row checked less than a week ago is not offered
// for checking again.
const qint64 kRouterHostRecheckInterval = 7 * 24 * 60 * 60;

//--------------------------------------------------------------------------------------------------
Database::ReadResult readRouterHost(const SqlQuery& query, RouterHostConfig* host)
{
    host->setHostId(query.columnUInt64(0));
    host->setCredentialId(query.columnInt64(2));
    host->setOsCredentialId(query.columnInt64(3));
    host->setProfileId(query.columnInt64(4));
    host->setModifyTime(query.columnInt64(5));

    if (!host->setEncryptedData(query.columnBlob(1)))
    {
        LOG(ERROR) << "Unable to read encrypted data of router host" << host->hostId();
        return Database::ReadResult::INCOMPLETE;
    }

    return Database::ReadResult::OK;
}

} // namespace

//--------------------------------------------------------------------------------------------------
RouterHostStore::RouterHostStore(Database& db)
    : DatabaseStore(db)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
Database::ReadResult RouterHostStore::list(QList<RouterHostConfig>* hosts) const
{
    CHECK(hosts);
    hosts->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return Database::ReadResult::FAILED;
    }

    SqlQuery query(db(), "SELECT host_id, data, IFNULL(credential_id, 0), IFNULL(os_credential_id, 0), "
                          "IFNULL(profile_id, 0), modify_time "
                          "FROM router_hosts WHERE sync_state=0");

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

        RouterHostConfig host;
        if (readRouterHost(query, &host) != Database::ReadResult::OK)
            result = Database::ReadResult::INCOMPLETE;

        hosts->append(host);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
bool RouterHostStore::add(const RouterHostConfig& host)
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

    {
        SqlQuery restore_query(db(), "UPDATE router_hosts SET data=?, credential_id=NULLIF(?, 0), "
                                      "os_credential_id=NULLIF(?, 0), profile_id=NULLIF(?, 0), modify_time=?, "
                                      "sync_dirty=sync_dirty+1, sync_state=0 WHERE host_id=? AND sync_state!=0");
        restore_query.addBlob(*encrypted_data);
        restore_query.addInt64(host.credentialId());
        restore_query.addInt64(host.osCredentialId());
        restore_query.addInt64(host.profileId());
        restore_query.addInt64(QDateTime::currentSecsSinceEpoch());
        restore_query.addUInt64(host.hostId());

        if (!restore_query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            return false;
        }

        if (db().changes() != 0)
            return true;
    }

    SqlQuery query(db(),
        "INSERT INTO router_hosts (host_id, data, credential_id, os_credential_id, profile_id, modify_time, "
        "sync_dirty) VALUES (?, ?, NULLIF(?, 0), NULLIF(?, 0), NULLIF(?, 0), ?, 1)");
    query.addUInt64(host.hostId());
    query.addBlob(*encrypted_data);
    query.addInt64(host.credentialId());
    query.addInt64(host.osCredentialId());
    query.addInt64(host.profileId());
    query.addInt64(QDateTime::currentSecsSinceEpoch());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool RouterHostStore::modify(const RouterHostConfig& host)
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

    SqlQuery row(db(), "SELECT data FROM router_hosts WHERE host_id=? AND sync_state=0");
    row.addUInt64(host.hostId());

    const SqlQuery::StepResult step = row.next();
    if (step != SqlQuery::StepResult::ROW)
    {
        if (step == SqlQuery::StepResult::FAILED)
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
        else
            LOG(ERROR) << "Credentials of router host" << host.hostId() << "not found";
        return false;
    }

    RouterHostConfig stored;
    stored.setHostId(host.hostId());

    if (!stored.setEncryptedData(row.columnBlob(0)))
        LOG(WARNING) << "Stored data of router host" << host.hostId() << "could not be read and is replaced";

    RouterHostConfig modified = host;
    modified.setUnknownData(stored.unknownData());

    std::optional<QByteArray> encrypted_data = modified.encryptedData();
    if (!encrypted_data.has_value())
        return false;

    SqlQuery query(db(), "UPDATE router_hosts SET data=?, credential_id=NULLIF(?, 0), modify_time=?, "
                          "sync_dirty=sync_dirty+1 WHERE host_id=?");
    query.addBlob(*encrypted_data);
    query.addInt64(host.credentialId());
    query.addInt64(QDateTime::currentSecsSinceEpoch());
    query.addUInt64(host.hostId());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    return transaction.commit();
}

//--------------------------------------------------------------------------------------------------
bool RouterHostStore::remove(HostId host_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db(), "UPDATE router_hosts SET data=X'', credential_id=NULL, os_credential_id=NULL, "
                          "profile_id=NULL WHERE host_id=? AND sync_state=0");
    query.addUInt64(host_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
Database::FindResult RouterHostStore::find(HostId host_id, RouterHostConfig* host) const
{
    CHECK(host);
    *host = RouterHostConfig();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return Database::FindResult::FAILED;
    }

    SqlQuery query(db(), "SELECT host_id, data, IFNULL(credential_id, 0), IFNULL(os_credential_id, 0), "
                          "IFNULL(profile_id, 0), modify_time "
                          "FROM router_hosts WHERE host_id=? AND sync_state=0");
    query.addUInt64(host_id);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return Database::FindResult::FAILED;
    }

    if (step == SqlQuery::StepResult::DONE)
        return Database::FindResult::NOT_FOUND;

    if (readRouterHost(query, host) != Database::ReadResult::OK)
    {
        LOG(ERROR) << "Unable to read the credentials of host" << host_id;
        return Database::FindResult::UNREADABLE;
    }

    return Database::FindResult::FOUND;
}

//--------------------------------------------------------------------------------------------------
std::optional<std::pair<QString, SecureString>> RouterHostStore::credentials(HostId host_id) const
{
    RouterHostConfig host;
    if (find(host_id, &host) != Database::FindResult::FOUND)
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
bool RouterHostStore::outdated(QList<HostId>* hosts) const
{
    CHECK(hosts);
    hosts->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    // The rows checked longest ago come first, so repeated calls walk the whole list instead of
    // returning to the same rows.
    SqlQuery query(db(), "SELECT host_id FROM router_hosts WHERE check_time<? AND sync_state=0 "
                          "ORDER BY check_time LIMIT ?");
    query.addInt64(QDateTime::currentSecsSinceEpoch() - kRouterHostRecheckInterval);
    query.addInt64(kMaxRouterHostsToCheck);

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            return false;
        }
        if (step == SqlQuery::StepResult::DONE)
            break;

        hosts->append(query.columnUInt64(0));
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool RouterHostStore::updateCheckTime(HostId host_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db(), "UPDATE router_hosts SET check_time=? WHERE host_id=? AND sync_state=0");
    query.addInt64(QDateTime::currentSecsSinceEpoch());
    query.addUInt64(host_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    // An update of a row that is not there changes nothing and reports no error of its own.
    if (db().changes() == 0)
    {
        LOG(ERROR) << "Credentials of router host" << host_id << "not found";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool RouterHostStore::setOsCredentialId(HostId host_id, qint64 credential_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db(), "UPDATE router_hosts SET os_credential_id=NULLIF(?, 0), modify_time=?, "
                          "sync_dirty=sync_dirty+1 WHERE host_id=? AND sync_state=0");
    query.addInt64(credential_id);
    query.addInt64(QDateTime::currentSecsSinceEpoch());
    query.addUInt64(host_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool RouterHostStore::setProfileId(HostId host_id, qint64 profile_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (profile_id <= 0)
    {
        SqlQuery query(db(), "UPDATE router_hosts SET profile_id=NULL, modify_time=?, sync_dirty=sync_dirty+1 "
                              "WHERE host_id=? AND sync_state=0");
        query.addInt64(QDateTime::currentSecsSinceEpoch());
        query.addUInt64(host_id);

        if (!query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            return false;
        }

        return true;
    }

    if (host_id == kInvalidHostId || isTempHostId(host_id))
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

    const qint64 modify_time = QDateTime::currentSecsSinceEpoch();

    SqlQuery update_query(db(), "UPDATE router_hosts SET profile_id=?, modify_time=?, sync_dirty=sync_dirty+1, "
                                 "sync_state=0 WHERE host_id=?");
    update_query.addInt64(profile_id);
    update_query.addInt64(modify_time);
    update_query.addUInt64(host_id);

    if (!update_query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db().lastError();
        return false;
    }

    if (db().changes() == 0)
    {
        SqlQuery insert_query(db(), "INSERT INTO router_hosts (host_id, profile_id, modify_time, sync_dirty) "
                                     "VALUES (?, ?, ?, 1)");
        insert_query.addUInt64(host_id);
        insert_query.addInt64(profile_id);
        insert_query.addInt64(modify_time);

        if (!insert_query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            return false;
        }
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
Database::ReadResult RouterHostStore::unsynced(QList<Unsynced>* hosts) const
{
    CHECK(hosts);
    hosts->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return Database::ReadResult::FAILED;
    }

    SqlQuery query(db(), "SELECT host_id, data, IFNULL(credential_id, 0), IFNULL(os_credential_id, 0), "
                          "IFNULL(profile_id, 0), modify_time, sync_revision, sync_dirty, sync_state "
                          "FROM router_hosts WHERE sync_dirty!=0");

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
        unsynced.deleted = query.columnInt64(8) == SyncStore::SYNC_STATE_DELETED;
        unsynced.sync_revision = query.columnInt64(6);
        unsynced.sync_dirty = query.columnInt64(7);

        if (unsynced.deleted)
        {
            unsynced.host.setHostId(query.columnUInt64(0));
            unsynced.host.setModifyTime(query.columnInt64(5));
        }
        else if (readRouterHost(query, &unsynced.host) != Database::ReadResult::OK)
        {
            unsynced.damaged = true;
            result = Database::ReadResult::INCOMPLETE;
        }
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
bool RouterHostStore::storeReceived(
    SyncTransaction& transaction, const RouterHostConfig& host, qint64 sync_revision)
{
    if (!transaction.isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    if (!host.isValid() || sync_revision <= 0)
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    std::optional<QByteArray> encrypted_data = host.encryptedData();
    if (!encrypted_data.has_value())
        return false;

    if (encrypted_data->isEmpty() && host.credentialId() <= 0 && host.osCredentialId() <= 0 &&
        host.profileId() <= 0)
    {
        LOG(ERROR) << "Received router host" << host.hostId() << "holds nothing";
        return false;
    }

    std::optional<qint64> stored_revision;

    {
        SqlQuery row(db(), "SELECT sync_revision FROM router_hosts WHERE host_id=?");
        row.addUInt64(host.hostId());

        const SqlQuery::StepResult step = row.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            return false;
        }

        if (step == SqlQuery::StepResult::ROW)
            stored_revision = row.columnInt64(0);
    }

    if (!stored_revision.has_value())
    {
        SqlQuery query(db(), "INSERT INTO router_hosts (host_id, data, credential_id, os_credential_id, "
                              "profile_id, modify_time, sync_revision) "
                              "VALUES (?, ?, NULLIF(?, 0), NULLIF(?, 0), NULLIF(?, 0), ?, ?)");
        query.addUInt64(host.hostId());
        query.addBlob(*encrypted_data);
        query.addInt64(host.credentialId());
        query.addInt64(host.osCredentialId());
        query.addInt64(host.profileId());
        query.addInt64(host.modifyTime());
        query.addInt64(sync_revision);

        if (!query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            return false;
        }
    }
    else if (*stored_revision != sync_revision)
    {
        SqlQuery query(db(), "UPDATE router_hosts SET data=?, credential_id=NULLIF(?, 0), "
                              "os_credential_id=NULLIF(?, 0), profile_id=NULLIF(?, 0), modify_time=?, "
                              "sync_revision=?, sync_dirty=0, sync_state=0 WHERE host_id=?");
        query.addBlob(*encrypted_data);
        query.addInt64(host.credentialId());
        query.addInt64(host.osCredentialId());
        query.addInt64(host.profileId());
        query.addInt64(host.modifyTime());
        query.addInt64(sync_revision);
        query.addUInt64(host.hostId());

        if (!query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db().lastError();
            return false;
        }
    }

    return true;
}
