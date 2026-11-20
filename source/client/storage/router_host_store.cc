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

    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return Database::ReadResult::FAILED;
    }

    SqlQuery query(sql(), "SELECT host_id, data, IFNULL(credential_id, 0), IFNULL(os_credential_id, 0), "
                          "IFNULL(profile_id, 0) "
                          "FROM router_hosts");

    Database::ReadResult result = Database::ReadResult::OK;

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            // A failed step must not pass for the end of the rows: a caller may treat what is
            // missing from the list as deleted.
            LOG(ERROR) << "Unable to execute query:" << sql().lastError();
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
    if (!db_.isValid())
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

    SqlQuery query(sql(),
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
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool RouterHostStore::modify(const RouterHostConfig& host)
{
    if (!db_.isValid())
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

    SqlQuery query(sql(), "UPDATE router_hosts SET data=?, credential_id=NULLIF(?, 0), modify_time=? WHERE host_id=?");
    query.addBlob(*encrypted_data);
    query.addInt64(host.credentialId());
    query.addInt64(QDateTime::currentSecsSinceEpoch());
    query.addUInt64(host.hostId());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    // An update of a row that is not there changes nothing and reports no error of its own.
    if (sql().changes() == 0)
    {
        LOG(ERROR) << "Credentials of router host" << host.hostId() << "not found";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool RouterHostStore::remove(HostId host_id)
{
    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(sql(), "DELETE FROM router_hosts WHERE host_id=?");
    query.addUInt64(host_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
Database::FindResult RouterHostStore::find(HostId host_id, RouterHostConfig* host) const
{
    CHECK(host);
    *host = RouterHostConfig();

    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return Database::FindResult::FAILED;
    }

    SqlQuery query(sql(), "SELECT host_id, data, IFNULL(credential_id, 0), IFNULL(os_credential_id, 0), "
                          "IFNULL(profile_id, 0) "
                          "FROM router_hosts WHERE host_id=?");
    query.addUInt64(host_id);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
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
    const Database::FindResult found = CredentialStore(db_).find(host.credentialId(), &credential);
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

    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    // The rows checked longest ago come first, so repeated calls walk the whole list instead of
    // returning to the same rows.
    SqlQuery query(sql(), "SELECT host_id FROM router_hosts WHERE check_time<? ORDER BY check_time LIMIT ?");
    query.addInt64(QDateTime::currentSecsSinceEpoch() - kRouterHostRecheckInterval);
    query.addInt64(kMaxRouterHostsToCheck);

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << sql().lastError();
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
    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(sql(), "UPDATE router_hosts SET check_time=? WHERE host_id=?");
    query.addInt64(QDateTime::currentSecsSinceEpoch());
    query.addUInt64(host_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    // An update of a row that is not there changes nothing and reports no error of its own.
    if (sql().changes() == 0)
    {
        LOG(ERROR) << "Credentials of router host" << host_id << "not found";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool RouterHostStore::setOsCredentialId(HostId host_id, qint64 credential_id)
{
    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(sql(), "UPDATE router_hosts SET os_credential_id=NULLIF(?, 0), modify_time=? WHERE host_id=?");
    query.addInt64(credential_id);
    query.addInt64(QDateTime::currentSecsSinceEpoch());
    query.addUInt64(host_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool RouterHostStore::setProfileId(HostId host_id, qint64 profile_id)
{
    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (profile_id <= 0)
    {
        SqlQuery query(sql(), "UPDATE router_hosts SET profile_id=NULL, modify_time=? WHERE host_id=?");
        query.addInt64(QDateTime::currentSecsSinceEpoch());
        query.addUInt64(host_id);

        if (!query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << sql().lastError();
            return false;
        }

        return true;
    }

    if (host_id == kInvalidHostId || isTempHostId(host_id))
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    SqlQuery query(sql(),
        "INSERT INTO router_hosts (host_id, profile_id, modify_time) VALUES (?, ?, ?) "
        "ON CONFLICT(host_id) DO UPDATE SET profile_id=excluded.profile_id, modify_time=excluded.modify_time");
    query.addUInt64(host_id);
    query.addInt64(profile_id);
    query.addInt64(QDateTime::currentSecsSinceEpoch());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    return true;
}
