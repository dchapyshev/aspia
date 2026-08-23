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
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QUuid>

#include "base/logging.h"
#include "base/files/base_paths.h"
#include "base/sql/sql_query.h"
#include "base/sql/sql_transaction.h"
#include "build/build_config.h"

namespace {

// The rows one pass takes. Every one of them costs a request to the router, so the list is walked
// over as many passes as it needs instead of at once.
const int kMaxRouterHostsToCheck = 10;

// How long the answer of the router is trusted: a row checked less than a week ago is not offered
// for checking again.
const qint64 kRouterHostRecheckInterval = 7 * 24 * 60 * 60;

constexpr auto kSettingDisplayName   = "display_name";
constexpr auto kSettingCheckUpdates  = "check_updates";
constexpr auto kSettingUpdateServer  = "update_server";
constexpr auto kSettingSalt          = "master_password_salt";
constexpr auto kSettingVerifier      = "master_password_verifier";
constexpr auto kSettingVersion       = "master_password_version";
constexpr auto kSettingBiometricBlob = "biometric_blob";

QString g_test_file_path;

//--------------------------------------------------------------------------------------------------
LocalHostConfig readHost(const SqlQuery& query)
{
    LocalHostConfig host;
    host.setId(query.columnInt64(0));
    host.setGroupId(query.columnInt64(1));
    host.setRouterId(query.columnInt64(2));
    host.setName(query.columnText(3));
    host.setComment(query.columnText(4));
    host.setCreateTime(query.columnInt64(6));
    host.setModifyTime(query.columnInt64(7));
    host.setConnectTime(query.columnInt64(8));
    host.setGuid(query.columnText(9));

    if (!host.setEncryptedData(query.columnBlob(5)))
        LOG(ERROR) << "Unable to read encrypted data of host" << host.id();

    return host;
}

//--------------------------------------------------------------------------------------------------
LocalGroupConfig readGroup(const SqlQuery& query)
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
RouterConfig readRouter(const SqlQuery& query)
{
    RouterConfig router;
    router.setRouterId(query.columnInt64(0));
    router.setDisplayName(query.columnText(1));
    router.setSessionType(static_cast<proto::router::SessionType>(query.columnInt64(2)));
    router.setGuid(query.columnText(4));

    if (!router.setEncryptedData(query.columnBlob(3)))
        LOG(ERROR) << "Unable to read encrypted data of router" << router.routerId();

    return router;
}

//--------------------------------------------------------------------------------------------------
RouterHostConfig readRouterHost(const SqlQuery& query)
{
    RouterHostConfig host;
    host.setRouterId(query.columnInt64(0));
    host.setHostId(query.columnUInt64(1));

    if (!host.setEncryptedData(query.columnBlob(2)))
        LOG(ERROR) << "Unable to read encrypted data of router host" << host.hostId();

    return host;
}

//--------------------------------------------------------------------------------------------------
bool createTables(SqlDatabase& db)
{
    // A group without a parent and a host without a group are the ones at the root: the root is
    // where the tree starts and not a record of its own, so there is no id for them to name.
    if (!db.exec("CREATE TABLE IF NOT EXISTS \"local_groups\" ("
                 "\"id\" INTEGER UNIQUE,"
                 "\"parent_id\" INTEGER REFERENCES \"local_groups\"(\"id\") ON DELETE CASCADE,"
                 "\"name\" TEXT NOT NULL DEFAULT '',"
                 "\"comment\" TEXT NOT NULL DEFAULT '',"
                 "\"guid\" TEXT NOT NULL UNIQUE,"
                 "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        LOG(ERROR) << "Unable to create local_groups table:" << db.lastError();
        return false;
    }

    if (!db.exec("CREATE TABLE IF NOT EXISTS \"local_hosts\" ("
                 "\"id\" INTEGER UNIQUE,"
                 "\"group_id\" INTEGER REFERENCES \"local_groups\"(\"id\") ON DELETE SET NULL,"
                 "\"router_id\" INTEGER NOT NULL DEFAULT 0,"
                 "\"name\" TEXT NOT NULL DEFAULT '',"
                 "\"comment\" TEXT NOT NULL DEFAULT '',"
                 "\"data\" BLOB DEFAULT X'',"
                 "\"create_time\" INTEGER NOT NULL DEFAULT 0,"
                 "\"modify_time\" INTEGER NOT NULL DEFAULT 0,"
                 "\"connect_time\" INTEGER NOT NULL DEFAULT 0,"
                 "\"guid\" TEXT NOT NULL UNIQUE,"
                 "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        LOG(ERROR) << "Unable to create local_hosts table:" << db.lastError();
        return false;
    }

    if (!db.exec("CREATE TABLE IF NOT EXISTS \"routers\" ("
                 "\"id\" INTEGER UNIQUE,"
                 "\"name\" TEXT NOT NULL DEFAULT '',"
                 "\"session_type\" INTEGER NOT NULL DEFAULT 0,"
                 "\"data\" BLOB DEFAULT X'',"
                 "\"guid\" TEXT NOT NULL UNIQUE,"
                 "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        LOG(ERROR) << "Unable to create routers table:" << db.lastError();
        return false;
    }

    if (!db.exec("CREATE TABLE IF NOT EXISTS \"router_hosts\" ("
                 "\"router_id\" INTEGER NOT NULL REFERENCES \"routers\"(\"id\") ON DELETE CASCADE,"
                 "\"host_id\" INTEGER NOT NULL,"
                 "\"check_time\" INTEGER NOT NULL DEFAULT 0,"
                 "\"data\" BLOB DEFAULT X'',"
                 "PRIMARY KEY(\"router_id\",\"host_id\"))"))
    {
        LOG(ERROR) << "Unable to create router_hosts table:" << db.lastError();
        return false;
    }

    if (!db.exec("CREATE TABLE IF NOT EXISTS \"settings\" ("
                 "\"name\" TEXT PRIMARY KEY NOT NULL,"
                 "\"value\" TEXT NOT NULL)"))
    {
        LOG(ERROR) << "Unable to create settings table:" << db.lastError();
        return false;
    }

    return true;
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
// static
Database& Database::instance()
{
    static thread_local Database database;

    if (!database.db_.isOpen())
        database.openDatabase();

    return database;
}

//--------------------------------------------------------------------------------------------------
// static
QString Database::filePath()
{
    if (!g_test_file_path.isEmpty())
        return g_test_file_path;

    QString dir_path = BasePaths::appUserDataDir();
    if (dir_path.isEmpty())
        return QString();

    return dir_path + "/client.db3";
}

//--------------------------------------------------------------------------------------------------
bool Database::isValid() const
{
    return db_.isOpen();
}

//--------------------------------------------------------------------------------------------------
bool Database::localHostList(qint64 group_id, QList<LocalHostConfig>* hosts) const
{
    CHECK(hosts);
    hosts->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "SELECT id, IFNULL(group_id, 0), router_id, name, comment, data, "
                        "create_time, modify_time, connect_time, guid "
                        "FROM local_hosts WHERE group_id IS NULLIF(?, 0)");
    query.addInt64(group_id);

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            // A failed step must not pass for the end of the rows: a caller may treat what is
            // missing from the list as deleted.
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return false;
        }
        if (step == SqlQuery::StepResult::DONE)
            break;

        hosts->append(readHost(query));
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::allLocalHosts(QList<LocalHostConfig>* hosts) const
{
    CHECK(hosts);
    hosts->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "SELECT id, IFNULL(group_id, 0), router_id, name, comment, data, "
                        "create_time, modify_time, connect_time, guid "
                        "FROM local_hosts");

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            // A failed step must not pass for the end of the rows: a caller may treat what is
            // missing from the list as deleted.
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return false;
        }
        if (step == SqlQuery::StepResult::DONE)
            break;

        hosts->append(readHost(query));
    }

    return true;
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
                        "create_time, modify_time, connect_time, guid) "
                        "VALUES (NULL, NULLIF(?, 0), ?, ?, ?, ?, ?, ?, ?, ?)");
    query.addInt64(host.groupId());
    query.addInt64(host.routerId());
    query.addText(host.name());
    query.addText(host.comment());
    query.addBlob(*encrypted_data);
    query.addInt64(host.createTime());
    query.addInt64(host.modifyTime());
    query.addInt64(host.connectTime());
    query.addText(host.guid());

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
                        "data=?, modify_time=? WHERE id=?");
    query.addInt64(host.groupId());
    query.addInt64(host.routerId());
    query.addText(host.name());
    query.addText(host.comment());
    query.addBlob(*encrypted_data);
    query.addInt64(host.modifyTime());
    query.addInt64(host.id());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
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
std::optional<LocalHostConfig> Database::findLocalHost(qint64 entry_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return std::nullopt;
    }

    SqlQuery query(db_, "SELECT id, IFNULL(group_id, 0), router_id, name, comment, data, "
                        "create_time, modify_time, connect_time, guid "
                        "FROM local_hosts WHERE id=?");
    query.addInt64(entry_id);

    if (query.next() != SqlQuery::StepResult::ROW)
        return std::nullopt;

    return readHost(query);
}

