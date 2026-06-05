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

#include "base/logging.h"
#include "base/files/base_paths.h"
#include "base/sql/sql_query.h"
#include "build/build_config.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>

namespace {

constexpr auto kSettingDisplayName  = "display_name";
constexpr auto kSettingCheckUpdates = "check_updates";
constexpr auto kSettingUpdateServer = "update_server";
constexpr auto kSettingSalt         = "master_password_salt";
constexpr auto kSettingVerifier     = "master_password_verifier";
constexpr auto kSettingVersion      = "master_password_version";

//--------------------------------------------------------------------------------------------------
HostConfig readHost(const SqlQuery& query)
{
    HostConfig host;
    host.setId(query.columnInt64(0));
    host.setGroupId(query.columnInt64(1));
    host.setRouterId(query.columnInt64(2));
    host.setEncryptedName(query.columnBlob(3));
    host.setEncryptedComment(query.columnBlob(4));
    host.setEncryptedAddress(query.columnBlob(5));
    host.setEncryptedUsername(query.columnBlob(6));
    host.setEncryptedPassword(query.columnBlob(7));
    host.setCreateTime(query.columnInt64(8));
    host.setModifyTime(query.columnInt64(9));
    host.setConnectTime(query.columnInt64(10));
    return host;
}

//--------------------------------------------------------------------------------------------------
GroupConfig readGroup(const SqlQuery& query)
{
    GroupConfig group;
    group.setId(query.columnInt64(0));
    group.setParentId(query.columnInt64(1));
    group.setEncryptedName(query.columnBlob(2));
    group.setEncryptedComment(query.columnBlob(3));
    return group;
}

//--------------------------------------------------------------------------------------------------
RouterConfig readRouter(const SqlQuery& query)
{
    RouterConfig router;
    router.setRouterId(query.columnInt64(0));
    router.setEncryptedDisplayName(query.columnBlob(1));
    router.setEncryptedAddress(query.columnBlob(2));
    router.setSessionType(static_cast<proto::router::SessionType>(query.columnInt64(3)));
    router.setEncryptedUsername(query.columnBlob(4));
    router.setEncryptedPassword(query.columnBlob(5));
    router.setEncryptedDeviceToken(query.columnBlob(6));
    return router;
}

//--------------------------------------------------------------------------------------------------
bool createTables(SqlDatabase& db)
{
    if (!db.exec("CREATE TABLE IF NOT EXISTS \"groups\" ("
                 "\"id\" INTEGER UNIQUE,"
                 "\"parent_id\" INTEGER NOT NULL DEFAULT 0,"
                 "\"name\" BLOB DEFAULT X'',"
                 "\"comment\" BLOB DEFAULT X'',"
                 "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        LOG(ERROR) << "Unable to create groups table:" << db.lastError();
        return false;
    }

    if (!db.exec("CREATE TABLE IF NOT EXISTS \"hosts\" ("
                 "\"id\" INTEGER UNIQUE,"
                 "\"group_id\" INTEGER NOT NULL DEFAULT 0,"
                 "\"router_id\" INTEGER NOT NULL DEFAULT 0,"
                 "\"name\" BLOB DEFAULT X'',"
                 "\"comment\" BLOB DEFAULT X'',"
                 "\"address\" BLOB DEFAULT X'',"
                 "\"username\" BLOB DEFAULT X'',"
                 "\"password\" BLOB DEFAULT X'',"
                 "\"create_time\" INTEGER NOT NULL DEFAULT 0,"
                 "\"modify_time\" INTEGER NOT NULL DEFAULT 0,"
                 "\"connect_time\" INTEGER NOT NULL DEFAULT 0,"
                 "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        LOG(ERROR) << "Unable to create hosts table:" << db.lastError();
        return false;
    }

    if (!db.exec("CREATE TABLE IF NOT EXISTS \"routers\" ("
                 "\"id\" INTEGER UNIQUE,"
                 "\"name\" BLOB DEFAULT X'',"
                 "\"address\" BLOB DEFAULT X'',"
                 "\"session_type\" INTEGER NOT NULL DEFAULT 0,"
                 "\"username\" BLOB DEFAULT X'',"
                 "\"password\" BLOB DEFAULT X'',"
                 "\"device_token\" BLOB DEFAULT X'',"
                 "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        LOG(ERROR) << "Unable to create routers table:" << db.lastError();
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
QList<HostConfig> Database::hostList(qint64 group_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    SqlQuery query(db_, "SELECT id, group_id, router_id, name, comment, address, username, password, "
                        "create_time, modify_time, connect_time "
                        "FROM hosts WHERE group_id=?");
    query.addInt64(group_id);

    QList<HostConfig> hosts;
    while (query.next())
        hosts.append(readHost(query));

    return hosts;
}

//--------------------------------------------------------------------------------------------------
QList<HostConfig> Database::allHosts() const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    SqlQuery query(db_, "SELECT id, group_id, router_id, name, comment, address, username, password, "
                        "create_time, modify_time, connect_time "
                        "FROM hosts");

    QList<HostConfig> hosts;
    while (query.next())
        hosts.append(readHost(query));

    return hosts;
}

//--------------------------------------------------------------------------------------------------
bool Database::addHost(HostConfig& host)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (host.encryptedName().isEmpty() || host.encryptedAddress().isEmpty() || host.groupId() < 0)
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    const qint64 current_time = QDateTime::currentSecsSinceEpoch();
    host.setCreateTime(current_time);
    host.setModifyTime(current_time);
    host.setConnectTime(0);

    SqlQuery query(db_, "INSERT INTO hosts (id, group_id, router_id, name, comment, address, "
                        "username, password, create_time, modify_time, connect_time) "
                        "VALUES (NULL, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    query.addInt64(host.groupId());
    query.addInt64(host.routerId());
    query.addBlob(host.encryptedName());
    query.addBlob(host.encryptedComment());
    query.addBlob(host.encryptedAddress());
    query.addBlob(host.encryptedUsername());
    query.addBlob(host.encryptedPassword());
    query.addInt64(host.createTime());
    query.addInt64(host.modifyTime());
    query.addInt64(host.connectTime());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    host.setId(db_.lastInsertRowId());
    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::modifyHost(HostConfig& host)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    host.setModifyTime(QDateTime::currentSecsSinceEpoch());

    SqlQuery query(db_, "UPDATE hosts SET group_id=?, router_id=?, name=?, comment=?, address=?, "
                        "username=?, password=?, modify_time=? WHERE id=?");
    query.addInt64(host.groupId());
    query.addInt64(host.routerId());
    query.addBlob(host.encryptedName());
    query.addBlob(host.encryptedComment());
    query.addBlob(host.encryptedAddress());
    query.addBlob(host.encryptedUsername());
    query.addBlob(host.encryptedPassword());
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
bool Database::removeHost(qint64 entry_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "DELETE FROM hosts WHERE id=?");
    query.addInt64(entry_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::setConnectTime(qint64 entry_id, qint64 connect_time)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "UPDATE hosts SET connect_time=? WHERE id=?");
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
std::optional<HostConfig> Database::findHost(qint64 entry_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return std::nullopt;
    }

    SqlQuery query(db_, "SELECT id, group_id, router_id, name, comment, address, username, password, "
                        "create_time, modify_time, connect_time "
                        "FROM hosts WHERE id=?");
    query.addInt64(entry_id);

    if (!query.next())
        return std::nullopt;

    return readHost(query);
}

//--------------------------------------------------------------------------------------------------
QList<HostConfig> Database::searchHosts(const QString& query_text) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    SqlQuery query(db_, "SELECT id, group_id, router_id, name, comment, address, username, password, "
                        "create_time, modify_time, connect_time FROM hosts");

    QList<HostConfig> hosts;
    while (query.next())
    {
        HostConfig host = readHost(query);
        if (host.name().contains(query_text, Qt::CaseInsensitive) ||
            host.address().contains(query_text, Qt::CaseInsensitive) ||
            host.comment().contains(query_text, Qt::CaseInsensitive))
        {
            hosts.append(host);
        }
    }

    return hosts;
}

//--------------------------------------------------------------------------------------------------
QList<GroupConfig> Database::groupList(qint64 parent_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    SqlQuery query(db_, "SELECT id, parent_id, name, comment FROM groups WHERE parent_id=?");
    query.addInt64(parent_id);

    QList<GroupConfig> groups;
    while (query.next())
        groups.append(readGroup(query));

    return groups;
}

//--------------------------------------------------------------------------------------------------
QList<GroupConfig> Database::allGroups() const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    SqlQuery query(db_, "SELECT id, parent_id, name, comment FROM groups");

    QList<GroupConfig> groups;
    while (query.next())
        groups.append(readGroup(query));

    return groups;
}

//--------------------------------------------------------------------------------------------------
bool Database::addGroup(GroupConfig& group)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "INSERT INTO groups (id, parent_id, name, comment) "
                        "VALUES (NULL, ?, ?, ?)");
    query.addInt64(group.parentId());
    query.addBlob(group.encryptedName());
    query.addBlob(group.encryptedComment());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    group.setId(db_.lastInsertRowId());
    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::modifyGroup(const GroupConfig& group)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "UPDATE groups SET parent_id=?, name=?, comment=? WHERE id=?");
    query.addInt64(group.parentId());
    query.addBlob(group.encryptedName());
    query.addBlob(group.encryptedComment());
    query.addInt64(group.id());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::moveGroup(qint64 group_id, qint64 new_parent_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "UPDATE groups SET parent_id=? WHERE id=?");
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
bool Database::removeGroup(qint64 group_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "DELETE FROM groups WHERE id=?");
    query.addInt64(group_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
std::optional<GroupConfig> Database::findGroup(qint64 group_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return std::nullopt;
    }

    SqlQuery query(db_, "SELECT id, parent_id, name, comment FROM groups WHERE id=?");
    query.addInt64(group_id);

    if (!query.next())
        return std::nullopt;

    return readGroup(query);
}

//--------------------------------------------------------------------------------------------------
QList<RouterConfig> Database::routerList() const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    SqlQuery query(db_, "SELECT id, name, address, session_type, username, password, "
                        "device_token FROM routers");

