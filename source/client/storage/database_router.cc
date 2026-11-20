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

#include "client/storage/database.h"

#include <QDateTime>

#include "base/logging.h"
#include "base/peer/host_id.h"
#include "base/sql/sql_query.h"
#include "base/sql/sql_transaction.h"
#include "proto/router.h"

namespace {

const char kSettingRouter[] = "router";

// The rows one pass takes. Every one of them costs a request to the router, so the list is walked
// over as many passes as it needs instead of at once.
const int kMaxRouterHostsToCheck = 10;

// How long the answer of the router is trusted: a row checked less than a week ago is not offered
// for checking again.
const qint64 kRouterHostRecheckInterval = 7 * 24 * 60 * 60;

//--------------------------------------------------------------------------------------------------
Database::ReadResult readRouter(const SqlQuery& query, RouterConfig* router)
{
    const bool opened = router->setEncryptedData(QByteArray::fromBase64(query.columnText(0).toLatin1()));

#if defined(Q_OS_ANDROID)
    router->setSessionType(proto::router::SESSION_TYPE_OPERATOR);
#endif

    if (!opened)
    {
        LOG(ERROR) << "Unable to read encrypted data of router";
        return Database::ReadResult::INCOMPLETE;
    }

    return Database::ReadResult::OK;
}

//--------------------------------------------------------------------------------------------------
Database::ReadResult readRouterHost(const SqlQuery& query, RouterHostConfig* host)
{
    host->setHostId(query.columnUInt64(0));
    host->setCredentialId(query.columnInt64(2));
    host->setOsCredentialId(query.columnInt64(3));
    host->setProfileId(query.columnInt64(4));

    if (!host->setEncryptedData(query.columnBlob(1)))
    {
        LOG(ERROR) << "Unable to read encrypted data of router host" << host->hostId();
        return Database::ReadResult::INCOMPLETE;
    }

    return Database::ReadResult::OK;
}

} // namespace

//--------------------------------------------------------------------------------------------------
Database::FindResult Database::routerConfig(RouterConfig* router) const
{
    CHECK(router);
    *router = RouterConfig();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return FindResult::FAILED;
    }

    SqlQuery query(db_, "SELECT value FROM settings WHERE name=?");
    query.addText(std::string_view(kSettingRouter));

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return FindResult::FAILED;
    }

    if (step == SqlQuery::StepResult::DONE)
        return FindResult::NOT_FOUND;

    router->setGuid(guid());

    if (readRouter(query, router) != ReadResult::OK)
    {
        LOG(ERROR) << "Unable to read the data of router";
        return FindResult::UNREADABLE;
    }

    return FindResult::FOUND;
}

//--------------------------------------------------------------------------------------------------
bool Database::setRouterConfig(const RouterConfig& router)
{
    SqlTransaction transaction(db_);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction";
        return false;
    }

    RouterConfig stored;
    if (routerConfig(&stored) == FindResult::FAILED)
        return false;

    RouterConfig config = router;

    if (config.userGuid().isEmpty() && (!stored.isSyncConfirmationNeeded(config) || stored.hasSameAccount(config)))
        config.setUserGuid(stored.userGuid());

    if (config.isSyncEnabled())
    {
        config.setSyncStorageId(stored.syncStorageId());
        config.setSyncStorageKey(stored.syncStorageKey());
        config.setSyncStorageResetPending(stored.isSyncStorageResetPending());
    }
    else
    {
        config.setSyncStorageId(QByteArray());
        config.setSyncStorageKey(SecureByteArray());
        config.setSyncStorageResetPending(false);

        if (!stored.syncStorageId().isEmpty())
        {
            LOG(INFO) << "The device is detached from the storage" << stored.syncStorageId().toHex();

            if (!resetSyncState())
            {
                LOG(ERROR) << "Unable to detach the records from the storage";
                return false;
            }
        }
    }

    if (!writeRouter(config))
        return false;

    return transaction.commit();
}

