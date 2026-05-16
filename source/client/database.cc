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
ComputerConfig readComputer(const QSqlQuery& query)
{
    ComputerConfig computer;
    computer.setId(query.value(0).toLongLong());
    computer.setGroupId(query.value(1).toLongLong());
    computer.setRouterId(query.value(2).toLongLong());
    computer.setEncryptedName(query.value(3).toByteArray());
    computer.setEncryptedComment(query.value(4).toByteArray());
    computer.setEncryptedAddress(query.value(5).toByteArray());
    computer.setEncryptedUsername(query.value(6).toByteArray());
    computer.setEncryptedPassword(query.value(7).toByteArray());
    computer.setCreateTime(query.value(8).toLongLong());
    computer.setModifyTime(query.value(9).toLongLong());
    computer.setConnectTime(query.value(10).toLongLong());
    computer.setEncryptedData(query.value(11).toByteArray());
    return computer;
}

//--------------------------------------------------------------------------------------------------
GroupConfig readGroup(const QSqlQuery& query)
{
    GroupConfig group;
    group.setId(query.value(0).toLongLong());
    group.setParentId(query.value(1).toLongLong());
    group.setEncryptedName(query.value(2).toByteArray());
    group.setEncryptedComment(query.value(3).toByteArray());
    group.setEncryptedData(query.value(4).toByteArray());
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
    router.setEncryptedData(query.value(6).toByteArray());
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
                    "\"data\" BLOB DEFAULT X'',"
                    "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        LOG(ERROR) << "Unable to create groups table:" << query.lastError();
        return false;
    }

    if (!query.exec("CREATE TABLE IF NOT EXISTS \"computers\" ("
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
                    "\"data\" BLOB DEFAULT X'',"
                    "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        LOG(ERROR) << "Unable to create computers table:" << query.lastError();
        return false;
    }

    if (!query.exec("CREATE TABLE IF NOT EXISTS \"routers\" ("
                    "\"id\" INTEGER UNIQUE,"
                    "\"name\" BLOB DEFAULT X'',"
                    "\"address\" BLOB DEFAULT X'',"
                    "\"session_type\" INTEGER NOT NULL DEFAULT 0,"
                    "\"username\" BLOB DEFAULT X'',"
                    "\"password\" BLOB DEFAULT X'',"
                    "\"data\" BLOB DEFAULT X'',"
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
QList<ComputerConfig> Database::computerList(qint64 group_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("SELECT id, group_id, router_id, name, comment, address, username, password, "
                  "create_time, modify_time, connect_time, data "
                  "FROM computers WHERE group_id=?");
    query.addBindValue(group_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to get computer list:" << query.lastError();
        return {};
    }

    QList<ComputerConfig> computers;
    while (query.next())
        computers.append(readComputer(query));

    return computers;
}

//--------------------------------------------------------------------------------------------------
QList<ComputerConfig> Database::allComputers() const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("SELECT id, group_id, router_id, name, comment, address, username, password, "
                  "create_time, modify_time, connect_time, data "
                  "FROM computers");

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to get computer list:" << query.lastError();
        return {};
    }

    QList<ComputerConfig> computers;
    while (query.next())
        computers.append(readComputer(query));

    return computers;
}

//--------------------------------------------------------------------------------------------------
bool Database::addComputer(ComputerConfig& computer)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (computer.encryptedName().isEmpty() || computer.encryptedAddress().isEmpty() || computer.groupId() < 0)
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    const qint64 current_time = QDateTime::currentSecsSinceEpoch();
    computer.setCreateTime(current_time);
    computer.setModifyTime(current_time);
    computer.setConnectTime(0);

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("INSERT INTO computers (id, group_id, router_id, name, comment, address, username, password, "
                  "create_time, modify_time, connect_time, data) "
                  "VALUES (NULL, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(computer.groupId());
    query.addBindValue(computer.routerId());
    query.addBindValue(computer.encryptedName());
    query.addBindValue(computer.encryptedComment());
    query.addBindValue(computer.encryptedAddress());
    query.addBindValue(computer.encryptedUsername());
    query.addBindValue(computer.encryptedPassword());
    query.addBindValue(computer.createTime());
    query.addBindValue(computer.modifyTime());
    query.addBindValue(computer.connectTime());
    query.addBindValue(computer.encryptedData());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    computer.setId(query.lastInsertId().toLongLong());
    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::modifyComputer(ComputerConfig& computer)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    computer.setModifyTime(QDateTime::currentSecsSinceEpoch());

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("UPDATE computers SET group_id=?, router_id=?, name=?, comment=?, address=?, username=?, "
                  "password=?, modify_time=?, data=? WHERE id=?");
    query.addBindValue(computer.groupId());
    query.addBindValue(computer.routerId());
    query.addBindValue(computer.encryptedName());
    query.addBindValue(computer.encryptedComment());
    query.addBindValue(computer.encryptedAddress());
    query.addBindValue(computer.encryptedUsername());
    query.addBindValue(computer.encryptedPassword());
    query.addBindValue(computer.modifyTime());
    query.addBindValue(computer.encryptedData());
    query.addBindValue(computer.id());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::removeComputer(qint64 computer_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("DELETE FROM computers WHERE id=?");
    query.addBindValue(computer_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::setConnectTime(qint64 computer_id, qint64 connect_time)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("UPDATE computers SET connect_time=? WHERE id=?");
    query.addBindValue(connect_time);
    query.addBindValue(computer_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
std::optional<ComputerConfig> Database::findComputer(qint64 computer_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return std::nullopt;
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("SELECT id, group_id, router_id, name, comment, address, username, password, "
                  "create_time, modify_time, connect_time, data "
                  "FROM computers WHERE id=?");
    query.addBindValue(computer_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return std::nullopt;
    }

    if (!query.next())
        return std::nullopt;

    return readComputer(query);
}

//--------------------------------------------------------------------------------------------------
QList<ComputerConfig> Database::searchComputers(const QString& query_text) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QSqlQuery query(QSqlDatabase::database(kConnectionName, false));
    query.prepare("SELECT id, group_id, router_id, name, comment, address, username, password, "
                  "create_time, modify_time, connect_time, data FROM computers");

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute search query:" << query.lastError();
        return {};
    }

    QList<ComputerConfig> computers;
    while (query.next())
    {
        ComputerConfig computer = readComputer(query);
        if (computer.name().contains(query_text, Qt::CaseInsensitive) ||
            computer.address().contains(query_text, Qt::CaseInsensitive) ||
            computer.comment().contains(query_text, Qt::CaseInsensitive))
        {
            computers.append(computer);
        }
    }

    return computers;
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
    query.prepare("SELECT id, parent_id, name, comment, data FROM groups WHERE parent_id=?");
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
    if (!query.exec("SELECT id, parent_id, name, comment, data FROM groups"))
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
    query.prepare("INSERT INTO groups (id, parent_id, name, comment, data) "
                  "VALUES (NULL, ?, ?, ?, ?)");
    query.addBindValue(group.parentId());
    query.addBindValue(group.encryptedName());
    query.addBindValue(group.encryptedComment());
    query.addBindValue(group.encryptedData());

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
    query.prepare("UPDATE groups SET parent_id=?, name=?, comment=?, data=? WHERE id=?");
    query.addBindValue(group.parentId());
    query.addBindValue(group.encryptedName());
    query.addBindValue(group.encryptedComment());
    query.addBindValue(group.encryptedData());
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
    query.prepare("SELECT id, parent_id, name, comment, data FROM groups WHERE id=?");
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
    if (!query.exec("SELECT id, name, address, session_type, username, password, data FROM routers"))
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
    query.prepare("INSERT INTO routers (id, name, address, session_type, username, password, data) "
                  "VALUES (NULL, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(router.encryptedDisplayName());
    query.addBindValue(router.encryptedAddress());
    query.addBindValue(static_cast<quint32>(router.sessionType()));
    query.addBindValue(router.encryptedUsername());
    query.addBindValue(router.encryptedPassword());
    query.addBindValue(router.encryptedData());

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
    query.prepare("UPDATE routers SET name=?, address=?, session_type=?, username=?, password=?, data=? "
                  "WHERE id=?");
    query.addBindValue(router.encryptedDisplayName());
    query.addBindValue(router.encryptedAddress());
    query.addBindValue(static_cast<quint32>(router.sessionType()));
    query.addBindValue(router.encryptedUsername());
    query.addBindValue(router.encryptedPassword());
    query.addBindValue(router.encryptedData());
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
    query.prepare("SELECT id, name, address, session_type, username, password, data "
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
