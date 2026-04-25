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

#include "router/database.h"

#include "base/logging.h"
#include "base/files/base_paths.h"

#include <utility>

#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace router {

namespace {

//--------------------------------------------------------------------------------------------------
QSqlDatabase databaseByName(const QString& connection_name)
{
    if (connection_name.isEmpty())
        return QSqlDatabase();

    return QSqlDatabase::database(connection_name, false);
}

//--------------------------------------------------------------------------------------------------
base::User readUser(const QSqlQuery& query)
{
    base::User user;
    user.entry_id = query.value(0).toLongLong();
    user.name = query.value(1).toString();
    user.group = query.value(2).toString();
    user.salt = query.value(3).toByteArray();
    user.verifier = query.value(4).toByteArray();
    user.sessions = query.value(5).toUInt();
    user.flags = query.value(6).toUInt();
    return user;
}

//--------------------------------------------------------------------------------------------------
QSqlDatabase ensureOpenDatabase(const QString& file_path)
{
    const QString connection_name = QStringLiteral("database");

    QSqlDatabase db = databaseByName(connection_name);
    if (db.isValid())
    {
        if (!db.isOpen() && !db.open())
        {
            LOG(ERROR) << "QSqlDatabase::open failed:" << db.lastError().text();
            return QSqlDatabase();
        }

        return db;
    }

    db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection_name);
    db.setDatabaseName(file_path);

    if (!db.open())
    {
        LOG(ERROR) << "QSqlDatabase::open failed:" << db.lastError().text();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(connection_name);
        return QSqlDatabase();
    }

    return db;
}

} // namespace

//--------------------------------------------------------------------------------------------------
Database::Database() = default;

//--------------------------------------------------------------------------------------------------
Database::Database(const QString& connection_name)
    : connection_name_(std::move(connection_name))
{
    DCHECK(!connection_name_.isEmpty());
}

//--------------------------------------------------------------------------------------------------
Database::Database(Database&& other) noexcept
    : connection_name_(std::move(other.connection_name_))
{
    other.connection_name_.clear();
}

//--------------------------------------------------------------------------------------------------
Database& Database::operator=(Database&& other) noexcept
{
    if (this != &other)
        connection_name_ = std::move(other.connection_name_);

    other.connection_name_.clear();
    return *this;
}

//--------------------------------------------------------------------------------------------------
Database::~Database() = default;