//--------------------------------------------------------------------------------------------------
std::optional<LocalHostConfig> Database::findLocalHostByGuid(const QString& guid) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return std::nullopt;
    }

    if (guid.isEmpty())
        return std::nullopt;

    SqlQuery query(db_, "SELECT id, IFNULL(group_id, 0), router_id, name, comment, data, "
                        "create_time, modify_time, connect_time, guid "
                        "FROM local_hosts WHERE guid=?");
    query.addText(guid);

    if (query.next() != SqlQuery::StepResult::ROW)
        return std::nullopt;

    return readHost(query);
}

//--------------------------------------------------------------------------------------------------
bool Database::searchLocalHosts(const QString& query_text, QList<LocalHostConfig>* hosts) const
{
    CHECK(hosts);
    hosts->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "SELECT id, IFNULL(group_id, 0), router_id, name, comment, data, "
                        "create_time, modify_time, connect_time, guid FROM local_hosts");

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

        LocalHostConfig host = readHost(query);
        if (host.name().contains(query_text, Qt::CaseInsensitive) ||
            host.address().contains(query_text, Qt::CaseInsensitive))
        {
            hosts->append(host);
        }
    }

    return true;
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
                        "WHERE parent_id IS NULLIF(?, 0)");
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

        groups->append(readGroup(query));
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

    SqlQuery query(db_, "SELECT id, IFNULL(parent_id, 0), name, comment, guid FROM local_groups");

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

        groups->append(readGroup(query));
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
std::optional<LocalGroupConfig> Database::findLocalGroup(qint64 group_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return std::nullopt;
    }

    SqlQuery query(db_, "SELECT id, IFNULL(parent_id, 0), name, comment, guid FROM local_groups "
                        "WHERE id=?");
    query.addInt64(group_id);

    if (query.next() != SqlQuery::StepResult::ROW)
        return std::nullopt;

    return readGroup(query);
}

