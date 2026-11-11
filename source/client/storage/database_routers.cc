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
#include <QUuid>

#include "base/logging.h"
#include "base/sql/sql_query.h"
#include "proto/router.h"

namespace {

// The rows one pass takes. Every one of them costs a request to the router, so the list is walked
// over as many passes as it needs instead of at once.
const int kMaxRouterHostsToCheck = 10;

// How long the answer of the router is trusted: a row checked less than a week ago is not offered
// for checking again.
const qint64 kRouterHostRecheckInterval = 7 * 24 * 60 * 60;

//--------------------------------------------------------------------------------------------------
Database::ReadResult readRouter(const SqlQuery& query, RouterConfig* router)
{
    router->setRouterId(query.columnInt64(0));
    router->setDisplayName(query.columnText(1));
#if defined(Q_OS_ANDROID)
    router->setSessionType(proto::router::SESSION_TYPE_OPERATOR);
#else
    router->setSessionType(static_cast<proto::router::SessionType>(query.columnInt64(2)));
#endif
    router->setGuid(query.columnText(4));

    if (!router->setEncryptedData(query.columnBlob(3)))
    {
        LOG(ERROR) << "Unable to read encrypted data of router" << router->routerId();
        return Database::ReadResult::INCOMPLETE;
    }

    return Database::ReadResult::OK;
}

//--------------------------------------------------------------------------------------------------
Database::ReadResult readRouterHost(const SqlQuery& query, RouterHostConfig* host)
{
    host->setRouterId(query.columnInt64(0));
    host->setHostId(query.columnUInt64(1));
    host->setCredentialId(query.columnInt64(3));
    host->setOsCredentialId(query.columnInt64(4));
    host->setProfileId(query.columnInt64(5));

    if (!host->setEncryptedData(query.columnBlob(2)))
    {
        LOG(ERROR) << "Unable to read encrypted data of router host" << host->hostId();
        return Database::ReadResult::INCOMPLETE;
    }

    return Database::ReadResult::OK;
}

} // namespace

//--------------------------------------------------------------------------------------------------
Database::ReadResult Database::routerList(QList<RouterConfig>* routers) const
{
    CHECK(routers);
    routers->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return ReadResult::FAILED;
    }

    SqlQuery query(db_, "SELECT id, name, session_type, data, guid FROM routers");

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

        RouterConfig router;
        if (readRouter(query, &router) != ReadResult::OK)
            result = ReadResult::INCOMPLETE;

        routers->append(router);
    }

    std::stable_sort(routers->begin(), routers->end(), [](const RouterConfig& first, const RouterConfig& second)
    {
        return first.displayLabel().compare(second.displayLabel(), Qt::CaseInsensitive) < 0;
    });

    return result;
}

