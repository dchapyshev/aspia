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
#include "proto/router_constants.h"

#include <utility>

#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

//--------------------------------------------------------------------------------------------------
QSqlDatabase databaseByName(const QString& connection_name)
{
    if (connection_name.isEmpty())
        return QSqlDatabase();

    return QSqlDatabase::database(connection_name, false);
}

//--------------------------------------------------------------------------------------------------
RouterUser readUser(const QSqlQuery& query)
{
    RouterUser user;
    user.entry_id         = query.value(0).toLongLong();
    user.name             = query.value(1).toString();
    user.group            = query.value(2).toString();
    user.salt             = query.value(3).toByteArray();
    user.verifier         = query.value(4).toByteArray();
    user.sessions         = query.value(5).toUInt();
    user.flags            = query.value(6).toUInt();
    user.public_key       = query.value(7).toByteArray();
    user.wrap_private_key = query.value(8).toByteArray();
    user.wrap_salt        = query.value(9).toByteArray();
    return user;
}

//--------------------------------------------------------------------------------------------------
bool hasColumn(QSqlDatabase& sql_db, const QString& table, const QString& column)
{
    QSqlQuery query(sql_db);
    query.prepare(QStringLiteral("SELECT 1 FROM pragma_table_info(?) WHERE name=?"));
    query.addBindValue(table);
    query.addBindValue(column);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to query table info:" << query.lastError();
        return true; // Pessimistic: pretend it exists, do not attempt ALTER.
    }

    return query.next();
}

//--------------------------------------------------------------------------------------------------
bool ensureSchema(QSqlDatabase& sql_db)
{
    if (!sql_db.transaction())
    {
        LOG(ERROR) << "Unable to execute transaction:" << sql_db.lastError();
        return false;
    }

    QSqlQuery query(sql_db);

    if (!query.exec("CREATE TABLE IF NOT EXISTS \"users\" ("
                    "\"id\" INTEGER UNIQUE,"
                    "\"name\" TEXT NOT NULL UNIQUE,"
                    "\"group\" TEXT NOT NULL,"
                    "\"salt\" BLOB NOT NULL,"
                    "\"verifier\" BLOB NOT NULL,"
                    "\"sessions\" INTEGER DEFAULT 0,"
                    "\"flags\" INTEGER DEFAULT 0,"
                    "\"public_key\" BLOB NOT NULL DEFAULT X'',"
                    "\"wrap_private_key\" BLOB NOT NULL DEFAULT X'',"
                    "\"wrap_salt\" BLOB NOT NULL DEFAULT X'',"
                    "PRIMARY KEY(\"id\" AUTOINCREMENT))") ||
        !query.exec("CREATE TABLE IF NOT EXISTS \"hosts\" ("
                    "\"id\" INTEGER UNIQUE,"
                    "\"key\" BLOB NOT NULL UNIQUE,"
                    "PRIMARY KEY(\"id\" AUTOINCREMENT))") ||
        !query.exec("CREATE TABLE IF NOT EXISTS \"workspaces\" ("
                    "\"id\" INTEGER UNIQUE,"
                    "\"name\" TEXT NOT NULL UNIQUE,"
                    "PRIMARY KEY(\"id\" AUTOINCREMENT))") ||
        !query.exec("CREATE TABLE IF NOT EXISTS \"workspace_access\" ("
                    "\"workspace_id\" INTEGER NOT NULL,"
                    "\"user_id\" INTEGER NOT NULL,"
                    "\"wrapped_gk\" BLOB NOT NULL,"
                    "PRIMARY KEY(\"workspace_id\", \"user_id\"),"
                    "FOREIGN KEY(\"workspace_id\") REFERENCES \"workspaces\"(\"id\") ON DELETE CASCADE,"
                    "FOREIGN KEY(\"user_id\") REFERENCES \"users\"(\"id\") ON DELETE CASCADE)"))
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        sql_db.rollback();
        return false;
    }

    static const struct { const char* name; } kUserColumns[] = {
        { "public_key" },
        { "wrap_private_key" },
        { "wrap_salt" }
    };

    for (const auto& column : kUserColumns)
    {
        if (hasColumn(sql_db, QStringLiteral("users"), QString::fromLatin1(column.name)))
            continue;

        if (!query.exec(QStringLiteral("ALTER TABLE \"users\" ADD COLUMN \"%1\" BLOB NOT NULL DEFAULT X''")
                            .arg(QString::fromLatin1(column.name))))
        {
            LOG(ERROR) << "Unable to add column" << column.name << ":" << query.lastError();
            sql_db.rollback();
            return false;
        }
    }

    if (!sql_db.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << sql_db.lastError();
        sql_db.rollback();
        return false;
    }

    return true;
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

    QSqlQuery pragma(db);
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

    // Foreign keys are off by default in SQLite, enable per-connection so
    // ON DELETE CASCADE on workspace_access actually triggers.
    if (!pragma.exec("PRAGMA foreign_keys = ON"))
        LOG(WARNING) << "Unable to enable foreign keys:" << pragma.lastError();

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

    return open();
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

    if (!ensureSchema(db))
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
QVector<RouterUser> Database::userList() const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QSqlQuery query(databaseByName(connection_name_));
    if (!query.exec(QStringLiteral(
        "SELECT id, name, \"group\", salt, verifier, sessions, flags, "
        "public_key, wrap_private_key, wrap_salt FROM users")))
    {
        LOG(ERROR) << "Unable to get user list:" << query.lastError();
        return {};
    }

    QVector<RouterUser> users;
    while (query.next())
        users.append(readUser(query));

    return users;
}