//--------------------------------------------------------------------------------------------------
bool Database::routerList(QList<RouterConfig>* routers) const
{
    CHECK(routers);
    routers->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "SELECT id, name, session_type, data, guid FROM routers");

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            // A failed step must not pass for the end of the rows: a caller may treat what is
            // missing from the list as deleted.
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return false;
        }
        if (step == SqlQuery::StepResult::DONE)
            break;

        routers->append(readRouter(query));
    }

    return true;
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
    // the links pointing at it would stop resolving if an edit could change it.
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
std::optional<RouterConfig> Database::findRouter(qint64 router_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return std::nullopt;
    }

    SqlQuery query(db_, "SELECT id, name, session_type, data, guid FROM routers WHERE id=?");
    query.addInt64(router_id);

    const SqlQuery::StepResult step = query.next();
    if (step != SqlQuery::StepResult::ROW)
    {
        if (step == SqlQuery::StepResult::FAILED)
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return std::nullopt;
    }

    return readRouter(query);
}

//--------------------------------------------------------------------------------------------------
bool Database::allRouterHosts(QList<RouterHostConfig>* hosts) const
{
    CHECK(hosts);
    hosts->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "SELECT router_id, host_id, data FROM router_hosts");

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            // A failed step must not pass for the end of the rows: a caller may treat what is
            // missing from the list as deleted.
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return false;
        }
        if (step == SqlQuery::StepResult::DONE)
            break;

        hosts->append(readRouterHost(query));
    }

    return true;
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

    SqlQuery query(db_, "INSERT INTO router_hosts (router_id, host_id, data) VALUES (?, ?, ?)");
    query.addInt64(host.routerId());
    query.addUInt64(host.hostId());
    query.addBlob(*encrypted_data);

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

    SqlQuery query(db_, "UPDATE router_hosts SET data=? WHERE router_id=? AND host_id=?");
    query.addBlob(*encrypted_data);
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
std::optional<RouterHostConfig> Database::findRouterHost(qint64 router_id, HostId host_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return std::nullopt;
    }

    SqlQuery query(db_, "SELECT router_id, host_id, data FROM router_hosts "
                        "WHERE router_id=? AND host_id=?");
    query.addInt64(router_id);
    query.addUInt64(host_id);

    if (query.next() != SqlQuery::StepResult::ROW)
        return std::nullopt;

    return readRouterHost(query);
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
bool Database::import(const QList<RouterConfig>& routers,
                      const QList<LocalGroupConfig>& local_groups,
                      const QList<LocalHostConfig>& local_hosts,
                      const QList<RouterHostConfig>& router_hosts)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlTransaction transaction(db_);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction";
        return false;
    }

    // What the book held is what the file replaces, and the two have nothing to do with each other.
    // Saved credentials go with their routers, and hosts are deleted on their own, because the ones
    // at the root of the book lie in no group.
    if (!db_.exec("DELETE FROM local_groups") || !db_.exec("DELETE FROM routers") ||
        !db_.exec("DELETE FROM local_hosts"))
    {
        LOG(ERROR) << "Unable to clear the address book:" << db_.lastError();
        return false;
    }

    // The ids the keys of this call turned into. A link that is not one of the keys already names
    // what it means in the address book, so it is left as it is.
    QHash<qint64, qint64> router_ids;
    for (const RouterConfig& router : routers)
    {
        RouterConfig config = router;
        if (!addRouter(config))
            return false;

        router_ids.insert(router.routerId(), config.routerId());
    }

    QHash<qint64, qint64> group_ids;
    for (const LocalGroupConfig& group : local_groups)
    {
        LocalGroupConfig config = group;
        config.setParentId(group_ids.value(group.parentId(), group.parentId()));

        if (!addLocalGroup(config))
            return false;

        group_ids.insert(group.id(), config.id());
    }

    for (const LocalHostConfig& host : local_hosts)
    {
        LocalHostConfig config = host;
        config.setGroupId(group_ids.value(host.groupId(), host.groupId()));
        config.setRouterId(router_ids.value(host.routerId(), host.routerId()));

        if (!config.isValid())
        {
            LOG(ERROR) << "Invalid parameters";
            return false;
        }

        std::optional<QByteArray> encrypted_data = config.encryptedData();
        if (!encrypted_data.has_value())
            return false;

        if (config.guid().isEmpty())
            config.setGuid(QUuid::createUuid().toString(QUuid::WithoutBraces));

        SqlQuery query(db_, "INSERT INTO local_hosts (id, group_id, router_id, name, comment, data, "
                            "create_time, modify_time, connect_time, guid) "
                            "VALUES (NULL, NULLIF(?, 0), ?, ?, ?, ?, ?, ?, ?, ?)");
        query.addInt64(config.groupId());
        query.addInt64(config.routerId());
        query.addText(config.name());
        query.addText(config.comment());
        query.addBlob(*encrypted_data);
        query.addInt64(config.createTime());
        query.addInt64(config.modifyTime());
        query.addInt64(config.connectTime());
        query.addText(config.guid());

        if (!query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return false;
        }
    }

    for (const RouterHostConfig& host : router_hosts)
    {
        RouterHostConfig config = host;
        config.setRouterId(router_ids.value(host.routerId(), host.routerId()));

        if (!addRouterHost(config))
            return false;
    }

    return transaction.commit();
}