//--------------------------------------------------------------------------------------------------
Database::SyncResult Database::setSyncStorage(
    const RouterConfig& router, const QByteArray& storage_id, const SecureByteArray& key, bool reset)
{
    SqlTransaction transaction(db_);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction";
        return SyncResult::FAILED;
    }

    RouterConfig stored;
    const FindResult found = routerConfig(&stored);
    if (found == FindResult::NOT_FOUND)
    {
        LOG(WARNING) << "The router is not saved";
        return SyncResult::DETACHED;
    }

    if (found != FindResult::FOUND)
    {
        LOG(ERROR) << "Unable to read the router";
        return SyncResult::FAILED;
    }

    if (!stored.hasSameAccount(router))
    {
        LOG(WARNING) << "The router is saved for another account";
        return SyncResult::DETACHED;
    }

    if (!stored.isSyncEnabled())
    {
        LOG(WARNING) << "The synchronization is turned off";
        return SyncResult::DETACHED;
    }

    const bool another_storage = stored.syncStorageId() != storage_id;

    if (another_storage)
    {
        const bool discard = reset && !stored.syncStorageId().isEmpty() && !storage_id.isEmpty() &&
            !stored.isSyncStorageResetPending();

        LOG(INFO) << "Storage" << stored.syncStorageId().toHex() << "is replaced with" << storage_id.toHex()
                  << "reset:" << reset << "reset pending:" << stored.isSyncStorageResetPending()
                  << "discard:" << discard;

        if (!(discard ? discardSyncData() : resetSyncState()))
        {
            LOG(ERROR) << "Unable to prepare the records for another storage";
            return SyncResult::FAILED;
        }
    }

    const bool reset_finished = router.isSyncStorageResetPending() && stored.isSyncStorageResetPending();

    if (another_storage || stored.syncStorageKey() != key || reset_finished)
    {
        stored.setSyncStorageId(storage_id);
        stored.setSyncStorageKey(key);

        if (reset_finished)
            stored.setSyncStorageResetPending(false);

        if (!writeRouter(stored))
        {
            LOG(ERROR) << "Unable to write the router";
            return SyncResult::FAILED;
        }
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction";
        return SyncResult::FAILED;
    }

    return SyncResult::DONE;
}

//--------------------------------------------------------------------------------------------------
bool Database::setSyncStorageResetPending(const RouterConfig& router)
{
    SqlTransaction transaction(db_);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction";
        return false;
    }

    RouterConfig stored;
    if (routerConfig(&stored) != FindResult::FOUND)
    {
        LOG(ERROR) << "Unable to read the router";
        return false;
    }

    if (!stored.hasSameAccount(router))
    {
        LOG(WARNING) << "The router is saved for another account";
        return false;
    }

    if (!stored.isSyncEnabled())
    {
        LOG(WARNING) << "The synchronization is turned off";
        return false;
    }

    if (!stored.isSyncStorageResetPending())
    {
        stored.setSyncStorageResetPending(true);

        if (!writeRouter(stored))
        {
            LOG(ERROR) << "Unable to write the router";
            return false;
        }

        LOG(INFO) << "The reset of the storage is set pending";
    }

    return transaction.commit();
}

//--------------------------------------------------------------------------------------------------
Database::SyncTransaction::SyncTransaction(Database& db)
    : db_(db)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
Database::SyncResult Database::SyncTransaction::begin(const QByteArray& storage_id)
{
    transaction_.emplace(db_.db_);

    if (!transaction_->begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction";
        transaction_.reset();
        return SyncResult::FAILED;
    }

    RouterConfig router;
    const FindResult found = db_.routerConfig(&router);
    if (found != FindResult::FOUND && found != FindResult::NOT_FOUND)
    {
        LOG(ERROR) << "Unable to read the router";
        transaction_.reset();
        return SyncResult::FAILED;
    }

    if (storage_id.isEmpty() || found == FindResult::NOT_FOUND || router.syncStorageId() != storage_id)
    {
        LOG(WARNING) << "The device is not attached to the storage";
        transaction_.reset();
        return SyncResult::DETACHED;
    }

    return SyncResult::DONE;
}

