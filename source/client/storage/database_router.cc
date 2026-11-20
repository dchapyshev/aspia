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
#include "client/storage/credential_store.h"
#include "client/storage/sync_store.h"
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

            if (!SyncStore(*this).reset())
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

        SyncStore sync_store(*this);
        if (!(discard ? sync_store.discard() : sync_store.reset()))
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
std::optional<std::pair<QString, SecureString>> Database::routerHostCredentials(HostId host_id)
{
    RouterHostConfig host;
    if (findRouterHost(host_id, &host) != FindResult::FOUND)
        return std::nullopt;

    if (host.credentialId() <= 0)
        return std::make_pair(host.username(), host.password());

    CredentialConfig credential;
    const FindResult found = CredentialStore(*this).find(host.credentialId(), &credential);
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