//--------------------------------------------------------------------------------------------------
QString Database::displayName() const
{
    return readSetting(kSettingDisplayName);
}

//--------------------------------------------------------------------------------------------------
bool Database::setDisplayName(const QString& name)
{
    return writeSetting(kSettingDisplayName, name);
}

//--------------------------------------------------------------------------------------------------
bool Database::isCheckUpdatesEnabled() const
{
    QString value = readSetting(kSettingCheckUpdates);
    if (value.isEmpty())
        return true;
    return value == "1";
}

//--------------------------------------------------------------------------------------------------
bool Database::setCheckUpdatesEnabled(bool enable)
{
    return writeSetting(kSettingCheckUpdates, enable ? "1" : "0");
}

//--------------------------------------------------------------------------------------------------
QString Database::updateServer() const
{
    QString value = readSetting(kSettingUpdateServer);
    if (value.isEmpty())
        value = QString::fromLatin1(DEFAULT_UPDATE_SERVER);
    return value.toLower();
}

//--------------------------------------------------------------------------------------------------
bool Database::setUpdateServer(const QString& server)
{
    return writeSetting(kSettingUpdateServer, server);
}

//--------------------------------------------------------------------------------------------------
bool Database::isMasterPasswordSet() const
{
    SqlTransaction transaction(db_);
    if (!transaction.begin())
    {
        LOG(ERROR) << "Unable to start transaction:" << db_.lastError();
        return false;
    }

    return !readSetting(kSettingSalt).isEmpty() && !readSetting(kSettingVerifier).isEmpty();
}