//--------------------------------------------------------------------------------------------------
bool Database::SyncTransaction::commit()
{
    return transaction_.has_value() && transaction_->commit();
}

//--------------------------------------------------------------------------------------------------
bool Database::SyncTransaction::storeReceivedCredential(const CredentialConfig& credential, qint64 sync_revision)
{
    if (!isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    SqlDatabase& db = db_.db_;

    if (!credential.isValid() || credential.guid().isEmpty() || sync_revision <= 0)
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    std::optional<QByteArray> encrypted_data = credential.encryptedData();
    if (!encrypted_data.has_value())
        return false;

    SqlQuery query(db, "INSERT INTO credentials (id, type, name, data, guid, modify_time, sync_revision) "
                       "VALUES (NULL, ?, ?, ?, ?, ?, ?) "
                       "ON CONFLICT(guid) DO UPDATE SET type=excluded.type, name=excluded.name, "
                       "data=excluded.data, modify_time=excluded.modify_time, "
                       "sync_revision=excluded.sync_revision, sync_dirty=0, deleted=0 "
                       "WHERE credentials.sync_revision!=excluded.sync_revision");
    query.addInt64(static_cast<int>(credential.type()));
    query.addText(credential.displayName());
    query.addBlob(*encrypted_data);
    query.addText(credential.guid());
    query.addInt64(credential.modifyTime());
    query.addInt64(sync_revision);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::SyncTransaction::eraseCredential(const QString& guid)
{
    if (!isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    SqlDatabase& db = db_.db_;

    SqlQuery query(db, "DELETE FROM credentials WHERE guid=?");
    query.addText(guid);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::SyncTransaction::setCredentialSyncRevision(const QString& guid, qint64 sync_revision)
{
    if (!isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    SqlDatabase& db = db_.db_;

    SqlQuery query(db, "UPDATE credentials SET sync_revision=? WHERE guid=?");
    query.addInt64(sync_revision);
    query.addText(guid);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::SyncTransaction::setCredentialUnknownData(const QString& guid, const QByteArray& unknown_data)
{
    if (!isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    SqlDatabase& db = db_.db_;

    SqlQuery row(db, "SELECT data FROM credentials WHERE guid=? AND deleted=0");
    row.addText(guid);

    const SqlQuery::StepResult step = row.next();
    if (step != SqlQuery::StepResult::ROW)
    {
        if (step == SqlQuery::StepResult::FAILED)
            LOG(ERROR) << "Unable to execute query:" << db.lastError();
        else
            LOG(ERROR) << "Credential with guid" << guid << "not found";
        return false;
    }

    CredentialConfig credential;
    credential.setGuid(guid);

    if (!credential.setEncryptedData(row.columnBlob(0)))
    {
        LOG(ERROR) << "Unable to read encrypted data of credential with guid" << guid;
        return false;
    }

    credential.setUnknownData(unknown_data);

    std::optional<QByteArray> encrypted_data = credential.encryptedData();
    if (!encrypted_data.has_value())
    {
        LOG(ERROR) << "Unable to seal credential with guid" << guid;
        return false;
    }

    SqlQuery query(db, "UPDATE credentials SET data=? WHERE guid=?");
    query.addBlob(*encrypted_data);
    query.addText(guid);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::SyncTransaction::confirmSentCredential(const QString& guid, qint64 sync_revision, qint64 sync_dirty)
{
    if (!isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    SqlDatabase& db = db_.db_;

    if (guid.isEmpty() || sync_revision <= 0 || sync_dirty < 0)
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    SqlQuery update_query(db, "UPDATE credentials SET sync_revision=?, sync_dirty=sync_dirty-? WHERE guid=?");
    update_query.addInt64(sync_revision);
    update_query.addInt64(sync_dirty);
    update_query.addText(guid);

    if (!update_query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db.lastError();
        return false;
    }

    if (db.changes() != 0)
    {
        SqlQuery erase_query(db, "DELETE FROM credentials WHERE guid=? AND deleted!=0 AND sync_dirty=0");
        erase_query.addText(guid);

        if (!erase_query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db.lastError();
            return false;
        }
    }
    else
    {
        SqlQuery mark_query(db, "INSERT INTO credentials (id, guid, modify_time, sync_revision, sync_dirty, "
                                "deleted) VALUES (NULL, ?, ?, ?, 1, 1)");
        mark_query.addText(guid);
        mark_query.addInt64(QDateTime::currentSecsSinceEpoch());
        mark_query.addInt64(sync_revision);

        if (!mark_query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db.lastError();
            return false;
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::SyncTransaction::setReceivedCredentialsRevision(qint64 revision)
{
    if (!isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    return db_.setReceivedCredentialsRevision(revision);
}

//--------------------------------------------------------------------------------------------------
bool Database::SyncTransaction::storeReceivedProfile(const ProfileConfig& profile, qint64 sync_revision)
{
    if (!isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    SqlDatabase& db = db_.db_;

    if (!profile.isValid() || profile.guid().isEmpty() || sync_revision <= 0)
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    SqlQuery query(db, "INSERT INTO profiles (id, name, data, guid, modify_time, sync_revision) "
                       "VALUES (NULL, ?, ?, ?, ?, ?) "
                       "ON CONFLICT(guid) DO UPDATE SET name=excluded.name, data=excluded.data, "
                       "modify_time=excluded.modify_time, sync_revision=excluded.sync_revision, sync_dirty=0, "
                       "deleted=0 WHERE profiles.sync_revision!=excluded.sync_revision");
    query.addText(profile.name());
    query.addBlob(profile.serializedData());
    query.addText(profile.guid());
    query.addInt64(profile.modifyTime());
    query.addInt64(sync_revision);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::SyncTransaction::eraseProfile(const QString& guid)
{
    if (!isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    SqlDatabase& db = db_.db_;

    if (guid == ProfileConfig::kDefaultGuid)
    {
        LOG(ERROR) << "The default profile cannot be erased";
        return false;
    }

    SqlQuery query(db, "DELETE FROM profiles WHERE guid=?");
    query.addText(guid);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::SyncTransaction::setProfileSyncRevision(const QString& guid, qint64 sync_revision)
{
    if (!isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    SqlDatabase& db = db_.db_;

    SqlQuery query(db, "UPDATE profiles SET sync_revision=? WHERE guid=?");
    query.addInt64(sync_revision);
    query.addText(guid);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::SyncTransaction::setProfileUnknownData(const QString& guid, const QByteArray& unknown_data)
{
    if (!isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    SqlDatabase& db = db_.db_;

    SqlQuery row(db, "SELECT data FROM profiles WHERE guid=? AND deleted=0");
    row.addText(guid);

    const SqlQuery::StepResult step = row.next();
    if (step != SqlQuery::StepResult::ROW)
    {
        if (step == SqlQuery::StepResult::FAILED)
            LOG(ERROR) << "Unable to execute query:" << db.lastError();
        else
            LOG(ERROR) << "Profile with guid" << guid << "not found";
        return false;
    }

    ProfileConfig profile;
    if (!profile.setSerializedData(row.columnBlob(0)))
    {
        LOG(ERROR) << "Unable to read data of profile with guid" << guid;
        return false;
    }

    profile.setUnknownData(unknown_data);

    SqlQuery query(db, "UPDATE profiles SET data=? WHERE guid=?");
    query.addBlob(profile.serializedData());
    query.addText(guid);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::SyncTransaction::confirmSentProfile(const QString& guid, qint64 sync_revision, qint64 sync_dirty)
{
    if (!isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    SqlDatabase& db = db_.db_;

    if (guid.isEmpty() || sync_revision <= 0 || sync_dirty < 0)
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    SqlQuery update_query(db, "UPDATE profiles SET sync_revision=?, sync_dirty=sync_dirty-? WHERE guid=?");
    update_query.addInt64(sync_revision);
    update_query.addInt64(sync_dirty);
    update_query.addText(guid);

    if (!update_query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db.lastError();
        return false;
    }

    if (db.changes() != 0)
    {
        SqlQuery erase_query(db, "DELETE FROM profiles WHERE guid=? AND deleted!=0 AND sync_dirty=0");
        erase_query.addText(guid);

        if (!erase_query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db.lastError();
            return false;
        }
    }
    else
    {
        SqlQuery mark_query(db, "INSERT INTO profiles (id, guid, modify_time, sync_revision, sync_dirty, deleted) "
                                "VALUES (NULL, ?, ?, ?, 1, 1)");
        mark_query.addText(guid);
        mark_query.addInt64(QDateTime::currentSecsSinceEpoch());
        mark_query.addInt64(sync_revision);

        if (!mark_query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db.lastError();
            return false;
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::SyncTransaction::setReceivedProfilesRevision(qint64 revision)
{
    if (!isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    return db_.setReceivedProfilesRevision(revision);
}

//--------------------------------------------------------------------------------------------------
bool Database::SyncTransaction::isActive() const
{
    return transaction_.has_value() && transaction_->isActive();
}

//--------------------------------------------------------------------------------------------------
Database::ReadResult Database::allRouterHosts(QList<RouterHostConfig>* hosts) const
{
    CHECK(hosts);
    hosts->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return ReadResult::FAILED;
    }

    SqlQuery query(db_, "SELECT host_id, data, IFNULL(credential_id, 0), IFNULL(os_credential_id, 0), "
                        "IFNULL(profile_id, 0) "
                        "FROM router_hosts");

    ReadResult result = ReadResult::OK;

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            // A failed step must not pass for the end of the rows: a caller may treat what is
            // missing from the list as deleted.
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return ReadResult::FAILED;
        }
        if (step == SqlQuery::StepResult::DONE)
            break;

        RouterHostConfig host;
        if (readRouterHost(query, &host) != ReadResult::OK)
            result = ReadResult::INCOMPLETE;

        hosts->append(host);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
bool Database::addRouterHost(const RouterHostConfig& host)
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

    SqlQuery query(db_,
        "INSERT INTO router_hosts (host_id, data, credential_id, os_credential_id, profile_id, modify_time) "
        "VALUES (?, ?, NULLIF(?, 0), NULLIF(?, 0), NULLIF(?, 0), ?)");
    query.addUInt64(host.hostId());
    query.addBlob(*encrypted_data);
    query.addInt64(host.credentialId());
    query.addInt64(host.osCredentialId());
    query.addInt64(host.profileId());
    query.addInt64(QDateTime::currentSecsSinceEpoch());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::modifyRouterHost(const RouterHostConfig& host)
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

    SqlQuery query(db_, "UPDATE router_hosts SET data=?, credential_id=NULLIF(?, 0), modify_time=? WHERE host_id=?");
    query.addBlob(*encrypted_data);
    query.addInt64(host.credentialId());
    query.addInt64(QDateTime::currentSecsSinceEpoch());
    query.addUInt64(host.hostId());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    // An update of a row that is not there changes nothing and reports no error of its own.
    if (db_.changes() == 0)
    {
        LOG(ERROR) << "Credentials of router host" << host.hostId() << "not found";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::removeRouterHost(HostId host_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "DELETE FROM router_hosts WHERE host_id=?");
    query.addUInt64(host_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
Database::FindResult Database::findRouterHost(HostId host_id, RouterHostConfig* host) const
{
    CHECK(host);
    *host = RouterHostConfig();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return FindResult::FAILED;
    }

    SqlQuery query(db_, "SELECT host_id, data, IFNULL(credential_id, 0), IFNULL(os_credential_id, 0), "
                        "IFNULL(profile_id, 0) "
                        "FROM router_hosts WHERE host_id=?");
    query.addUInt64(host_id);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return FindResult::FAILED;
    }

    if (step == SqlQuery::StepResult::DONE)
        return FindResult::NOT_FOUND;

    if (readRouterHost(query, host) != ReadResult::OK)
    {
        LOG(ERROR) << "Unable to read the credentials of host" << host_id;
        return FindResult::UNREADABLE;
    }

    return FindResult::FOUND;
}

//--------------------------------------------------------------------------------------------------
std::optional<std::pair<QString, SecureString>> Database::routerHostCredentials(HostId host_id) const
{
    RouterHostConfig host;
    if (findRouterHost(host_id, &host) != FindResult::FOUND)
        return std::nullopt;

    if (host.credentialId() <= 0)
        return std::make_pair(host.username(), host.password());

    CredentialConfig credential;
    const FindResult found = findCredential(host.credentialId(), &credential);
    if (found == FindResult::NOT_FOUND)
        return std::make_pair(host.username(), host.password());

    if (found != FindResult::FOUND)
        return std::nullopt;

    return std::make_pair(credential.username(), credential.password());
}

//--------------------------------------------------------------------------------------------------
bool Database::outdatedRouterHosts(QList<HostId>* hosts) const
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
    SqlQuery query(db_, "SELECT host_id FROM router_hosts WHERE check_time<? ORDER BY check_time LIMIT ?");
    query.addInt64(QDateTime::currentSecsSinceEpoch() - kRouterHostRecheckInterval);
    query.addInt64(kMaxRouterHostsToCheck);

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return false;
        }
        if (step == SqlQuery::StepResult::DONE)
            break;

        hosts->append(query.columnUInt64(0));
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::updateRouterHostCheckTime(HostId host_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "UPDATE router_hosts SET check_time=? WHERE host_id=?");
    query.addInt64(QDateTime::currentSecsSinceEpoch());
    query.addUInt64(host_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    // An update of a row that is not there changes nothing and reports no error of its own.
    if (db_.changes() == 0)
    {
        LOG(ERROR) << "Credentials of router host" << host_id << "not found";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::setRouterHostOsCredentialId(HostId host_id, qint64 credential_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "UPDATE router_hosts SET os_credential_id=NULLIF(?, 0), modify_time=? WHERE host_id=?");
    query.addInt64(credential_id);
    query.addInt64(QDateTime::currentSecsSinceEpoch());
    query.addUInt64(host_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::setRouterHostProfileId(HostId host_id, qint64 profile_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (profile_id <= 0)
    {
        SqlQuery query(db_, "UPDATE router_hosts SET profile_id=NULL, modify_time=? WHERE host_id=?");
        query.addInt64(QDateTime::currentSecsSinceEpoch());
        query.addUInt64(host_id);

        if (!query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return false;
        }

        return true;
    }

    if (host_id == kInvalidHostId || isTempHostId(host_id))
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    SqlQuery query(db_,
        "INSERT INTO router_hosts (host_id, profile_id, modify_time) VALUES (?, ?, ?) "
        "ON CONFLICT(host_id) DO UPDATE SET profile_id=excluded.profile_id, modify_time=excluded.modify_time");
    query.addUInt64(host_id);
    query.addInt64(profile_id);
    query.addInt64(QDateTime::currentSecsSinceEpoch());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::writeRouter(const RouterConfig& router)
{
    if (!router.isValid())
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    std::optional<QByteArray> encrypted_data = router.encryptedData();
    if (!encrypted_data.has_value())
        return false;

    return writeSetting(kSettingRouter, QString::fromLatin1(encrypted_data->toBase64()));
}

//--------------------------------------------------------------------------------------------------
bool Database::clearRouter()
{
    SqlQuery query(db_, "DELETE FROM settings WHERE name=?");
    query.addText(std::string_view(kSettingRouter));

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return true;
}