//--------------------------------------------------------------------------------------------------
bool Database::addRouter(RouterConfig& router)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (!router.isValid())
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    std::optional<QByteArray> encrypted_data = router.encryptedData();
    if (!encrypted_data.has_value())
        return false;

    if (router.guid().isEmpty())
        router.setGuid(QUuid::createUuid().toString(QUuid::WithoutBraces));

    SqlQuery query(db_, "INSERT INTO routers (id, name, session_type, data, guid) "
                        "VALUES (NULL, ?, ?, ?, ?)");
    query.addText(router.displayName());
    query.addInt64(static_cast<quint32>(router.sessionType()));
    query.addBlob(*encrypted_data);
    query.addText(router.guid());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    router.setRouterId(db_.lastInsertRowId());
    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::modifyRouter(const RouterConfig& router)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (!router.isValid())
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    std::optional<QByteArray> encrypted_data = router.encryptedData();
    if (!encrypted_data.has_value())
        return false;

    // The guid is not among the columns. It names the record from the moment it is added, and
    // the chat history stored under it would be lost if an edit could change it.
    SqlQuery query(db_, "UPDATE routers SET name=?, session_type=?, data=? WHERE id=?");
    query.addText(router.displayName());
    query.addInt64(static_cast<quint32>(router.sessionType()));
    query.addBlob(*encrypted_data);
    query.addInt64(router.routerId());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    // An update of a row that is not there changes nothing and reports no error of its own.
    if (db_.changes() == 0)
    {
        LOG(ERROR) << "Router" << router.routerId() << "not found";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::removeRouter(qint64 router_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "DELETE FROM routers WHERE id=?");
    query.addInt64(router_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
Database::FindResult Database::findRouter(qint64 router_id, RouterConfig* router) const
{
    CHECK(router);
    *router = RouterConfig();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return FindResult::FAILED;
    }

    SqlQuery query(db_, "SELECT id, name, session_type, data, guid FROM routers WHERE id=?");
    query.addInt64(router_id);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return FindResult::FAILED;
    }

    if (step == SqlQuery::StepResult::DONE)
        return FindResult::NOT_FOUND;

    if (readRouter(query, router) != ReadResult::OK)
    {
        LOG(ERROR) << "Unable to read the data of router" << router_id;
        return FindResult::UNREADABLE;
    }

    return FindResult::FOUND;
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

    SqlQuery query(db_, "SELECT router_id, host_id, data, IFNULL(credential_id, 0), IFNULL(os_credential_id, 0), "
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

    SqlQuery query(db_, "INSERT INTO router_hosts (router_id, host_id, data, credential_id, os_credential_id, "
                        "profile_id) VALUES (?, ?, ?, NULLIF(?, 0), NULLIF(?, 0), NULLIF(?, 0))");
    query.addInt64(host.routerId());
    query.addUInt64(host.hostId());
    query.addBlob(*encrypted_data);
    query.addInt64(host.credentialId());
    query.addInt64(host.osCredentialId());
    query.addInt64(host.profileId());

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

    SqlQuery query(db_, "UPDATE router_hosts SET data=?, credential_id=NULLIF(?, 0) "
                        "WHERE router_id=? AND host_id=?");
    query.addBlob(*encrypted_data);
    query.addInt64(host.credentialId());
    query.addInt64(host.routerId());
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
bool Database::removeRouterHost(qint64 router_id, HostId host_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "DELETE FROM router_hosts WHERE router_id=? AND host_id=?");
    query.addInt64(router_id);
    query.addUInt64(host_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
Database::FindResult Database::findRouterHost(qint64 router_id, HostId host_id, RouterHostConfig* host) const
{
    CHECK(host);
    *host = RouterHostConfig();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return FindResult::FAILED;
    }

    SqlQuery query(db_, "SELECT router_id, host_id, data, IFNULL(credential_id, 0), IFNULL(os_credential_id, 0), "
                        "IFNULL(profile_id, 0) "
                        "FROM router_hosts WHERE router_id=? AND host_id=?");
    query.addInt64(router_id);
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
std::optional<std::pair<QString, SecureString>> Database::routerHostCredentials(
    qint64 router_id, HostId host_id) const
{
    RouterHostConfig host;
    if (findRouterHost(router_id, host_id, &host) != FindResult::FOUND)
        return std::nullopt;

    if (host.credentialId() <= 0)
        return std::make_pair(host.username(), host.password());

    CredentialConfig credential;
    if (findCredential(host.credentialId(), &credential) != FindResult::FOUND)
        return std::nullopt;

    return std::make_pair(credential.username(), credential.password());
}

//--------------------------------------------------------------------------------------------------
bool Database::outdatedRouterHosts(qint64 router_id, QList<HostId>* hosts) const
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
    SqlQuery query(db_, "SELECT host_id FROM router_hosts WHERE router_id=? AND check_time<? "
                        "ORDER BY check_time LIMIT ?");
    query.addInt64(router_id);
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
bool Database::updateRouterHostCheckTime(qint64 router_id, HostId host_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "UPDATE router_hosts SET check_time=? WHERE router_id=? AND host_id=?");
    query.addInt64(QDateTime::currentSecsSinceEpoch());
    query.addInt64(router_id);
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
bool Database::setRouterHostOsCredentialId(qint64 router_id, HostId host_id, qint64 credential_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "UPDATE router_hosts SET os_credential_id=NULLIF(?, 0) WHERE router_id=? AND host_id=?");
    query.addInt64(credential_id);
    query.addInt64(router_id);
    query.addUInt64(host_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::setRouterHostProfileId(qint64 router_id, HostId host_id, qint64 profile_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (profile_id <= 0)
    {
        SqlQuery query(db_, "UPDATE router_hosts SET profile_id=NULL WHERE router_id=? AND host_id=?");
        query.addInt64(router_id);
        query.addUInt64(host_id);

        if (!query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return false;
        }

        return true;
    }

    RouterHostConfig host;
    host.setRouterId(router_id);
    host.setHostId(host_id);

    if (!host.isValid())
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    SqlQuery query(db_, "INSERT INTO router_hosts (router_id, host_id, profile_id) VALUES (?, ?, ?) "
                        "ON CONFLICT(router_id, host_id) DO UPDATE SET profile_id=excluded.profile_id");
    query.addInt64(router_id);
    query.addUInt64(host_id);
    query.addInt64(profile_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return true;
}