//--------------------------------------------------------------------------------------------------
bool Database::reencryptAll(const QList<LocalHostConfig>& local_hosts,
                            const QList<RouterConfig>& routers,
                            const QList<RouterHostConfig>& router_hosts,
                            const QByteArray& salt,
                            const QByteArray& verifier,
                            quint32 version)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlTransaction transaction(db_);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction";
        return false;
    }

    // On any failure the early return skips commit(), so the transaction destructor rolls back and
    // the address book stays fully readable with the old master password.
    //
    // Only the ciphertext is written. Going through modifyHost() would stamp every host as edited
    // now, and the moment a host was last edited is a column of the list the user reads. Names and
    // comments are plain text, so a change of key leaves them alone.
    for (const LocalHostConfig& local_host : local_hosts)
    {
        std::optional<QByteArray> encrypted_data = local_host.encryptedData();
        if (!encrypted_data.has_value())
            return false;

        SqlQuery query(db_, "UPDATE local_hosts SET data=? WHERE id=?");
        query.addBlob(*encrypted_data);
        query.addInt64(local_host.id());

        if (!query.exec())
        {
            LOG(ERROR) << "Unable to re-encrypt host" << local_host.id() << ":" << db_.lastError();
            return false;
        }
    }

    for (const RouterConfig& router : routers)
    {
        if (!modifyRouter(router))
        {
            LOG(ERROR) << "Unable to re-encrypt router:" << router.routerId();
            return false;
        }
    }

    for (const RouterHostConfig& router_host : router_hosts)
    {
        if (!modifyRouterHost(router_host))
        {
            LOG(ERROR) << "Unable to re-encrypt credentials of router host:" << router_host.hostId();
            return false;
        }
    }

    if (!setMasterPassword(salt, verifier, version))
        return false;

    return transaction.commit();
}

//--------------------------------------------------------------------------------------------------
QByteArray Database::masterPasswordSalt() const
{
    return QByteArray::fromBase64(readSetting(kSettingSalt).toLatin1());
}

//--------------------------------------------------------------------------------------------------
QByteArray Database::masterPasswordVerifier() const
{
    return QByteArray::fromBase64(readSetting(kSettingVerifier).toLatin1());
}

//--------------------------------------------------------------------------------------------------
quint32 Database::masterPasswordVersion() const
{
    return readSetting(kSettingVersion).toUInt();
}

