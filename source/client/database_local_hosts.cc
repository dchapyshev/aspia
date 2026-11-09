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

#include "client/database.h"

#include <QDateTime>
#include <QUuid>

#include "base/logging.h"
#include "base/sql/sql_query.h"

namespace {

//--------------------------------------------------------------------------------------------------
Database::ReadResult readLocalHost(const SqlQuery& query, LocalHostConfig* host)
{
    host->setId(query.columnInt64(0));
    host->setGroupId(query.columnInt64(1));
    host->setRouterId(query.columnInt64(2));
    host->setName(query.columnText(3));
    host->setComment(query.columnText(4));
    host->setCreateTime(query.columnInt64(6));
    host->setModifyTime(query.columnInt64(7));
    host->setConnectTime(query.columnInt64(8));
    host->setGuid(query.columnText(9));
    host->setCredentialId(query.columnInt64(10));
    host->setOsCredentialId(query.columnInt64(11));

    if (!host->setEncryptedData(query.columnBlob(5)))
    {
        LOG(ERROR) << "Unable to read encrypted data of host" << host->id();
        return Database::ReadResult::INCOMPLETE;
    }

    return Database::ReadResult::OK;
}

//--------------------------------------------------------------------------------------------------
LocalGroupConfig readLocalGroup(const SqlQuery& query)
{
    LocalGroupConfig group;
    group.setId(query.columnInt64(0));
    group.setParentId(query.columnInt64(1));
    group.setName(query.columnText(2));
    group.setComment(query.columnText(3));
    group.setGuid(query.columnText(4));
    return group;
}

//--------------------------------------------------------------------------------------------------
// True when the new parent is the group itself or one of the groups below it. Such a move closes a
// loop: the group and everything under it drop out of the tree, which is walked from the root down,
// and a path built by walking parents upward from a host inside the loop never ends.
bool wouldCloseLoop(SqlDatabase& db, qint64 group_id, qint64 new_parent_id)
{
    // The root is where the tree starts and is below nothing.
    if (new_parent_id <= 0)
        return false;

    if (new_parent_id == group_id)
        return true;

    // Walks the parents of the new parent upward. On a tree that is still whole the walk ends at
    // the root, and SQLite caps a runaway recursion by itself.
    const char kSql[] =
        "WITH RECURSIVE ancestors(id) AS ("
        "    SELECT id FROM local_groups WHERE id=?"
        "    UNION ALL"
        "    SELECT g.parent_id FROM local_groups g JOIN ancestors a ON g.id = a.id "
        "      WHERE g.parent_id IS NOT NULL"
        ") SELECT 1 FROM ancestors WHERE id=? LIMIT 1";

    SqlQuery query(db, kSql);
    query.addInt64(new_parent_id);
    query.addInt64(group_id);

    return query.next() == SqlQuery::StepResult::ROW;
}

} // namespace

//--------------------------------------------------------------------------------------------------
Database::ReadResult Database::localHostList(qint64 group_id, QList<LocalHostConfig>* hosts) const
{
    CHECK(hosts);
    hosts->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return ReadResult::FAILED;
    }

    SqlQuery query(db_, "SELECT id, IFNULL(group_id, 0), router_id, name, comment, data, "
                        "create_time, modify_time, connect_time, guid, IFNULL(credential_id, 0), "
                        "IFNULL(os_credential_id, 0) "
                        "FROM local_hosts WHERE group_id IS NULLIF(?, 0) ORDER BY casefold(name)");
    query.addInt64(group_id);

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

        LocalHostConfig host;
        if (readLocalHost(query, &host) != ReadResult::OK)
            result = ReadResult::INCOMPLETE;

        hosts->append(host);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
Database::ReadResult Database::allLocalHosts(QList<LocalHostConfig>* hosts) const
{
    CHECK(hosts);
    hosts->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return ReadResult::FAILED;
    }

    SqlQuery query(db_, "SELECT id, IFNULL(group_id, 0), router_id, name, comment, data, "
                        "create_time, modify_time, connect_time, guid, IFNULL(credential_id, 0), "
                        "IFNULL(os_credential_id, 0) "
                        "FROM local_hosts");

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

        LocalHostConfig host;
        if (readLocalHost(query, &host) != ReadResult::OK)
            result = ReadResult::INCOMPLETE;

        hosts->append(host);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