//--------------------------------------------------------------------------------------------------
bool Database::addUser(const RouterUser& user)
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
        "INSERT INTO users (id, name, \"group\", salt, verifier, sessions, flags, "
        "public_key, wrap_private_key, wrap_salt) "
        "VALUES (NULL, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(user.name);
    query.addBindValue(user.group);
    query.addBindValue(user.salt);
    query.addBindValue(user.verifier);
    query.addBindValue(static_cast<qulonglong>(user.sessions));
    query.addBindValue(static_cast<qulonglong>(user.flags));
    query.addBindValue(user.public_key);
    query.addBindValue(user.wrap_private_key);
    query.addBindValue(user.wrap_salt);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::modifyUser(const RouterUser& user)
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
        "UPDATE users SET name=?, \"group\"=?, salt=?, verifier=?, sessions=?, flags=?, "
        "public_key=?, wrap_private_key=?, wrap_salt=? WHERE id=?"));
    query.addBindValue(user.name);
    query.addBindValue(user.group);
    query.addBindValue(user.salt);
    query.addBindValue(user.verifier);
    query.addBindValue(static_cast<qulonglong>(user.sessions));
    query.addBindValue(static_cast<qulonglong>(user.flags));
    query.addBindValue(user.public_key);
    query.addBindValue(user.wrap_private_key);
    query.addBindValue(user.wrap_salt);
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
RouterUser Database::findUser(const QString& username) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return RouterUser();
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare(QStringLiteral(
        "SELECT id, name, \"group\", salt, verifier, sessions, flags, "
        "public_key, wrap_private_key, wrap_salt FROM users WHERE name=?"));
    query.addBindValue(username);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return RouterUser();
    }

    if (!query.next())
        return RouterUser();

    return readUser(query);
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::hostId(const QByteArray& key_hash, HostId* host_id) const
{
    CHECK(host_id);

    *host_id = kInvalidHostId;

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (key_hash.isEmpty())
    {
        LOG(ERROR) << "Invalid key hash";
        return proto::router::kErrorInvalidData;
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare(QStringLiteral("SELECT id FROM hosts WHERE key=?"));
    query.addBindValue(key_hash);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return proto::router::kErrorInternalError;
    }

    if (!query.next())
        return proto::router::kErrorNotFound;

    *host_id = static_cast<HostId>(query.value(0).toLongLong());
    return proto::router::kErrorOk;
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
QVector<Workspace> Database::workspaceList() const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QSqlQuery query(databaseByName(connection_name_));
    if (!query.exec(QStringLiteral("SELECT id, name FROM workspaces")))
    {
        LOG(ERROR) << "Unable to get workspace list:" << query.lastError();
        return {};
    }

    QVector<Workspace> workspaces;
    while (query.next())
    {
        Workspace workspace;
        workspace.entry_id = query.value(0).toLongLong();
        workspace.name = query.value(1).toString();
        workspaces.append(workspace);
    }

    return workspaces;
}

