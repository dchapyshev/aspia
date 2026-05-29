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
#include "build/build_config.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

const char kConnectionName[] = "client";

constexpr auto kSettingDisplayName  = "display_name";
constexpr auto kSettingCheckUpdates = "check_updates";
constexpr auto kSettingUpdateServer = "update_server";
constexpr auto kSettingSalt         = "master_password_salt";
constexpr auto kSettingVerifier     = "master_password_verifier";
constexpr auto kSettingVersion      = "master_password_version";

//--------------------------------------------------------------------------------------------------
HostConfig readHost(const QSqlQuery& query)
{
    HostConfig host;
    host.setId(query.value(0).toLongLong());
    host.setGroupId(query.value(1).toLongLong());
    host.setRouterId(query.value(2).toLongLong());
    host.setEncryptedName(query.value(3).toByteArray());
    host.setEncryptedComment(query.value(4).toByteArray());
    host.setEncryptedAddress(query.value(5).toByteArray());
    host.setEncryptedUsername(query.value(6).toByteArray());
    host.setEncryptedPassword(query.value(7).toByteArray());
    host.setCreateTime(query.value(8).toLongLong());
    host.setModifyTime(query.value(9).toLongLong());
    host.setConnectTime(query.value(10).toLongLong());
    return host;
}

//--------------------------------------------------------------------------------------------------
GroupConfig readGroup(const QSqlQuery& query)
{
    GroupConfig group;
    group.setId(query.value(0).toLongLong());
    group.setParentId(query.value(1).toLongLong());
    group.setEncryptedName(query.value(2).toByteArray());
    group.setEncryptedComment(query.value(3).toByteArray());
    return group;
}

//--------------------------------------------------------------------------------------------------
RouterConfig readRouter(const QSqlQuery& query)
{
    RouterConfig router;
    router.setRouterId(query.value(0).toLongLong());
    router.setEncryptedDisplayName(query.value(1).toByteArray());
    router.setEncryptedAddress(query.value(2).toByteArray());
    router.setSessionType(static_cast<proto::router::SessionType>(query.value(3).toUInt()));
    router.setEncryptedUsername(query.value(4).toByteArray());
    router.setEncryptedPassword(query.value(5).toByteArray());
    router.setEncryptedDevicePrivateKey(query.value(6).toByteArray());
    router.setEncryptedDeviceTokenId(query.value(7).toByteArray());
    return router;
}