bool Database::addLocalHost(LocalHostConfig& host)
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

    SqlQuery query(db_, "INSERT INTO local_hosts (id, group_id, router_id, name, comment, data, "
                        "create_time, modify_time, connect_time, guid, credential_id, os_credential_id) "
                        "VALUES (NULL, NULLIF(?, 0), ?, ?, ?, ?, ?, ?, ?, ?, NULLIF(?, 0), NULLIF(?, 0))");
    query.addInt64(host.groupId());
    query.addInt64(host.routerId());
    query.addText(host.name());
    query.addText(host.comment());
    query.addBlob(*encrypted_data);
    query.addInt64(host.createTime());
    query.addInt64(host.modifyTime());
    query.addInt64(host.connectTime());
    query.addText(host.guid());
    query.addInt64(host.credentialId());
    query.addInt64(host.osCredentialId());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    host.setId(db_.lastInsertRowId());
    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::modifyLocalHost(LocalHostConfig& host)
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

    host.setModifyTime(QDateTime::currentSecsSinceEpoch());

    SqlQuery query(db_, "UPDATE local_hosts SET group_id=NULLIF(?, 0), router_id=?, name=?, comment=?, "
                        "data=?, modify_time=?, credential_id=NULLIF(?, 0) WHERE id=?");
    query.addInt64(host.groupId());
    query.addInt64(host.routerId());
    query.addText(host.name());
    query.addText(host.comment());
    query.addBlob(*encrypted_data);
    query.addInt64(host.modifyTime());
    query.addInt64(host.credentialId());
    query.addInt64(host.id());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::moveLocalHost(qint64 entry_id, qint64 new_group_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "UPDATE local_hosts SET group_id=NULLIF(?, 0), modify_time=? WHERE id=?");
    query.addInt64(new_group_id);
    query.addInt64(QDateTime::currentSecsSinceEpoch());
    query.addInt64(entry_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    if (db_.changes() == 0)
    {
        LOG(ERROR) << "Local host" << entry_id << "not found";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::removeLocalHost(qint64 entry_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "DELETE FROM local_hosts WHERE id=?");
    query.addInt64(entry_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::setLocalHostConnectTime(qint64 entry_id, qint64 connect_time)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "UPDATE local_hosts SET connect_time=? WHERE id=?");
    query.addInt64(connect_time);
    query.addInt64(entry_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::setLocalHostOsCredentialId(qint64 entry_id, qint64 credential_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "UPDATE local_hosts SET os_credential_id=NULLIF(?, 0) WHERE id=?");
    query.addInt64(credential_id);
    query.addInt64(entry_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
Database::FindResult Database::findLocalHost(qint64 entry_id, LocalHostConfig* host) const
{
    CHECK(host);
    *host = LocalHostConfig();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return FindResult::FAILED;
    }

    SqlQuery query(db_, "SELECT id, IFNULL(group_id, 0), router_id, name, comment, data, "
                        "create_time, modify_time, connect_time, guid, IFNULL(credential_id, 0), "
                        "IFNULL(os_credential_id, 0) "
                        "FROM local_hosts WHERE id=?");
    query.addInt64(entry_id);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return FindResult::FAILED;
    }

    if (step == SqlQuery::StepResult::DONE)
        return FindResult::NOT_FOUND;

    if (readLocalHost(query, host) != ReadResult::OK)
    {
        LOG(ERROR) << "Unable to read the data of host" << entry_id;
        return FindResult::UNREADABLE;
    }

    return FindResult::FOUND;
}

//--------------------------------------------------------------------------------------------------
std::optional<std::pair<QString, SecureString>> Database::localHostCredentials(qint64 entry_id) const
{
    LocalHostConfig host;
    if (findLocalHost(entry_id, &host) != FindResult::FOUND)
        return std::nullopt;

    if (host.credentialId() <= 0)
        return std::make_pair(host.username(), host.password());

    CredentialConfig credential;
    if (findCredential(host.credentialId(), &credential) != FindResult::FOUND)
        return std::nullopt;

    return std::make_pair(credential.username(), credential.password());
}

//--------------------------------------------------------------------------------------------------
Database::ReadResult Database::searchLocalHosts(
    const QString& query_text, QList<LocalHostConfig>* hosts) const
{
    CHECK(hosts);
    hosts->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return ReadResult::FAILED;
    }

    SqlQuery query(db_, "SELECT id, IFNULL(group_id, 0), router_id, name, comment, data, "
                        "create_time, modify_time, connect_time, guid, IFNULL(credential_id, 0), "
                        "IFNULL(os_credential_id, 0) "
                        "FROM local_hosts ORDER BY casefold(name)");

    ReadResult result = ReadResult::OK;

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return ReadResult::FAILED;
        }
        if (step == SqlQuery::StepResult::DONE)
            break;

        LocalHostConfig host;
        const ReadResult host_result = readLocalHost(query, &host);

        if (host.name().contains(query_text, Qt::CaseInsensitive) ||
            host.address().contains(query_text, Qt::CaseInsensitive) ||
            host.comment().contains(query_text, Qt::CaseInsensitive))
        {
            if (host_result != ReadResult::OK)
                result = ReadResult::INCOMPLETE;

            hosts->append(host);
        }
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
bool Database::localGroupList(qint64 parent_id, QList<LocalGroupConfig>* groups) const
{
    CHECK(groups);
    groups->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "SELECT id, IFNULL(parent_id, 0), name, comment, guid FROM local_groups "
                        "WHERE parent_id IS NULLIF(?, 0) ORDER BY casefold(name)");
    query.addInt64(parent_id);

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

        groups->append(readLocalGroup(query));
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::allLocalGroups(QList<LocalGroupConfig>* groups) const
{
    CHECK(groups);
    groups->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "SELECT id, IFNULL(parent_id, 0), name, comment, guid FROM local_groups "
                        "ORDER BY casefold(name)");

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

        groups->append(readLocalGroup(query));
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::addLocalGroup(LocalGroupConfig& group)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (!group.isValid())
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    if (group.guid().isEmpty())
        group.setGuid(QUuid::createUuid().toString(QUuid::WithoutBraces));

    SqlQuery query(db_, "INSERT INTO local_groups (id, parent_id, name, comment, guid) "
                        "VALUES (NULL, NULLIF(?, 0), ?, ?, ?)");
    query.addInt64(group.parentId());
    query.addText(group.name());
    query.addText(group.comment());
    query.addText(group.guid());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    group.setId(db_.lastInsertRowId());
    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::modifyLocalGroup(const LocalGroupConfig& group)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (!group.isValid())
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    if (wouldCloseLoop(db_, group.id(), group.parentId()))
    {
        LOG(ERROR) << "Group" << group.id() << "cannot be put under" << group.parentId();
        return false;
    }

    SqlQuery query(db_, "UPDATE local_groups SET parent_id=NULLIF(?, 0), name=?, comment=? WHERE id=?");
    query.addInt64(group.parentId());
    query.addText(group.name());
    query.addText(group.comment());
    query.addInt64(group.id());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::moveLocalGroup(qint64 group_id, qint64 new_parent_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (wouldCloseLoop(db_, group_id, new_parent_id))
    {
        LOG(ERROR) << "Group" << group_id << "cannot be put under" << new_parent_id;
        return false;
    }

    SqlQuery query(db_, "UPDATE local_groups SET parent_id=NULLIF(?, 0) WHERE id=?");
    query.addInt64(new_parent_id);
    query.addInt64(group_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    if (db_.changes() == 0)
    {
        LOG(ERROR) << "Local group" << group_id << "not found";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::removeLocalGroup(qint64 group_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    // The root is the parent every group of the top level names and not a record of its own.
    if (group_id <= 0)
    {
        LOG(ERROR) << "Invalid group id:" << group_id;
        return false;
    }

    // The child groups go with it and the hosts of the whole subtree move to the root. Both are
    // declared by the tables themselves, so no path can leave a row pointing at a group that is
    // gone - the tree is walked from the root down and such a row is in no place the user can reach.
    SqlQuery query(db_, "DELETE FROM local_groups WHERE id=?");
    query.addInt64(group_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
Database::FindResult Database::findLocalGroup(qint64 group_id, LocalGroupConfig* group) const
{
    CHECK(group);
    *group = LocalGroupConfig();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return FindResult::FAILED;
    }

    SqlQuery query(db_, "SELECT id, IFNULL(parent_id, 0), name, comment, guid FROM local_groups "
                        "WHERE id=?");
    query.addInt64(group_id);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return FindResult::FAILED;
    }

    if (step == SqlQuery::StepResult::DONE)
        return FindResult::NOT_FOUND;

    *group = readLocalGroup(query);
    return FindResult::FOUND;
}