//--------------------------------------------------------------------------------------------------
// static
Database Database::create()
{
    QString dir_path = databaseDirectory();
    if (dir_path.isEmpty())
    {
        LOG(ERROR) << "Invalid directory path";
        return Database();
    }

    QFileInfo dir_info(dir_path);

    if (dir_info.exists())
    {
        if (!dir_info.isDir())
        {
            LOG(ERROR) << "Unable to create directory for database. Need to delete file" << dir_path;
            return Database();
        }
    }
    else
    {
        if (!QDir().mkpath(dir_path))
        {
            LOG(ERROR) << "Unable to create directory for database";
            return Database();
        }
    }

    QString file_path = filePath();
    if (file_path.isEmpty())
    {
        LOG(ERROR) << "Invalid file path";
        return Database();
    }

    if (QFileInfo::exists(file_path))
    {
        LOG(ERROR) << "Database file already exists";
        return Database();
    }

    Database db = open();
    if (!db.isValid())
        return Database();

    QSqlDatabase sql_db = databaseByName(db.connection_name_);
    if (!sql_db.isValid())
    {
        LOG(ERROR) << "Invalid database connection";
        return Database();
    }

    if (!sql_db.transaction())
    {
        LOG(ERROR) << "Unable to execute transaction:" << sql_db.lastError();
        return Database();
    }

    bool success = false;

    do
    {
        QSqlQuery query(sql_db);
        if (!query.exec("CREATE TABLE IF NOT EXISTS \"users\" ("
                        "\"id\" INTEGER UNIQUE,"
                        "\"name\" TEXT NOT NULL UNIQUE,"
                        "\"group\" TEXT NOT NULL,"
                        "\"salt\" BLOB NOT NULL,"
                        "\"verifier\" BLOB NOT NULL,"
                        "\"sessions\" INTEGER DEFAULT 0,"
                        "\"flags\" INTEGER DEFAULT 0,"
                        "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
        {
            LOG(ERROR) << "Unable to execute query:" << query.lastError();
            break;
        }

        if (!query.exec("CREATE TABLE IF NOT EXISTS \"hosts\" ("
                        "\"id\" INTEGER UNIQUE,"
                        "\"key\" BLOB NOT NULL UNIQUE,"
                        "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
        {
            LOG(ERROR) << "Unable to execute query:" << query.lastError();
            break;
        }

        success = true;
    }
    while (false);

    if (!success)
    {
        sql_db.rollback();
        return Database();
    }

    if (!sql_db.commit())
    {
        LOG(ERROR) << "Unable to common transaction:" << sql_db.lastError();
        sql_db.rollback();
        return Database();
    }

    return db;
}

//--------------------------------------------------------------------------------------------------
// static
Database Database::open()
{
    QString file_path = filePath();
    if (file_path.isEmpty())
    {
        LOG(ERROR) << "Invalid file path";
        return Database();
    }

    LOG(INFO) << "Opening database:" << file_path;

    QSqlDatabase db = ensureOpenDatabase(file_path);
    if (!db.isValid() || !db.isOpen())
        return Database();

    return Database(db.connectionName());
}

//--------------------------------------------------------------------------------------------------
// static
QString Database::filePath()
{
    QString file_path = databaseDirectory();
    if (file_path.isEmpty())
        return QString();

    file_path.append(QStringLiteral("/router.db3"));
    return file_path;
}

//--------------------------------------------------------------------------------------------------
bool Database::isValid() const
{
    const QSqlDatabase db = databaseByName(connection_name_);
    return db.isValid() && db.isOpen();
}

//--------------------------------------------------------------------------------------------------
QVector<base::User> Database::userList() const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QSqlQuery query(databaseByName(connection_name_));
    if (!query.exec(QStringLiteral(
        "SELECT id, name, \"group\", salt, verifier, sessions, flags FROM users")))
    {
        LOG(ERROR) << "Unable to get user list:" << query.lastError();
        return {};
    }

    QVector<base::User> users;
    while (query.next())
        users.append(readUser(query));

    return users;
}

//--------------------------------------------------------------------------------------------------
bool Database::addUser(const base::User& user)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (!user.isValid())
    {
        LOG(ERROR) << "Not valid user";
        return false;
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare(QStringLiteral(
        "INSERT INTO users (id, name, \"group\", salt, verifier, sessions, flags) "
        "VALUES (NULL, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(user.name);
    query.addBindValue(user.group);
    query.addBindValue(user.salt);
    query.addBindValue(user.verifier);
    query.addBindValue(static_cast<qulonglong>(user.sessions));
    query.addBindValue(static_cast<qulonglong>(user.flags));

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::modifyUser(const base::User& user)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (!user.isValid())
    {
        LOG(ERROR) << "Not valid user";
        return false;
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare(QStringLiteral(
        "UPDATE users SET name=?, \"group\"=?, salt=?, verifier=?, sessions=?, flags=? WHERE id=?"));
    query.addBindValue(user.name);
    query.addBindValue(user.group);
    query.addBindValue(user.salt);
    query.addBindValue(user.verifier);
    query.addBindValue(static_cast<qulonglong>(user.sessions));
    query.addBindValue(static_cast<qulonglong>(user.flags));
    query.addBindValue(user.entry_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::removeUser(qint64 entry_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare(QStringLiteral("DELETE FROM users WHERE id=?"));
    query.addBindValue(entry_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
base::User Database::findUser(const QString& username) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return base::User::kInvalidUser;
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare(QStringLiteral(
        "SELECT id, name, \"group\", salt, verifier, sessions, flags FROM users WHERE name=?"));
    query.addBindValue(username);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return base::User::kInvalidUser;
    }

    if (!query.next())
        return base::User::kInvalidUser;

    return readUser(query);
}

//--------------------------------------------------------------------------------------------------
Database::ErrorCode Database::hostId(const QByteArray& key_hash, base::HostId* host_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return ErrorCode::UNKNOWN;
    }

    if (key_hash.isEmpty())
    {
        LOG(ERROR) << "Invalid key hash";
        return ErrorCode::UNKNOWN;
    }

    if (!host_id)
    {
        LOG(ERROR) << "Invalid host id";
        return ErrorCode::UNKNOWN;
    }

    *host_id = base::kInvalidHostId;

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare(QStringLiteral("SELECT id FROM hosts WHERE key=?"));
    query.addBindValue(key_hash);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return ErrorCode::UNKNOWN;
    }

    if (!query.next())
        return ErrorCode::NO_HOST_FOUND;

    *host_id = static_cast<base::HostId>(query.value(0).toLongLong());
    return ErrorCode::SUCCESS;
}

//--------------------------------------------------------------------------------------------------
bool Database::addHost(const QByteArray& key_hash)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (key_hash.isEmpty())
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare(QStringLiteral("INSERT INTO hosts (id, key) VALUES (NULL, ?)"));
    query.addBindValue(key_hash);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
// static
QString Database::databaseDirectory()
{
    return base::BasePaths::appDataDir();
}

} // namespace router