    QList<RouterConfig> routers;
    while (query.next())
        routers.append(readRouter(query));

    return routers;
}

//--------------------------------------------------------------------------------------------------
bool Database::addRouter(RouterConfig& router)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (router.encryptedAddress().isEmpty() || router.encryptedUsername().isEmpty())
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    SqlQuery query(db_, "INSERT INTO routers (id, name, address, session_type, username, password, "
                        "device_token) "
                        "VALUES (NULL, ?, ?, ?, ?, ?, ?)");
    query.addBlob(router.encryptedDisplayName());
    query.addBlob(router.encryptedAddress());
    query.addInt64(static_cast<quint32>(router.sessionType()));
    query.addBlob(router.encryptedUsername());
    query.addBlob(router.encryptedPassword());
    query.addBlob(router.encryptedDeviceToken());

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

    SqlQuery query(db_, "UPDATE routers SET name=?, address=?, session_type=?, username=?, "
                        "password=?, device_token=? WHERE id=?");
    query.addBlob(router.encryptedDisplayName());
    query.addBlob(router.encryptedAddress());
    query.addInt64(static_cast<quint32>(router.sessionType()));
    query.addBlob(router.encryptedUsername());
    query.addBlob(router.encryptedPassword());
    query.addBlob(router.encryptedDeviceToken());
    query.addInt64(router.routerId());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
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

    SqlQuery query(db_, "SELECT id, name, address, session_type, username, password, device_token "
                        "FROM routers WHERE id=?");
    query.addInt64(router_id);

    if (!query.next())
        return std::nullopt;

    return readRouter(query);
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
    return !readSetting(kSettingSalt).isEmpty() && !readSetting(kSettingVerifier).isEmpty();
}

//--------------------------------------------------------------------------------------------------
bool Database::setMasterPassword(const QByteArray& salt, const QByteArray& verifier, quint32 version)
{
    return writeSetting(kSettingSalt, QString::fromLatin1(salt.toBase64())) &&
           writeSetting(kSettingVerifier, QString::fromLatin1(verifier.toBase64())) &&
           writeSetting(kSettingVersion, QString::number(version));
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

    LOG(INFO) << (!QFileInfo::exists(file_path) ? "Creating" : "Opening") << "database:" << file_path;

    if (!db_.open(file_path))
        return false;

    if (!db_.exec("PRAGMA secure_delete = ON"))
        LOG(WARNING) << "Unable to enable secure_delete:" << db_.lastError();

    {
        SqlQuery pragma(db_, "PRAGMA quick_check");
        if (pragma.next())
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

    return true;
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

    if (!query.next())
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