//--------------------------------------------------------------------------------------------------
bool Database::isBiometricUnlockEnabled() const
{
    // The presence of a wrapped key is the single source of truth for the feature being enabled.
    return !biometricBlob().isEmpty();
}

//--------------------------------------------------------------------------------------------------
QByteArray Database::biometricBlob() const
{
    return QByteArray::fromBase64(readSetting(kSettingBiometricBlob).toLatin1());
}

//--------------------------------------------------------------------------------------------------
bool Database::setBiometricBlob(const QByteArray& blob)
{
    return writeSetting(kSettingBiometricBlob, QString::fromLatin1(blob.toBase64()));
}

//--------------------------------------------------------------------------------------------------
bool Database::clearBiometricUnlock()
{
    // Overwrite the wrapped key so the stored blob does not survive disabling the feature
    // (the database runs with secure_delete enabled).
    return writeSetting(kSettingBiometricBlob, QString());
}

//--------------------------------------------------------------------------------------------------
// static
void Database::setFilePathForTesting(const QString& file_path)
{
    g_test_file_path = file_path;
    instance().db_.close();
}

//--------------------------------------------------------------------------------------------------
bool Database::openDatabase()
{
    QString dir_path = BasePaths::appUserDataDir();
    if (dir_path.isEmpty())
    {
        LOG(ERROR) << "Invalid directory path";
        return false;
    }

    // Ensure directory exists.
    QFileInfo dir_info(dir_path);
    if (dir_info.exists())
    {
        if (!dir_info.isDir())
        {
            LOG(ERROR) << "Unable to create directory for database. Need to delete file:" << dir_path;
            return false;
        }
    }
    else
    {
        if (!QDir().mkpath(dir_path))
        {
            LOG(ERROR) << "Unable to create directory for database";
            return false;
        }
    }

    QString file_path = filePath();
    if (file_path.isEmpty())
    {
        LOG(ERROR) << "Invalid file path";
        return false;
    }

    return open(file_path);
}

//--------------------------------------------------------------------------------------------------
bool Database::open(const QString& file_path)
{
    LOG(INFO) << (!QFileInfo::exists(file_path) ? "Creating" : "Opening") << "database:" << file_path;

    if (!db_.open(file_path))
        return false;

    if (!db_.exec("PRAGMA secure_delete = ON"))
        LOG(WARNING) << "Unable to enable secure_delete:" << db_.lastError();

    {
        SqlQuery pragma(db_, "PRAGMA quick_check");
        if (pragma.next() == SqlQuery::StepResult::ROW)
        {
            const QString result = pragma.columnText(0);
            if (result != "ok")
                LOG(ERROR) << "Database integrity check failed:" << result;
        }
        else
        {
            LOG(WARNING) << "Unable to run quick_check:" << db_.lastError();
        }
    }

    if (!createTables(db_))
    {
        db_.close();
        return false;
    }

    if (!db_.exec("PRAGMA foreign_keys = ON"))
    {
        LOG(ERROR) << "Unable to enable foreign keys:" << db_.lastError();
        db_.close();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::setMasterPassword(const QByteArray& salt, const QByteArray& verifier, quint32 version)
{
    return writeSetting(kSettingSalt, QString::fromLatin1(salt.toBase64())) &&
           writeSetting(kSettingVerifier, QString::fromLatin1(verifier.toBase64())) &&
           writeSetting(kSettingVersion, QString::number(version));
}

//--------------------------------------------------------------------------------------------------
QString Database::readSetting(const QString& name) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return QString();
    }

    SqlQuery query(db_, "SELECT value FROM settings WHERE name=?");
    query.addText(name);

    if (query.next() != SqlQuery::StepResult::ROW)
        return QString();

    return query.columnText(0);
}

//--------------------------------------------------------------------------------------------------
bool Database::writeSetting(const QString& name, const QString& value)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "INSERT OR REPLACE INTO settings (name, value) VALUES (?, ?)");
    query.addText(name);
    query.addText(value);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return true;
}
