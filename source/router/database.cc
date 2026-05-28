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

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSet>
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
        // workspace_id == 0 means the host is not yet assigned to any workspace; group_id == 0
        // means the host is shown at the workspace root. No FKs on these columns because 0 is a
        // sentinel value; for any non-zero value the application enforces that it points to an
        // existing row in workspaces/host_groups.
        // comment, user_name and password are AEAD-encrypted with the workspace GK (only
        // meaningful when workspace_id != 0). name, computer_name (real OS hostname), cpu_arch,
        // version, os_name, address and last_connect are plain values; the latter five are
        // updated by the router on every host connection and reflect the latest connect attempt.
        !query.exec("CREATE TABLE IF NOT EXISTS \"hosts\" ("
                    "\"id\" INTEGER UNIQUE,"
                    "\"key\" BLOB NOT NULL UNIQUE,"
                    "\"workspace_id\" INTEGER NOT NULL DEFAULT 0,"
                    "\"group_id\" INTEGER NOT NULL DEFAULT 0,"
                    "\"display_name\" TEXT NOT NULL DEFAULT '',"
                    "\"computer_name\" TEXT NOT NULL DEFAULT '',"
                    "\"cpu_arch\" TEXT NOT NULL DEFAULT '',"
                    "\"version\" TEXT NOT NULL DEFAULT '',"
                    "\"os_name\" TEXT NOT NULL DEFAULT '',"
                    "\"address\" TEXT NOT NULL DEFAULT '',"
                    "\"comment\" BLOB NOT NULL DEFAULT X'',"
                    "\"user_name\" BLOB NOT NULL DEFAULT X'',"
                    "\"password\" BLOB NOT NULL DEFAULT X'',"
                    "\"last_connect\" INTEGER NOT NULL DEFAULT 0,"
                    "\"last_modify\" INTEGER NOT NULL DEFAULT 0,"
                    "PRIMARY KEY(\"id\" AUTOINCREMENT))") ||
        // comment is AEAD-encrypted with the workspace GK; name is plain text.
        !query.exec("CREATE TABLE IF NOT EXISTS \"workspaces\" ("
                    "\"id\" INTEGER UNIQUE,"
                    "\"name\" TEXT NOT NULL UNIQUE,"
                    "\"comment\" BLOB NOT NULL DEFAULT X'',"
                    "PRIMARY KEY(\"id\" AUTOINCREMENT))") ||
        !query.exec("CREATE TABLE IF NOT EXISTS \"workspace_access\" ("
                    "\"workspace_id\" INTEGER NOT NULL,"
                    "\"user_id\" INTEGER NOT NULL,"
                    "\"wrapped_gk\" BLOB NOT NULL,"
                    "PRIMARY KEY(\"workspace_id\", \"user_id\"),"
                    "FOREIGN KEY(\"workspace_id\") REFERENCES \"workspaces\"(\"id\") ON DELETE CASCADE,"
                    "FOREIGN KEY(\"user_id\") REFERENCES \"users\"(\"id\") ON DELETE CASCADE)") ||
        !query.exec("CREATE TABLE IF NOT EXISTS \"host_groups\" ("
                    "\"id\" INTEGER PRIMARY KEY AUTOINCREMENT,"
                    "\"workspace_id\" INTEGER NOT NULL,"
                    "\"parent_id\" INTEGER,"
                    "\"name\" TEXT NOT NULL,"
                    "\"comment\" BLOB NOT NULL DEFAULT X'',"
                    "FOREIGN KEY(\"workspace_id\") REFERENCES \"workspaces\"(\"id\") ON DELETE CASCADE,"
                    "FOREIGN KEY(\"parent_id\") REFERENCES \"host_groups\"(\"id\") ON DELETE CASCADE)") ||
        !query.exec("CREATE INDEX IF NOT EXISTS \"host_groups_workspace_id\" "
                    "ON \"host_groups\"(\"workspace_id\")") ||
        !query.exec("CREATE INDEX IF NOT EXISTS \"host_groups_parent_id\" "
                    "ON \"host_groups\"(\"parent_id\")") ||
        // Pending host removals. When an admin requests host removal we move the row from hosts
        // into this table and wait for the host to acknowledge the remove command.
        !query.exec("CREATE TABLE IF NOT EXISTS \"hosts_remove\" ("
                    "\"host_id\" INTEGER PRIMARY KEY,"
                    "\"key\" BLOB NOT NULL UNIQUE,"
                    "\"timestamp\" INTEGER NOT NULL DEFAULT 0)"))
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

    // hosts used to be a thin (id, key) table; everything below moved in from the now-gone
    // computers table. Backfill any column that's missing on upgraded databases.
    static const struct { const char* name; const char* definition; } kHostColumns[] = {
        { "workspace_id",  "INTEGER NOT NULL DEFAULT 0"  },
        { "group_id",      "INTEGER NOT NULL DEFAULT 0"  },
        { "display_name",  "TEXT NOT NULL DEFAULT ''"    },
        { "computer_name", "TEXT NOT NULL DEFAULT ''"    },
        { "cpu_arch",      "TEXT NOT NULL DEFAULT ''"    },
        { "version",       "TEXT NOT NULL DEFAULT ''"    },
        { "os_name",       "TEXT NOT NULL DEFAULT ''"    },
        { "address",       "TEXT NOT NULL DEFAULT ''"    },
        { "comment",       "BLOB NOT NULL DEFAULT X''"   },
        { "user_name",     "BLOB NOT NULL DEFAULT X''"   },
        { "password",      "BLOB NOT NULL DEFAULT X''"   },
        { "last_connect",  "INTEGER NOT NULL DEFAULT 0"  },
        { "last_modify",   "INTEGER NOT NULL DEFAULT 0"  }
    };

    for (const auto& column : kHostColumns)
    {
        if (hasColumn(sql_db, QStringLiteral("hosts"), QString::fromLatin1(column.name)))
            continue;

        if (!query.exec(QStringLiteral("ALTER TABLE \"hosts\" ADD COLUMN \"%1\" %2")
                            .arg(QString::fromLatin1(column.name))
                            .arg(QString::fromLatin1(column.definition))))
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
RouterUser Database::findUser(qint64 entry_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return RouterUser();
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare(QStringLiteral(
        "SELECT id, name, \"group\", salt, verifier, sessions, flags, "
        "public_key, wrap_private_key, wrap_salt FROM users WHERE id=?"));
    query.addBindValue(entry_id);

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

    if (query.next())
    {
        *host_id = query.value(0).toULongLong();
        return proto::router::kErrorOk;
    }

    // Not found in hosts - check hosts_remove. A reconnecting host whose removal was scheduled
    // while it was offline will be matched here; the caller is expected to send the remove
    // command using the returned host_id and wait for the ack before finalizing the deletion.
    QSqlQuery pending(databaseByName(connection_name_));
    pending.prepare(QStringLiteral("SELECT host_id FROM hosts_remove WHERE key=?"));
    pending.addBindValue(key_hash);

    if (!pending.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << pending.lastError();
        return proto::router::kErrorInternalError;
    }

    if (!pending.next())
        return proto::router::kErrorNotFound;

    *host_id = pending.value(0).toULongLong();
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
bool Database::updateHostInfo(HostId host_id, const QString& computer_name, const QString& cpu_arch,
    const QString& version, const QString& os_name, const QString& address)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (host_id == kInvalidHostId)
    {
        LOG(ERROR) << "Invalid host id";
        return false;
    }

    const qint64 timestamp = QDateTime::currentSecsSinceEpoch();

    // The hosts row is created by addHost() before this method runs, so a plain UPDATE is
    // enough. If display name has never been set by the admin we seed it from computer_name so
    // the host has a readable label in the UI.
    QSqlQuery query(databaseByName(connection_name_));
    query.prepare(QStringLiteral(
        "UPDATE hosts SET "
        "display_name = CASE WHEN display_name='' THEN ? ELSE display_name END, "
        "computer_name=?, cpu_arch=?, version=?, os_name=?, address=?, last_connect=? "
        "WHERE id=?"));
    query.addBindValue(computer_name);
    query.addBindValue(computer_name);
    query.addBindValue(cpu_arch);
    query.addBindValue(version);
    query.addBindValue(os_name);
    query.addBindValue(address);
    query.addBindValue(timestamp);
    query.addBindValue(host_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
qint64 Database::hostWorkspaceId(HostId host_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return -1;
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare(QStringLiteral("SELECT workspace_id FROM hosts WHERE id=?"));
    query.addBindValue(host_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return -1;
    }

    if (!query.next())
        return -1;
    return query.value(0).toLongLong();
}

//--------------------------------------------------------------------------------------------------
bool Database::modifyHost(HostId host_id, const QString& display_name, const QByteArray& comment,
    const QByteArray& user_name, const QByteArray& password)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (host_id == kInvalidHostId)
    {
        LOG(ERROR) << "Invalid host id";
        return false;
    }

    const qint64 timestamp = QDateTime::currentSecsSinceEpoch();

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare(QStringLiteral(
        "UPDATE hosts SET display_name=?, comment=?, user_name=?, password=?, last_modify=? "
        "WHERE id=?"));
    query.addBindValue(display_name);
    query.addBindValue(comment);
    query.addBindValue(user_name);
    query.addBindValue(password);
    query.addBindValue(timestamp);
    query.addBindValue(host_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    return query.numRowsAffected() > 0;
}

//--------------------------------------------------------------------------------------------------
bool Database::scheduleHostRemoval(HostId host_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (host_id == kInvalidHostId)
    {
        LOG(ERROR) << "Invalid host id";
        return false;
    }

    QSqlDatabase sql_db = databaseByName(connection_name_);
    if (!sql_db.transaction())
    {
        LOG(ERROR) << "Unable to start transaction:" << sql_db.lastError();
        return false;
    }

    QSqlQuery select(sql_db);
    select.prepare(QStringLiteral("SELECT key FROM hosts WHERE id=?"));
    select.addBindValue(host_id);

    if (!select.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << select.lastError();
        sql_db.rollback();
        return false;
    }

    if (!select.next())
    {
        LOG(ERROR) << "Host not found:" << host_id;
        sql_db.rollback();
        return false;
    }

    const QByteArray key = select.value(0).toByteArray();
    const qint64 timestamp = QDateTime::currentSecsSinceEpoch();

    QSqlQuery insert(sql_db);
    insert.prepare(QStringLiteral(
        "INSERT INTO hosts_remove (host_id, key, timestamp) VALUES (?, ?, ?)"));
    insert.addBindValue(host_id);
    insert.addBindValue(key);
    insert.addBindValue(timestamp);

    if (!insert.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << insert.lastError();
        sql_db.rollback();
        return false;
    }

    QSqlQuery del(sql_db);
    del.prepare(QStringLiteral("DELETE FROM hosts WHERE id=?"));
    del.addBindValue(host_id);

    if (!del.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << del.lastError();
        sql_db.rollback();
        return false;
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
bool Database::hasPendingHostRemoval(HostId host_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (host_id == kInvalidHostId)
        return false;

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare(QStringLiteral("SELECT 1 FROM hosts_remove WHERE host_id=?"));
    query.addBindValue(host_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    return query.next();
}

//--------------------------------------------------------------------------------------------------
bool Database::finalizeHostRemoval(HostId host_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (host_id == kInvalidHostId)
    {
        LOG(ERROR) << "Invalid host id";
        return false;
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare(QStringLiteral("DELETE FROM hosts_remove WHERE host_id=?"));
    query.addBindValue(host_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    return query.numRowsAffected() > 0;
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
    if (!query.exec(QStringLiteral("SELECT id, name, comment FROM workspaces")))
    {
        LOG(ERROR) << "Unable to get workspace list:" << query.lastError();
        return {};
    }

    QVector<Workspace> workspaces;
    while (query.next())
    {
        Workspace workspace;
        workspace.entry_id = query.value(0).toLongLong();
        workspace.name     = query.value(1).toString();
        workspace.comment  = query.value(2).toByteArray();
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
    query.prepare(QStringLiteral("SELECT id, name, comment FROM workspaces WHERE id=?"));
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
    workspace.name     = query.value(1).toString();
    workspace.comment  = query.value(2).toByteArray();
    return workspace;
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::addWorkspace(const QString& name, const QByteArray& comment,
    const QVector<Workspace::Access>& initial_access, qint64* entry_id)
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
    insert_workspace.prepare(QStringLiteral(
        "INSERT INTO workspaces (id, name, comment) VALUES (NULL, ?, ?)"));
    insert_workspace.addBindValue(name);
    insert_workspace.addBindValue(comment);

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
std::string_view Database::modifyWorkspace(qint64 entry_id, const QString& name, const QByteArray& comment,
    const QVector<Workspace::Access>& desired_access)
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

    QSet<qint64> desired_ids;
    desired_ids.reserve(desired_access.size());
    for (const Workspace::Access& access : desired_access)
    {
        if (access.user_id <= 0)
        {
            LOG(ERROR) << "Invalid access record (user_id <= 0)";
            return proto::router::kErrorInvalidData;
        }

        if (desired_ids.contains(access.user_id))
        {
            LOG(ERROR) << "Duplicate user_id in desired access list:" << access.user_id;
            return proto::router::kErrorInvalidData;
        }

        desired_ids.insert(access.user_id);
    }

    QSqlDatabase sql_db = databaseByName(connection_name_);
    if (!sql_db.transaction())
    {
        LOG(ERROR) << "Unable to start transaction:" << sql_db.lastError();
        return proto::router::kErrorInternalError;
    }

    QSqlQuery exists_check(sql_db);
    exists_check.prepare(QStringLiteral("SELECT 1 FROM workspaces WHERE id=?"));
    exists_check.addBindValue(entry_id);

    if (!exists_check.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << exists_check.lastError();
        sql_db.rollback();
        return proto::router::kErrorInternalError;
    }

    if (!exists_check.next())
    {
        sql_db.rollback();
        return proto::router::kErrorNotFound;
    }

    QSqlQuery name_check(sql_db);
    name_check.prepare(QStringLiteral("SELECT id FROM workspaces WHERE name=?"));
    name_check.addBindValue(name);

    if (!name_check.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << name_check.lastError();
        sql_db.rollback();
        return proto::router::kErrorInternalError;
    }

    if (name_check.next() && name_check.value(0).toLongLong() != entry_id)
    {
        sql_db.rollback();
        return proto::router::kErrorAlreadyExists;
    }

    QSqlQuery update_workspace(sql_db);
    update_workspace.prepare(QStringLiteral("UPDATE workspaces SET name=?, comment=? WHERE id=?"));
    update_workspace.addBindValue(name);
    update_workspace.addBindValue(comment);
    update_workspace.addBindValue(entry_id);

    if (!update_workspace.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << update_workspace.lastError();
        sql_db.rollback();
        return proto::router::kErrorInternalError;
    }

    QSqlQuery select_current(sql_db);
    select_current.prepare(QStringLiteral(
        "SELECT user_id FROM workspace_access WHERE workspace_id=?"));
    select_current.addBindValue(entry_id);

    if (!select_current.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << select_current.lastError();
        sql_db.rollback();
        return proto::router::kErrorInternalError;
    }

    QSet<qint64> current_ids;
    while (select_current.next())
        current_ids.insert(select_current.value(0).toLongLong());

    QSqlQuery delete_access(sql_db);
    delete_access.prepare(QStringLiteral(
        "DELETE FROM workspace_access WHERE workspace_id=? AND user_id=?"));

    for (qint64 user_id : std::as_const(current_ids))
    {
        if (desired_ids.contains(user_id))
            continue;

        delete_access.bindValue(0, entry_id);
        delete_access.bindValue(1, user_id);

        if (!delete_access.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << delete_access.lastError();
            sql_db.rollback();
            return proto::router::kErrorInternalError;
        }
    }

    QSqlQuery insert_access(sql_db);
    insert_access.prepare(QStringLiteral(
        "INSERT INTO workspace_access (workspace_id, user_id, wrapped_gk) VALUES (?, ?, ?)"));

    for (const Workspace::Access& access : desired_access)
    {
        if (current_ids.contains(access.user_id))
            continue;

        if (access.wrapped_gk.isEmpty())
        {
            LOG(ERROR) << "Missing wrapped_gk for new access entry, user_id:" << access.user_id;
            sql_db.rollback();
            return proto::router::kErrorInvalidData;
        }

        insert_access.bindValue(0, entry_id);
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

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::setWorkspaceHosts(qint64 entry_id, const QSet<HostId>& desired_host_ids)
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

    for (HostId host_id : std::as_const(desired_host_ids))
    {
        if (host_id == kInvalidHostId)
        {
            LOG(ERROR) << "Invalid host_id in desired list:" << host_id;
            return proto::router::kErrorInvalidData;
        }
    }

    QSqlDatabase sql_db = databaseByName(connection_name_);
    if (!sql_db.transaction())
    {
        LOG(ERROR) << "Unable to start transaction:" << sql_db.lastError();
        return proto::router::kErrorInternalError;
    }

    // Release: hosts currently in this workspace but no longer wanted.
    QSqlQuery select_current(sql_db);
    select_current.prepare(QStringLiteral("SELECT id FROM hosts WHERE workspace_id=?"));
    select_current.addBindValue(entry_id);
    if (!select_current.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << select_current.lastError();
        sql_db.rollback();
        return proto::router::kErrorInternalError;
    }

    QSqlQuery release(sql_db);
    release.prepare(QStringLiteral("UPDATE hosts SET workspace_id=0, group_id=0 WHERE id=?"));
    while (select_current.next())
    {
        const HostId host_id = select_current.value(0).toULongLong();
        if (desired_host_ids.contains(host_id))
            continue;

        release.bindValue(0, host_id);
        if (!release.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << release.lastError();
            sql_db.rollback();
            return proto::router::kErrorInternalError;
        }
    }

    // Claim: hosts the operator wants in this workspace. Only hosts that are unassigned or
    // already in this workspace are touched; hosts in another workspace stay put.
    QSqlQuery claim(sql_db);
    claim.prepare(QStringLiteral(
        "UPDATE hosts SET workspace_id=? WHERE id=? AND workspace_id IN (0, ?)"));
    for (HostId host_id : std::as_const(desired_host_ids))
    {
        claim.bindValue(0, entry_id);
        claim.bindValue(1, host_id);
        claim.bindValue(2, entry_id);
        if (!claim.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << claim.lastError();
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
QVector<Group> Database::groupList(qint64 workspace_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    if (workspace_id <= 0)
    {
        LOG(ERROR) << "Invalid workspace id:" << workspace_id;
        return {};
    }

    // Read every group of this workspace. parent_id is NULL for root groups in storage; we
    // coalesce it to 0 to match the Group struct convention used everywhere in the API. Rows
    // come back in whatever order SQLite produces them. Display ordering (alphabetic,
    // locale-aware, and so on) is the client's job. Building a tree from this result does not
    // require any particular order: index nodes by id, then link children to parents.
    QSqlQuery query(databaseByName(connection_name_));
    query.prepare(QStringLiteral(
        "SELECT id, IFNULL(parent_id, 0), name, comment FROM host_groups WHERE workspace_id=?"));
    query.addBindValue(workspace_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to get group list:" << query.lastError();
        return {};
    }

    QVector<Group> groups;
    while (query.next())
    {
        Group group;
        group.entry_id  = query.value(0).toLongLong();
        group.parent_id = query.value(1).toLongLong();
        group.name      = query.value(2).toString();
        group.comment   = query.value(3).toByteArray();
        groups.append(group);
    }

    return groups;
}

//--------------------------------------------------------------------------------------------------
QVector<Group> Database::groupChildren(qint64 workspace_id, qint64 parent_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    if (workspace_id <= 0 || parent_id < 0)
    {
        LOG(ERROR) << "Invalid arguments: workspace_id=" << workspace_id << "parent_id=" << parent_id;
        return {};
    }

    // Read direct children of the requested parent, alphabetically sorted. parent_id == 0 means
    // "root groups" and maps to WHERE parent_id IS NULL (SQLite treats NULL via IS, not =).
    // Otherwise filter on the parent id as a normal integer match. The SELECT shape mirrors
    // groupList() so the column to struct mapping below stays identical.
    QSqlQuery query(databaseByName(connection_name_));
    if (parent_id == 0)
    {
        query.prepare(QStringLiteral(
            "SELECT id, IFNULL(parent_id, 0), name, comment FROM host_groups "
            "WHERE workspace_id=? AND parent_id IS NULL ORDER BY name"));
        query.addBindValue(workspace_id);
    }
    else
    {
        query.prepare(QStringLiteral(
            "SELECT id, IFNULL(parent_id, 0), name, comment FROM host_groups "
            "WHERE workspace_id=? AND parent_id=? ORDER BY name"));
        query.addBindValue(workspace_id);
        query.addBindValue(parent_id);
    }

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to get group children:" << query.lastError();
        return {};
    }

    QVector<Group> groups;
    while (query.next())
    {
        Group group;
        group.entry_id  = query.value(0).toLongLong();
        group.parent_id = query.value(1).toLongLong();
        group.name      = query.value(2).toString();
        group.comment   = query.value(3).toByteArray();
        groups.append(group);
    }

    return groups;
}

//--------------------------------------------------------------------------------------------------
Group Database::findGroup(qint64 workspace_id, qint64 entry_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return Group();
    }

    if (workspace_id <= 0 || entry_id <= 0)
    {
        LOG(ERROR) << "Invalid arguments: workspace_id=" << workspace_id << "entry_id=" << entry_id;
        return Group();
    }

    // Look up a single group by its id within this workspace. Same column projection as
    // groupList(); IFNULL maps the storage NULL parent_id of a root node to 0. workspace_id is
    // part of the filter so a stale id from another workspace cannot leak through.
    QSqlQuery query(databaseByName(connection_name_));
    query.prepare(QStringLiteral(
        "SELECT id, IFNULL(parent_id, 0), name, comment FROM host_groups "
        "WHERE id=? AND workspace_id=?"));
    query.addBindValue(entry_id);
    query.addBindValue(workspace_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return Group();
    }

    if (!query.next())
        return Group();

    Group group;
    group.entry_id  = query.value(0).toLongLong();
    group.parent_id = query.value(1).toLongLong();
    group.name      = query.value(2).toString();
    group.comment   = query.value(3).toByteArray();
    return group;
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::addGroup(qint64 workspace_id, qint64 parent_id, const QString& name,
    const QByteArray& comment, qint64* entry_id)
{
    CHECK(entry_id);

    *entry_id = -1;

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (workspace_id <= 0)
    {
        LOG(ERROR) << "Invalid workspace id:" << workspace_id;
        return proto::router::kErrorInvalidData;
    }

    if (parent_id < 0)
    {
        LOG(ERROR) << "Invalid parent id:" << parent_id;
        return proto::router::kErrorInvalidData;
    }

    if (name.trimmed().isEmpty())
    {
        LOG(ERROR) << "Invalid group name";
        return proto::router::kErrorInvalidData;
    }

    QSqlDatabase sql_db = databaseByName(connection_name_);
    if (!sql_db.transaction())
    {
        LOG(ERROR) << "Unable to start transaction:" << sql_db.lastError();
        return proto::router::kErrorInternalError;
    }

    // The parent (if any) must exist in this workspace. Filtering on workspace_id here is the
    // only safeguard against the API silently linking groups across workspace boundaries.
    if (parent_id != 0)
    {
        QSqlQuery parent_check(sql_db);
        parent_check.prepare(QStringLiteral(
            "SELECT 1 FROM host_groups WHERE id=? AND workspace_id=?"));
        parent_check.addBindValue(parent_id);
        parent_check.addBindValue(workspace_id);

        if (!parent_check.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << parent_check.lastError();
            sql_db.rollback();
            return proto::router::kErrorInternalError;
        }

        if (!parent_check.next())
        {
            sql_db.rollback();
            return proto::router::kErrorInvalidData;
        }
    }

    QSqlQuery insert(sql_db);
    insert.prepare(QStringLiteral(
        "INSERT INTO host_groups (workspace_id, parent_id, name, comment) "
        "VALUES (?, ?, ?, ?)"));
    insert.addBindValue(workspace_id);
    insert.addBindValue(parent_id == 0 ? QVariant() : QVariant(parent_id));
    insert.addBindValue(name);
    insert.addBindValue(comment);

    if (!insert.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << insert.lastError();
        sql_db.rollback();
        return proto::router::kErrorInternalError;
    }

    const qint64 new_id = insert.lastInsertId().toLongLong();

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
std::string_view Database::modifyGroup(qint64 workspace_id, qint64 entry_id, qint64 new_parent_id,
    const QString& name, const QByteArray& comment)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (workspace_id <= 0 || entry_id <= 0 || new_parent_id < 0)
    {
        LOG(ERROR) << "Invalid arguments: workspace_id=" << workspace_id
                   << "entry_id=" << entry_id << "new_parent_id=" << new_parent_id;
        return proto::router::kErrorInvalidData;
    }

    if (name.trimmed().isEmpty())
    {
        LOG(ERROR) << "Invalid group name";
        return proto::router::kErrorInvalidData;
    }

    QSqlDatabase sql_db = databaseByName(connection_name_);
    if (!sql_db.transaction())
    {
        LOG(ERROR) << "Unable to start transaction:" << sql_db.lastError();
        return proto::router::kErrorInternalError;
    }

    // Verify the group being modified exists in this workspace.
    QSqlQuery select_self(sql_db);
    select_self.prepare(QStringLiteral(
        "SELECT 1 FROM host_groups WHERE id=? AND workspace_id=?"));
    select_self.addBindValue(entry_id);
    select_self.addBindValue(workspace_id);

    if (!select_self.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << select_self.lastError();
        sql_db.rollback();
        return proto::router::kErrorInternalError;
    }

    if (!select_self.next())
    {
        sql_db.rollback();
        return proto::router::kErrorNotFound;
    }

    if (new_parent_id != 0)
    {
        // The new parent must exist in this workspace.
        QSqlQuery parent_check(sql_db);
        parent_check.prepare(QStringLiteral(
            "SELECT 1 FROM host_groups WHERE id=? AND workspace_id=?"));
        parent_check.addBindValue(new_parent_id);
        parent_check.addBindValue(workspace_id);

        if (!parent_check.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << parent_check.lastError();
            sql_db.rollback();
            return proto::router::kErrorInternalError;
        }

        if (!parent_check.next())
        {
            sql_db.rollback();
            return proto::router::kErrorInvalidData;
        }

        // Cycle protection. Walk parent links from new_parent_id upward; if entry_id appears
        // anywhere on that chain, the move would put the group inside its own subtree. The
        // recursive CTE bounds itself naturally on a well-formed tree (parent_id IS NULL marks
        // the top) and SQLite caps runaway recursion at SQLITE_MAX_RECURSION_DEPTH. parent_id
        // links never cross workspace boundaries by construction (enforced at insert and
        // modify), so no workspace_id filter is needed inside the recursion.
        QSqlQuery cycle_check(sql_db);
        cycle_check.prepare(QStringLiteral(
            "WITH RECURSIVE ancestors(id) AS ("
            "    SELECT id FROM host_groups WHERE id=?"
            "    UNION ALL"
            "    SELECT g.parent_id FROM host_groups g JOIN ancestors a ON g.id = a.id "
            "      WHERE g.parent_id IS NOT NULL"
            ") SELECT 1 FROM ancestors WHERE id=? LIMIT 1"));
        cycle_check.addBindValue(new_parent_id);
        cycle_check.addBindValue(entry_id);

        if (!cycle_check.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << cycle_check.lastError();
            sql_db.rollback();
            return proto::router::kErrorInternalError;
        }

        if (cycle_check.next())
        {
            sql_db.rollback();
            return proto::router::kErrorInvalidData;
        }
    }

    QSqlQuery update_self(sql_db);
    update_self.prepare(QStringLiteral(
        "UPDATE host_groups SET parent_id=?, name=?, comment=? "
        "WHERE id=? AND workspace_id=?"));
    update_self.addBindValue(new_parent_id == 0 ? QVariant() : QVariant(new_parent_id));
    update_self.addBindValue(name);
    update_self.addBindValue(comment);
    update_self.addBindValue(entry_id);
    update_self.addBindValue(workspace_id);

    if (!update_self.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << update_self.lastError();
        sql_db.rollback();
        return proto::router::kErrorInternalError;
    }

    if (!sql_db.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << sql_db.lastError();
        sql_db.rollback();
        return proto::router::kErrorInternalError;
    }

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::removeGroup(qint64 workspace_id, qint64 entry_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (workspace_id <= 0 || entry_id <= 0)
    {
        LOG(ERROR) << "Invalid arguments: workspace_id=" << workspace_id << "entry_id=" << entry_id;
        return proto::router::kErrorInvalidData;
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare(QStringLiteral("DELETE FROM host_groups WHERE id=? AND workspace_id=?"));
    query.addBindValue(entry_id);
    query.addBindValue(workspace_id);

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
QSet<qint64> Database::workspaceAccessListForUser(qint64 user_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare(QStringLiteral("SELECT workspace_id FROM workspace_access WHERE user_id=?"));
    query.addBindValue(user_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to get workspace access list for user:" << query.lastError();
        return {};
    }

    QSet<qint64> result;
    while (query.next())
        result.insert(query.value(0).toLongLong());

    return result;
}

//--------------------------------------------------------------------------------------------------
bool Database::hasWorkspaceAccess(qint64 user_id, qint64 workspace_id) const
{
    if (!isValid() || user_id <= 0 || workspace_id <= 0)
        return false;

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare(QStringLiteral(
        "SELECT 1 FROM workspace_access WHERE workspace_id=? AND user_id=?"));
    query.addBindValue(workspace_id);
    query.addBindValue(user_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    return query.next();
}

//--------------------------------------------------------------------------------------------------
QVector<HostInfo> Database::hosts(qint64 start_item, qint64 end_item) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QString sql = QStringLiteral(
        "SELECT id, workspace_id, group_id, display_name, computer_name, "
        "cpu_arch, version, os_name, address, "
        "comment, user_name, password, last_connect, last_modify FROM hosts");

    const bool paginate = end_item > 0 && end_item >= start_item;
    if (paginate)
        sql += QStringLiteral(" LIMIT ? OFFSET ?");

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare(sql);
    if (paginate)
    {
        query.addBindValue(end_item - start_item + 1);
        query.addBindValue(start_item);
    }

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to get hosts:" << query.lastError();
        return {};
    }

    QVector<HostInfo> result;
    while (query.next())
    {
        HostInfo info;
        info.host_id       = query.value(0).toULongLong();
        info.workspace_id  = query.value(1).toLongLong();
        info.group_id      = query.value(2).toLongLong();
        info.display_name  = query.value(3).toString();
        info.computer_name = query.value(4).toString();
        info.cpu_arch      = query.value(5).toString();
        info.version       = query.value(6).toString();
        info.os_name       = query.value(7).toString();
        info.address       = query.value(8).toString();
        info.comment       = query.value(9).toByteArray();
        info.user_name     = query.value(10).toByteArray();
        info.password      = query.value(11).toByteArray();
        info.last_connect  = query.value(12).toLongLong();
        info.last_modify   = query.value(13).toLongLong();
        result.append(info);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
QVector<HostInfo> Database::hosts(
    qint64 workspace_id, qint64 group_id, qint64 start_item, qint64 end_item) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QString sql = QStringLiteral(
        "SELECT id, workspace_id, group_id, display_name, computer_name, "
        "cpu_arch, version, os_name, address, "
        "comment, user_name, password, last_connect, last_modify "
        "FROM hosts WHERE workspace_id=? AND group_id=?");

    const bool paginate = end_item > 0 && end_item >= start_item;
    if (paginate)
        sql += QStringLiteral(" LIMIT ? OFFSET ?");

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare(sql);
    query.addBindValue(workspace_id);
    query.addBindValue(group_id);
    if (paginate)
    {
        query.addBindValue(end_item - start_item + 1);
        query.addBindValue(start_item);
    }

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to get hosts:" << query.lastError();
        return {};
    }

    QVector<HostInfo> result;
    while (query.next())
    {
        HostInfo info;
        info.host_id       = query.value(0).toULongLong();
        info.workspace_id  = query.value(1).toLongLong();
        info.group_id      = query.value(2).toLongLong();
        info.display_name  = query.value(3).toString();
        info.computer_name = query.value(4).toString();
        info.cpu_arch      = query.value(5).toString();
        info.version       = query.value(6).toString();
        info.os_name       = query.value(7).toString();
        info.address       = query.value(8).toString();
        info.comment       = query.value(9).toByteArray();
        info.user_name     = query.value(10).toByteArray();
        info.password      = query.value(11).toByteArray();
        info.last_connect  = query.value(12).toLongLong();
        info.last_modify   = query.value(13).toLongLong();
        result.append(info);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
qint64 Database::hostCount() const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return 0;
    }

    QSqlQuery query(databaseByName(connection_name_));
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM hosts")))
    {
        LOG(ERROR) << "Unable to count hosts:" << query.lastError();
        return 0;
    }

    if (!query.next())
        return 0;

    return query.value(0).toLongLong();
}

//--------------------------------------------------------------------------------------------------
qint64 Database::hostCount(qint64 workspace_id, qint64 group_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return 0;
    }

    QSqlQuery query(databaseByName(connection_name_));
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM hosts WHERE workspace_id=? AND group_id=?"));
    query.addBindValue(workspace_id);
    query.addBindValue(group_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to count hosts:" << query.lastError();
        return 0;
    }

    if (!query.next())
        return 0;

    return query.value(0).toLongLong();
}

//--------------------------------------------------------------------------------------------------
// static
QString Database::databaseDirectory()
{
    return BasePaths::appDataDir();
}