//--------------------------------------------------------------------------------------------------
Workspace Database::findWorkspace(qint64 entry_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return Workspace();
    }

    if (entry_id <= 0)
    {
        LOG(ERROR) << "Invalid workspace id:" << entry_id;
        return Workspace();
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare(QStringLiteral("SELECT id, name FROM workspaces WHERE id=?"));
    query.addBindValue(entry_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return Workspace();
    }

    if (!query.next())
        return Workspace();

    Workspace workspace;
    workspace.entry_id = query.value(0).toLongLong();
    workspace.name = query.value(1).toString();
    return workspace;
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::addWorkspace(
    const QString& name, const QVector<Workspace::Access>& initial_access, qint64* entry_id)
{
    CHECK(entry_id);

    *entry_id = -1;

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (!Workspace::isValidName(name))
    {
        LOG(ERROR) << "Invalid workspace name:" << name;
        return proto::router::kErrorInvalidData;
    }

    for (const Workspace::Access& access : initial_access)
    {
        if (access.user_id <= 0 || access.wrapped_gk.isEmpty())
        {
            LOG(ERROR) << "Invalid access record";
            return proto::router::kErrorInvalidData;
        }
    }

    QSqlDatabase sql_db = databaseByName(connection_name_);
    if (!sql_db.transaction())
    {
        LOG(ERROR) << "Unable to start transaction:" << sql_db.lastError();
        return proto::router::kErrorInternalError;
    }

    QSqlQuery check(sql_db);
    check.prepare(QStringLiteral("SELECT 1 FROM workspaces WHERE name=?"));
    check.addBindValue(name);

    if (!check.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << check.lastError();
        sql_db.rollback();
        return proto::router::kErrorInternalError;
    }

    if (check.next())
    {
        sql_db.rollback();
        return proto::router::kErrorAlreadyExists;
    }

    QSqlQuery insert_workspace(sql_db);
    insert_workspace.prepare(QStringLiteral("INSERT INTO workspaces (id, name) VALUES (NULL, ?)"));
    insert_workspace.addBindValue(name);

    if (!insert_workspace.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << insert_workspace.lastError();
        sql_db.rollback();
        return proto::router::kErrorInternalError;
    }

    const qint64 new_id = insert_workspace.lastInsertId().toLongLong();

    QSqlQuery insert_access(sql_db);
    insert_access.prepare(QStringLiteral(
        "INSERT INTO workspace_access (workspace_id, user_id, wrapped_gk) VALUES (?, ?, ?)"));

    for (const Workspace::Access& access : initial_access)
    {
        insert_access.bindValue(0, new_id);
        insert_access.bindValue(1, access.user_id);
        insert_access.bindValue(2, access.wrapped_gk);

        if (!insert_access.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << insert_access.lastError();
            sql_db.rollback();
            return proto::router::kErrorInternalError;
        }
    }

    if (!sql_db.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << sql_db.lastError();
        sql_db.rollback();
        return proto::router::kErrorInternalError;
    }

    *entry_id = new_id;
    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::modifyWorkspace(qint64 entry_id, const QString& name)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (entry_id <= 0)
    {
        LOG(ERROR) << "Invalid workspace id:" << entry_id;
        return proto::router::kErrorInvalidData;
    }

    if (!Workspace::isValidName(name))
    {
        LOG(ERROR) << "Invalid workspace name:" << name;
        return proto::router::kErrorInvalidData;
    }

    QSqlQuery check(databaseByName(connection_name_));
    check.prepare(QStringLiteral("SELECT id FROM workspaces WHERE name=?"));
    check.addBindValue(name);

    if (!check.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << check.lastError();
        return proto::router::kErrorInternalError;
    }

    if (check.next() && check.value(0).toLongLong() != entry_id)
        return proto::router::kErrorAlreadyExists;

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare(QStringLiteral("UPDATE workspaces SET name=? WHERE id=?"));
    query.addBindValue(name);
    query.addBindValue(entry_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return proto::router::kErrorInternalError;
    }

    if (query.numRowsAffected() == 0)
        return proto::router::kErrorNotFound;

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::removeWorkspace(qint64 entry_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (entry_id <= 0)
    {
        LOG(ERROR) << "Invalid workspace id:" << entry_id;
        return proto::router::kErrorInvalidData;
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare(QStringLiteral("DELETE FROM workspaces WHERE id=?"));
    query.addBindValue(entry_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return proto::router::kErrorInternalError;
    }

    if (query.numRowsAffected() == 0)
        return proto::router::kErrorNotFound;

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
QVector<Workspace::Access> Database::workspaceAccessList(qint64 workspace_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare(QStringLiteral(
        "SELECT workspace_id, user_id, wrapped_gk FROM workspace_access WHERE workspace_id=?"));
    query.addBindValue(workspace_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to get workspace access list:" << query.lastError();
        return {};
    }

    QVector<Workspace::Access> result;
    while (query.next())
    {
        Workspace::Access access;
        access.workspace_id = query.value(0).toLongLong();
        access.user_id      = query.value(1).toLongLong();
        access.wrapped_gk   = query.value(2).toByteArray();
        result.append(access);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
QVector<Workspace::Access> Database::workspaceAccessListForUser(qint64 user_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare(QStringLiteral(
        "SELECT workspace_id, user_id, wrapped_gk FROM workspace_access WHERE user_id=?"));
    query.addBindValue(user_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to get workspace access list for user:" << query.lastError();
        return {};
    }

    QVector<Workspace::Access> result;
    while (query.next())
    {
        Workspace::Access access;
        access.workspace_id = query.value(0).toLongLong();
        access.user_id      = query.value(1).toLongLong();
        access.wrapped_gk   = query.value(2).toByteArray();
        result.append(access);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::grantWorkspaceAccess(
    qint64 workspace_id, qint64 user_id, const QByteArray& wrapped_gk)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (workspace_id <= 0 || user_id <= 0 || wrapped_gk.isEmpty())
    {
        LOG(ERROR) << "Invalid grant access parameters";
        return proto::router::kErrorInvalidData;
    }

    QSqlQuery check(databaseByName(connection_name_));
    check.prepare(QStringLiteral(
        "SELECT 1 FROM workspace_access WHERE workspace_id=? AND user_id=?"));
    check.addBindValue(workspace_id);
    check.addBindValue(user_id);

    if (!check.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << check.lastError();
        return proto::router::kErrorInternalError;
    }

    if (check.next())
        return proto::router::kErrorAlreadyExists;

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare(QStringLiteral(
        "INSERT INTO workspace_access (workspace_id, user_id, wrapped_gk) VALUES (?, ?, ?)"));
    query.addBindValue(workspace_id);
    query.addBindValue(user_id);
    query.addBindValue(wrapped_gk);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return proto::router::kErrorInternalError;
    }

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::revokeWorkspaceAccess(qint64 workspace_id, qint64 user_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (workspace_id <= 0 || user_id <= 0)
    {
        LOG(ERROR) << "Invalid revoke access parameters";
        return proto::router::kErrorInvalidData;
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare(QStringLiteral(
        "DELETE FROM workspace_access WHERE workspace_id=? AND user_id=?"));
    query.addBindValue(workspace_id);
    query.addBindValue(user_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return proto::router::kErrorInternalError;
    }

    if (query.numRowsAffected() == 0)
        return proto::router::kErrorNotFound;

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
// static
QString Database::databaseDirectory()
{
    return BasePaths::appDataDir();
}