//--------------------------------------------------------------------------------------------------
bool createTables(QSqlDatabase& db)
{
    QSqlQuery query(db);

    if (!query.exec("CREATE TABLE IF NOT EXISTS \"groups\" ("
                    "\"id\" INTEGER UNIQUE,"
                    "\"parent_id\" INTEGER NOT NULL DEFAULT 0,"
                    "\"name\" BLOB DEFAULT X'',"
                    "\"comment\" BLOB DEFAULT X'',"
                    "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        LOG(ERROR) << "Unable to create groups table:" << query.lastError();
        return false;
    }

    if (!query.exec("CREATE TABLE IF NOT EXISTS \"hosts\" ("
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
        LOG(ERROR) << "Unable to create hosts table:" << query.lastError();
        return false;
    }

    if (!query.exec("CREATE TABLE IF NOT EXISTS \"routers\" ("
                    "\"id\" INTEGER UNIQUE,"
                    "\"name\" BLOB DEFAULT X'',"
                    "\"address\" BLOB DEFAULT X'',"
                    "\"session_type\" INTEGER NOT NULL DEFAULT 0,"
                    "\"username\" BLOB DEFAULT X'',"
                    "\"password\" BLOB DEFAULT X'',"
                    "\"device_private_key\" BLOB DEFAULT X'',"
                    "\"device_token_id\" BLOB DEFAULT X'',"
                    "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        LOG(ERROR) << "Unable to create routers table:" << query.lastError();
        return false;
    }

    if (!query.exec("CREATE TABLE IF NOT EXISTS \"settings\" ("
                    "\"name\" TEXT PRIMARY KEY NOT NULL,"
                    "\"value\" TEXT NOT NULL)"))
    {
        LOG(ERROR) << "Unable to create settings table:" << query.lastError();
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

    if (!database.valid_)
        database.valid_ = database.openDatabase();

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
    return valid_;
}

//--------------------------------------------------------------------------------------------------
QList<HostConfig> Database::hostList(qint64 group_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("SELECT id, group_id, router_id, name, comment, address, username, password, "
                  "create_time, modify_time, connect_time "
                  "FROM hosts WHERE group_id=?");
    query.addBindValue(group_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to get host list:" << query.lastError();
        return {};
    }

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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("SELECT id, group_id, router_id, name, comment, address, username, password, "
                  "create_time, modify_time, connect_time "
                  "FROM hosts");

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to get host list:" << query.lastError();
        return {};
    }

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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("INSERT INTO hosts (id, group_id, router_id, name, comment, address, username, password, "
                  "create_time, modify_time, connect_time) "
                  "VALUES (NULL, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(host.groupId());
    query.addBindValue(host.routerId());
    query.addBindValue(host.encryptedName());
    query.addBindValue(host.encryptedComment());
    query.addBindValue(host.encryptedAddress());
    query.addBindValue(host.encryptedUsername());
    query.addBindValue(host.encryptedPassword());
    query.addBindValue(host.createTime());
    query.addBindValue(host.modifyTime());
    query.addBindValue(host.connectTime());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    host.setId(query.lastInsertId().toLongLong());
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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("UPDATE hosts SET group_id=?, router_id=?, name=?, comment=?, address=?, username=?, "
                  "password=?, modify_time=? WHERE id=?");
    query.addBindValue(host.groupId());
    query.addBindValue(host.routerId());
    query.addBindValue(host.encryptedName());
    query.addBindValue(host.encryptedComment());
    query.addBindValue(host.encryptedAddress());
    query.addBindValue(host.encryptedUsername());
    query.addBindValue(host.encryptedPassword());
    query.addBindValue(host.modifyTime());
    query.addBindValue(host.id());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("DELETE FROM hosts WHERE id=?");
    query.addBindValue(entry_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("UPDATE hosts SET connect_time=? WHERE id=?");
    query.addBindValue(connect_time);
    query.addBindValue(entry_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("SELECT id, group_id, router_id, name, comment, address, username, password, "
                  "create_time, modify_time, connect_time "
                  "FROM hosts WHERE id=?");
    query.addBindValue(entry_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return std::nullopt;
    }

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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("SELECT id, group_id, router_id, name, comment, address, username, password, "
                  "create_time, modify_time, connect_time FROM hosts");

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute search query:" << query.lastError();
        return {};
    }

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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("SELECT id, parent_id, name, comment FROM groups WHERE parent_id=?");
    query.addBindValue(parent_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to get group list:" << query.lastError();
        return {};
    }

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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    if (!query.exec("SELECT id, parent_id, name, comment FROM groups"))
    {
        LOG(ERROR) << "Unable to get all groups:" << query.lastError();
        return {};
    }

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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("INSERT INTO groups (id, parent_id, name, comment) "
                  "VALUES (NULL, ?, ?, ?)");
    query.addBindValue(group.parentId());
    query.addBindValue(group.encryptedName());
    query.addBindValue(group.encryptedComment());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    group.setId(query.lastInsertId().toLongLong());
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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("UPDATE groups SET parent_id=?, name=?, comment=? WHERE id=?");
    query.addBindValue(group.parentId());
    query.addBindValue(group.encryptedName());
    query.addBindValue(group.encryptedComment());
    query.addBindValue(group.id());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("UPDATE groups SET parent_id=? WHERE id=?");
    query.addBindValue(new_parent_id);
    query.addBindValue(group_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("DELETE FROM groups WHERE id=?");
    query.addBindValue(group_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("SELECT id, parent_id, name, comment FROM groups WHERE id=?");
    query.addBindValue(group_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return std::nullopt;
    }

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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    if (!query.exec("SELECT id, name, address, session_type, username, password, "
                    "device_private_key, device_token_id FROM routers"))
    {
        LOG(ERROR) << "Unable to get router list:" << query.lastError();
        return {};
    }

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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("INSERT INTO routers (id, name, address, session_type, username, password, "
                  "device_private_key, device_token_id) "
                  "VALUES (NULL, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(router.encryptedDisplayName());
    query.addBindValue(router.encryptedAddress());
    query.addBindValue(static_cast<quint32>(router.sessionType()));
    query.addBindValue(router.encryptedUsername());
    query.addBindValue(router.encryptedPassword());
    query.addBindValue(router.encryptedDevicePrivateKey());
    query.addBindValue(router.encryptedDeviceTokenId());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    router.setRouterId(query.lastInsertId().toLongLong());
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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("UPDATE routers SET name=?, address=?, session_type=?, username=?, password=?, "
                  "device_private_key=?, device_token_id=? "
                  "WHERE id=?");
    query.addBindValue(router.encryptedDisplayName());
    query.addBindValue(router.encryptedAddress());
    query.addBindValue(static_cast<quint32>(router.sessionType()));
    query.addBindValue(router.encryptedUsername());
    query.addBindValue(router.encryptedPassword());
    query.addBindValue(router.encryptedDevicePrivateKey());
    query.addBindValue(router.encryptedDeviceTokenId());
    query.addBindValue(router.routerId());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("DELETE FROM routers WHERE id=?");
    query.addBindValue(router_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("SELECT id, name, address, session_type, username, password, "
                  "device_private_key, device_token_id "
                  "FROM routers WHERE id=?");
    query.addBindValue(router_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return std::nullopt;
    }

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

    QSqlDatabase db = QSqlDatabase::database(kConnectionName, false);
    if (!db.isValid())
    {
        db = QSqlDatabase::addDatabase("QSQLITE", kConnectionName);
        db.setDatabaseName(file_path);
    }

    if (!db.isOpen() && !db.open())
    {
        LOG(ERROR) << "QSqlDatabase::open failed:" << db.lastError();
        return false;
    }

    QSqlQuery pragma(db);

    if (!pragma.exec("PRAGMA secure_delete = ON"))
        LOG(WARNING) << "Unable to enable secure_delete:" << pragma.lastError();

    if (pragma.exec("PRAGMA quick_check") && pragma.next())
    {
        const QString result = pragma.value(0).toString();
        if (result != "ok")
            LOG(ERROR) << "Database integrity check failed:" << result;
    }
    else
    {
        LOG(WARNING) << "Unable to run quick_check:" << pragma.lastError();
    }

    if (!createTables(db))
    {
        db.close();
        QSqlDatabase::removeDatabase(kConnectionName);
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

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("SELECT value FROM settings WHERE name=?");
    query.addBindValue(name);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return QString();
    }

    if (!query.next())
        return QString();

    return query.value(0).toString();
}

//--------------------------------------------------------------------------------------------------
bool Database::writeSetting(const QString& name, const QString& value)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("INSERT OR REPLACE INTO settings (name, value) VALUES (?, ?)");
    query.addBindValue(name);
    query.addBindValue(value);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    return true;
}
