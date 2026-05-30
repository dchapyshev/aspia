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

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <utility>

#include "base/logging.h"
#include "base/crypto/generic_hash.h"
#include "base/crypto/random.h"
#include "base/files/base_paths.h"
#include "proto/router_constants.h"

namespace {

const char kConnectionName[] = "router_database";
constexpr int kClientDeviceTokenSize = 32;
constexpr qint64 kClientDeviceTokenTtlSec = 7 * 24 * 3600; // 7 days, sliding window.

//--------------------------------------------------------------------------------------------------
QString databaseDirectory()
{
    return BasePaths::appDataDir();
}

//--------------------------------------------------------------------------------------------------
QSqlDatabase connection()
{
    return QSqlDatabase::database(kConnectionName, false);
}

//--------------------------------------------------------------------------------------------------
// Reads a RouterUser row. Column order must match the SELECT projections used by userList()
// and findUser().
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
    user.otp_secret       = query.value(10).toByteArray();
    user.otp_counter      = query.value(11).toULongLong();
    return user;
}

//--------------------------------------------------------------------------------------------------
bool hasColumn(QSqlDatabase& sql_db, const QString& table, const QString& column)
{
    QSqlQuery query(sql_db);
    query.prepare("SELECT 1 FROM pragma_table_info(?) WHERE name=?");
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

    auto run = [&](const char* sql)
    {
        if (query.exec(sql))
            return true;
        LOG(ERROR) << "Unable to execute query:" << query.lastError() << "SQL:" << sql;
        sql_db.rollback();
        return false;
    };

    if (!run("CREATE TABLE IF NOT EXISTS \"users\" ("
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
             "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        return false;
    }

    // workspace_id == 0 means the host is not yet assigned to any workspace; group_id == 0
    // means the host is shown at the workspace root. No FKs on these columns because 0 is a
    // sentinel value; for any non-zero value the application enforces that it points to an
    // existing row in workspaces/host_groups.
    // comment, user_name and password are AEAD-encrypted with the workspace GK (only
    // meaningful when workspace_id != 0). name, computer_name (real OS hostname), cpu_arch,
    // version, os_name, address and last_connect are plain values; the latter five are
    // updated by the router on every host connection and reflect the latest connect attempt.
    if (!run("CREATE TABLE IF NOT EXISTS \"hosts\" ("
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
             "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        return false;
    }

    if (!run("CREATE TABLE IF NOT EXISTS \"workspaces\" ("
             "\"id\" INTEGER UNIQUE,"
             "\"name\" TEXT NOT NULL UNIQUE,"
             "\"comment\" BLOB NOT NULL DEFAULT X'',"
             "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        return false;
    }

    if (!run("CREATE TABLE IF NOT EXISTS \"workspace_access\" ("
             "\"workspace_id\" INTEGER NOT NULL,"
             "\"user_id\" INTEGER NOT NULL,"
             "\"wrapped_gk\" BLOB NOT NULL,"
             "PRIMARY KEY(\"workspace_id\", \"user_id\"),"
             "FOREIGN KEY(\"workspace_id\") REFERENCES \"workspaces\"(\"id\") ON DELETE CASCADE,"
             "FOREIGN KEY(\"user_id\") REFERENCES \"users\"(\"id\") ON DELETE CASCADE)"))
    {
        return false;
    }

    if (!run("CREATE TABLE IF NOT EXISTS \"host_groups\" ("
             "\"id\" INTEGER PRIMARY KEY AUTOINCREMENT,"
             "\"workspace_id\" INTEGER NOT NULL,"
             "\"parent_id\" INTEGER,"
             "\"name\" TEXT NOT NULL,"
             "\"comment\" BLOB NOT NULL DEFAULT X'',"
             "FOREIGN KEY(\"workspace_id\") REFERENCES \"workspaces\"(\"id\") ON DELETE CASCADE,"
             "FOREIGN KEY(\"parent_id\") REFERENCES \"host_groups\"(\"id\") ON DELETE CASCADE)"))
    {
        return false;
    }

    // Pending host removals. When an admin requests host removal we move the row from hosts
    // into this table and wait for the host to acknowledge the remove command.
    if (!run("CREATE TABLE IF NOT EXISTS \"hosts_remove\" ("
             "\"host_id\" INTEGER PRIMARY KEY,"
             "\"key\" BLOB NOT NULL UNIQUE,"
             "\"timestamp\" INTEGER NOT NULL DEFAULT 0)"))
    {
        return false;
    }

    // Composite index for the dominant host list/count query (hosts of a given workspace and
    // group). Without it list-by-group does a full table scan once hosts grows.
    if (!run("CREATE INDEX IF NOT EXISTS \"hosts_workspace_group\" "
             "ON \"hosts\"(\"workspace_id\", \"group_id\")"))
    {
        return false;
    }

    // workspace_access's PRIMARY KEY is (workspace_id, user_id), which indexes only the leading
    // column. workspaceAccessIdsForUser filters by user_id alone, so we need a dedicated index
    // on the trailing column.
    if (!run("CREATE INDEX IF NOT EXISTS \"workspace_access_user_id\" "
             "ON \"workspace_access\"(\"user_id\")"))
    {
        return false;
    }

    if (!run("CREATE INDEX IF NOT EXISTS \"host_groups_workspace_id\" "
             "ON \"host_groups\"(\"workspace_id\")"))
    {
        return false;
    }

    if (!run("CREATE INDEX IF NOT EXISTS \"host_groups_parent_id\" "
             "ON \"host_groups\"(\"parent_id\")"))
    {
        return false;
    }

    // |otp_secret| is the AEAD-encrypted shared secret (empty until self-enrollment completes;
    // non-empty implies 2FA is active for the user).
    // |otp_counter| is the highest TOTP step already consumed; the server refuses any
    // subsequent code whose step is less-than-or-equal to it (replay protection, per RFC 6238
    // section 5.2).
    static const struct { const char* name; const char* definition; } kUserColumns[] = {
        { "public_key",       "BLOB NOT NULL DEFAULT X''" },
        { "wrap_private_key", "BLOB NOT NULL DEFAULT X''" },
        { "wrap_salt",        "BLOB NOT NULL DEFAULT X''" },
        { "otp_secret",       "BLOB NOT NULL DEFAULT X''" },
        { "otp_counter",      "INTEGER NOT NULL DEFAULT 0" }
    };

    for (const auto& column : kUserColumns)
    {
        if (hasColumn(sql_db, "users", column.name))
            continue;

        if (!query.exec(QString("ALTER TABLE \"users\" ADD COLUMN \"%1\" %2")
                            .arg(QString(column.name), QString(column.definition))))
        {
            LOG(ERROR) << "Unable to add column" << column.name << ":" << query.lastError();
            sql_db.rollback();
            return false;
        }
    }

    // Bearer "remember this device" tokens issued during client sessions (admin/host sessions
    // do not use this flow). The raw token never reaches the database - only its SHA-256
    // hash is stored, so a leak of |router.db3| does not yield usable tokens. Device-binding
    // is provided by the OS keystore wrap that protects the raw token on the client side.
    // Tokens use a sliding |kClientDeviceTokenTtlSec| TTL: every successful presentation
    // refreshes |last_used_at|, and rows whose last use predates the TTL are pruned lazily
    // on the next lookup attempt. Explicit revocation (admin action, password change, user
    // removal cascade) still wipes rows out-of-band.
    if (!run("CREATE TABLE IF NOT EXISTS \"client_device_tokens\" ("
             "\"token_id\" INTEGER PRIMARY KEY AUTOINCREMENT,"
             "\"token_hash\" BLOB NOT NULL UNIQUE,"
             "\"user_id\" INTEGER NOT NULL,"
             "\"created_at\" INTEGER NOT NULL DEFAULT 0,"
             "\"last_used_at\" INTEGER NOT NULL DEFAULT 0,"
             "\"address\" TEXT NOT NULL DEFAULT '',"
             "FOREIGN KEY(\"user_id\") REFERENCES \"users\"(\"id\") ON DELETE CASCADE)"))
    {
        return false;
    }

    if (!run("CREATE INDEX IF NOT EXISTS \"client_device_tokens_user_id\" "
             "ON \"client_device_tokens\"(\"user_id\")"))
    {
        return false;
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
        if (hasColumn(sql_db, "hosts", column.name))
            continue;

        if (!query.exec(QString("ALTER TABLE \"hosts\" ADD COLUMN \"%1\" %2")
                            .arg(QString(column.name), QString(column.definition))))
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
    QString file_path = databaseDirectory();
    if (file_path.isEmpty())
        return QString();

    file_path.append("/router.db3");
    return file_path;
}

//--------------------------------------------------------------------------------------------------
bool Database::isValid() const
{
    return valid_;
}

//--------------------------------------------------------------------------------------------------
QList<RouterUser> Database::userList() const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QSqlQuery query(connection());
    if (!query.exec("SELECT id, name, \"group\", salt, verifier, sessions, flags, public_key, "
                    "wrap_private_key, wrap_salt, otp_secret, otp_counter "
                    "FROM users"))
    {
        LOG(ERROR) << "Unable to get user list:" << query.lastError();
        return {};
    }

    QList<RouterUser> users;
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

    QSqlQuery query(connection());
    query.prepare("INSERT INTO users (id, name, \"group\", salt, verifier, sessions, flags, "
        "public_key, wrap_private_key, wrap_salt) "
        "VALUES (NULL, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
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

    QSqlDatabase sql_db = connection();
    if (!sql_db.transaction())
    {
        LOG(ERROR) << "Unable to start transaction:" << sql_db.lastError();
        return false;
    }

    // Read the existing verifier first - a change to salt or verifier means a password rotation,
    // which must invalidate every device token attached to this user.
    QByteArray old_salt;
    QByteArray old_verifier;
    {
        QSqlQuery select(sql_db);
        select.prepare("SELECT salt, verifier FROM users WHERE id=?");
        select.addBindValue(user.entry_id);

        if (!select.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << select.lastError();
            sql_db.rollback();
            return false;
        }

        if (select.next())
        {
            old_salt = select.value(0).toByteArray();
            old_verifier = select.value(1).toByteArray();
        }
    }

    QSqlQuery query(sql_db);
    query.prepare("UPDATE users SET name=?, \"group\"=?, salt=?, verifier=?, sessions=?, flags=?, "
        "public_key=?, wrap_private_key=?, wrap_salt=? WHERE id=?");
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
        sql_db.rollback();
        return false;
    }

    const bool password_changed = old_salt != user.salt || old_verifier != user.verifier;
    if (password_changed)
    {
        QSqlQuery revoke(sql_db);
        revoke.prepare("DELETE FROM client_device_tokens WHERE user_id=?");
        revoke.addBindValue(user.entry_id);

        if (!revoke.exec())
        {
            LOG(ERROR) << "Unable to revoke device tokens:" << revoke.lastError();
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
bool Database::removeUser(qint64 entry_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QSqlQuery query(connection());
    query.prepare("DELETE FROM users WHERE id=?");
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

    QSqlQuery query(connection());
    query.prepare("SELECT id, name, \"group\", salt, verifier, sessions, flags, public_key, "
                  "wrap_private_key, wrap_salt, otp_secret, otp_counter "
                  "FROM users WHERE name=?");
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

    QSqlQuery query(connection());
    query.prepare("SELECT id, name, \"group\", salt, verifier, sessions, flags, public_key, "
                  "wrap_private_key, wrap_salt, otp_secret, otp_counter "
                  "FROM users WHERE id=?");
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
bool Database::setUserOtp(qint64 user_id, const QByteArray& encrypted_secret, quint64 counter)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QSqlQuery query(connection());
    query.prepare("UPDATE users SET otp_secret=?, otp_counter=? WHERE id=?");
    query.addBindValue(encrypted_secret);
    query.addBindValue(counter);
    query.addBindValue(user_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to set user OTP:" << query.lastError();
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::clearUserOtp(qint64 user_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QSqlQuery query(connection());
    query.prepare("UPDATE users SET otp_secret=X'', otp_counter=0 WHERE id=?");
    query.addBindValue(user_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to clear user OTP:" << query.lastError();
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::updateUserOtpCounter(qint64 user_id, quint64 counter)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QSqlQuery query(connection());
    query.prepare("UPDATE users SET otp_counter=? WHERE id=?");
    query.addBindValue(counter);
    query.addBindValue(user_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to update OTP counter:" << query.lastError();
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::issueClientDeviceToken(qint64 user_id, const QString& address, QByteArray* token_id)
{
    CHECK(token_id);
    *token_id = QByteArray();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    // Generate the raw token, hand it back to the caller (it will leave the router to the
    // client) and persist only its SHA-256 hash. Hashing without a salt is safe here because
    // the input is uniformly random CSPRNG output - precomputation/rainbow tables make no
    // sense against 2^256 of entropy.
    const QByteArray new_token_id = Random::byteArray(kClientDeviceTokenSize);
    const QByteArray token_hash = GenericHash::hash(GenericHash::SHA256, new_token_id);
    const qint64 now = QDateTime::currentSecsSinceEpoch();

    QSqlQuery query(connection());
    query.prepare("INSERT INTO client_device_tokens "
                  "(token_hash, user_id, created_at, last_used_at, address) "
                  "VALUES (?, ?, ?, ?, ?)");
    query.addBindValue(token_hash);
    query.addBindValue(user_id);
    query.addBindValue(now);
    query.addBindValue(now);
    query.addBindValue(address);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to issue client device token:" << query.lastError();
        return false;
    }

    *token_id = new_token_id;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::findClientDeviceToken(const QByteArray& token_id, qint64* user_id) const
{
    CHECK(user_id);
    *user_id = 0;

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (token_id.size() != kClientDeviceTokenSize)
        return false;

    const QByteArray token_hash = GenericHash::hash(GenericHash::SHA256, token_id);

    QSqlQuery query(connection());
    query.prepare("SELECT user_id, last_used_at FROM client_device_tokens WHERE token_hash=?");
    query.addBindValue(token_hash);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to look up client device token:" << query.lastError();
        return false;
    }

    if (!query.next())
        return false;

    const qint64 last_used_at = query.value(1).toLongLong();
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (now - last_used_at > kClientDeviceTokenTtlSec)
    {
        // Lazy GC: drop the row so the table does not accumulate stale entries. The caller
        // will treat the result as INVALID_TOKEN and walk the user back through TOTP.
        QSqlQuery prune(connection());
        prune.prepare("DELETE FROM client_device_tokens WHERE token_hash=?");
        prune.addBindValue(token_hash);
        if (!prune.exec())
            LOG(WARNING) << "Unable to prune expired client device token:" << prune.lastError();
        return false;
    }

    *user_id = query.value(0).toLongLong();
    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::touchClientDeviceToken(const QByteArray& token_id, const QString& address)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    const QByteArray token_hash = GenericHash::hash(GenericHash::SHA256, token_id);

    QSqlQuery query(connection());
    query.prepare("UPDATE client_device_tokens SET last_used_at=?, address=? WHERE token_hash=?");
    query.addBindValue(QDateTime::currentSecsSinceEpoch());
    query.addBindValue(address);
    query.addBindValue(token_hash);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to touch client device token:" << query.lastError();
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::revokeClientDeviceToken(qint64 user_id, qint64 token_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QSqlQuery query(connection());
    query.prepare("DELETE FROM client_device_tokens WHERE token_id=? AND user_id=?");
    query.addBindValue(token_id);
    query.addBindValue(user_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to revoke client device token:" << query.lastError();
        return false;
    }
    return query.numRowsAffected() > 0;
}

//--------------------------------------------------------------------------------------------------
bool Database::revokeUserClientDeviceTokens(qint64 user_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    QSqlQuery query(connection());
    query.prepare("DELETE FROM client_device_tokens WHERE user_id=?");
    query.addBindValue(user_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to revoke client device tokens:" << query.lastError();
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
QList<DeviceToken> Database::listClientDeviceTokens(qint64 user_id) const
{
    QList<DeviceToken> tokens;

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return tokens;
    }

    QSqlQuery query(connection());
    query.prepare("SELECT token_id, created_at, last_used_at, address FROM client_device_tokens "
                  "WHERE user_id=? ORDER BY created_at");
    query.addBindValue(user_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to list client device tokens:" << query.lastError();
        return tokens;
    }

    while (query.next())
    {
        DeviceToken token;
        token.token_id     = query.value(0).toLongLong();
        token.created_at   = query.value(1).toLongLong();
        token.last_used_at = query.value(2).toLongLong();
        token.address      = query.value(3).toString();
        tokens.append(token);
    }
    return tokens;
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

    QSqlQuery query(connection());
    query.prepare("SELECT id FROM hosts WHERE key=?");
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
    QSqlQuery pending(connection());
    pending.prepare("SELECT host_id FROM hosts_remove WHERE key=?");
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

    QSqlQuery query(connection());
    query.prepare("INSERT INTO hosts (id, key) VALUES (NULL, ?)");
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
    QSqlQuery query(connection());
    query.prepare("UPDATE hosts SET "
        "display_name = CASE WHEN display_name='' THEN ? ELSE display_name END, "
        "computer_name=?, cpu_arch=?, version=?, os_name=?, address=?, last_connect=? "
        "WHERE id=?");
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

    QSqlQuery query(connection());
    query.prepare("SELECT workspace_id FROM hosts WHERE id=?");
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
bool Database::modifyHost(HostId host_id, qint64 group_id, const QString& display_name,
    const QByteArray& comment, const QByteArray& user_name, const QByteArray& password)
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

    QSqlQuery query(connection());
    query.prepare("UPDATE hosts SET display_name=?, group_id=?, comment=?, user_name=?, password=?, "
        "last_modify=? WHERE id=?");
    query.addBindValue(display_name);
    query.addBindValue(group_id);
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
QList<HostInfo> Database::hosts(qint64 start_item, qint64 end_item) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QString sql = "SELECT id, workspace_id, group_id, display_name, computer_name, cpu_arch, "
                  "version, os_name, address, comment, user_name, password, last_connect, last_modify "
                  "FROM hosts";

    const bool paginate = end_item > 0 && end_item >= start_item;
    if (paginate)
        sql += " LIMIT ? OFFSET ?";

    QSqlQuery query(connection());
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

    QList<HostInfo> result;
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
QList<HostInfo> Database::hosts(
    qint64 workspace_id, qint64 group_id, qint64 start_item, qint64 end_item) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QString sql = "SELECT id, workspace_id, group_id, display_name, computer_name, cpu_arch, "
                  "version, os_name, address, comment, user_name, password, last_connect, last_modify "
                  "FROM hosts WHERE workspace_id=? AND group_id=?";

    const bool paginate = end_item > 0 && end_item >= start_item;
    if (paginate)
        sql += " LIMIT ? OFFSET ?";

    QSqlQuery query(connection());
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

    QList<HostInfo> result;
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

    QSqlQuery query(connection());
    if (!query.exec("SELECT COUNT(*) FROM hosts"))
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

    QSqlQuery query(connection());
    query.prepare("SELECT COUNT(*) FROM hosts WHERE workspace_id=? AND group_id=?");
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

    QSqlDatabase sql_db = connection();
    if (!sql_db.transaction())
    {
        LOG(ERROR) << "Unable to start transaction:" << sql_db.lastError();
        return false;
    }

    QSqlQuery select(sql_db);
    select.prepare("SELECT key FROM hosts WHERE id=?");
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
    insert.prepare("INSERT INTO hosts_remove (host_id, key, timestamp) VALUES (?, ?, ?)");
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
    del.prepare("DELETE FROM hosts WHERE id=?");
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

    QSqlQuery query(connection());
    query.prepare("SELECT 1 FROM hosts_remove WHERE host_id=?");
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

    QSqlQuery query(connection());
    query.prepare("DELETE FROM hosts_remove WHERE host_id=?");
    query.addBindValue(host_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << query.lastError();
        return false;
    }

    return query.numRowsAffected() > 0;
}

//--------------------------------------------------------------------------------------------------
QList<Workspace> Database::workspaceList() const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QSqlQuery query(connection());
    if (!query.exec("SELECT id, name, comment FROM workspaces"))
    {
        LOG(ERROR) << "Unable to get workspace list:" << query.lastError();
        return {};
    }

    QList<Workspace> workspaces;
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

    QSqlQuery query(connection());
    query.prepare("SELECT id, name, comment FROM workspaces WHERE id=?");
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
    const QList<Workspace::Access>& initial_access, qint64* entry_id)
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

    QSqlDatabase sql_db = connection();
    if (!sql_db.transaction())
    {
        LOG(ERROR) << "Unable to start transaction:" << sql_db.lastError();
        return proto::router::kErrorInternalError;
    }

    QSqlQuery check(sql_db);
    check.prepare("SELECT 1 FROM workspaces WHERE name=?");
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
    insert_workspace.prepare("INSERT INTO workspaces (id, name, comment) VALUES (NULL, ?, ?)");
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
    insert_access.prepare("INSERT INTO workspace_access (workspace_id, user_id, wrapped_gk) VALUES (?, ?, ?)");

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
    const QList<Workspace::Access>& desired_access)
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

    QSqlDatabase sql_db = connection();
    if (!sql_db.transaction())
    {
        LOG(ERROR) << "Unable to start transaction:" << sql_db.lastError();
        return proto::router::kErrorInternalError;
    }

    QSqlQuery exists_check(sql_db);
    exists_check.prepare("SELECT 1 FROM workspaces WHERE id=?");
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
    name_check.prepare("SELECT id FROM workspaces WHERE name=?");
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
    update_workspace.prepare("UPDATE workspaces SET name=?, comment=? WHERE id=?");
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
    select_current.prepare("SELECT user_id FROM workspace_access WHERE workspace_id=?");
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
    delete_access.prepare("DELETE FROM workspace_access WHERE workspace_id=? AND user_id=?");

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
    insert_access.prepare("INSERT INTO workspace_access (workspace_id, user_id, wrapped_gk) VALUES (?, ?, ?)");

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

    QSqlQuery query(connection());
    query.prepare("DELETE FROM workspaces WHERE id=?");
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

    QSqlDatabase sql_db = connection();
    if (!sql_db.transaction())
    {
        LOG(ERROR) << "Unable to start transaction:" << sql_db.lastError();
        return proto::router::kErrorInternalError;
    }

    // Release: hosts currently in this workspace but no longer wanted.
    QSqlQuery select_current(sql_db);
    select_current.prepare("SELECT id FROM hosts WHERE workspace_id=?");
    select_current.addBindValue(entry_id);
    if (!select_current.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << select_current.lastError();
        sql_db.rollback();
        return proto::router::kErrorInternalError;
    }

    QSqlQuery release(sql_db);
    release.prepare("UPDATE hosts SET workspace_id=0, group_id=0 WHERE id=?");
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
    claim.prepare("UPDATE hosts SET workspace_id=? WHERE id=? AND workspace_id IN (0, ?)");
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
QList<Workspace::Access> Database::workspaceAccessList(qint64 workspace_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QSqlQuery query(connection());
    query.prepare("SELECT workspace_id, user_id, wrapped_gk FROM workspace_access WHERE workspace_id=?");
    query.addBindValue(workspace_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to get workspace access list:" << query.lastError();
        return {};
    }

    QList<Workspace::Access> result;
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
QSet<qint64> Database::workspaceAccessIdsForUser(qint64 user_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QSqlQuery query(connection());
    query.prepare("SELECT workspace_id FROM workspace_access WHERE user_id=?");
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
QList<Workspace::Access> Database::workspaceAccessListForUser(qint64 user_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    QSqlQuery query(connection());
    query.prepare("SELECT workspace_id, wrapped_gk FROM workspace_access WHERE user_id=?");
    query.addBindValue(user_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to get workspace keys for user:" << query.lastError();
        return {};
    }

    QList<Workspace::Access> result;
    while (query.next())
    {
        Workspace::Access access;
        access.workspace_id = query.value(0).toLongLong();
        access.user_id      = user_id;
        access.wrapped_gk   = query.value(1).toByteArray();
        result.append(access);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
bool Database::hasWorkspaceAccess(qint64 user_id, qint64 workspace_id) const
{
    if (!isValid() || user_id <= 0 || workspace_id <= 0)
        return false;

    QSqlQuery query(connection());
    query.prepare("SELECT 1 FROM workspace_access WHERE workspace_id=? AND user_id=?");
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
QList<Group> Database::groupList(qint64 workspace_id) const
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
    QSqlQuery query(connection());
    query.prepare("SELECT id, IFNULL(parent_id, 0), name, comment FROM host_groups WHERE workspace_id=?");
    query.addBindValue(workspace_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to get group list:" << query.lastError();
        return {};
    }

    QList<Group> groups;
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
QList<Group> Database::groupChildren(qint64 workspace_id, qint64 parent_id) const
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
    QSqlQuery query(connection());
    if (parent_id == 0)
    {
        query.prepare("SELECT id, IFNULL(parent_id, 0), name, comment FROM host_groups "
                      "WHERE workspace_id=? AND parent_id IS NULL ORDER BY name");
        query.addBindValue(workspace_id);
    }
    else
    {
        query.prepare("SELECT id, IFNULL(parent_id, 0), name, comment FROM host_groups "
                      "WHERE workspace_id=? AND parent_id=? ORDER BY name");
        query.addBindValue(workspace_id);
        query.addBindValue(parent_id);
    }

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to get group children:" << query.lastError();
        return {};
    }

    QList<Group> groups;
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
    QSqlQuery query(connection());
    query.prepare("SELECT id, IFNULL(parent_id, 0), name, comment FROM host_groups "
                  "WHERE id=? AND workspace_id=?");
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

    QSqlDatabase sql_db = connection();
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
        parent_check.prepare("SELECT 1 FROM host_groups WHERE id=? AND workspace_id=?");
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
    insert.prepare("INSERT INTO host_groups (workspace_id, parent_id, name, comment) "
                   "VALUES (?, ?, ?, ?)");
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

    QSqlDatabase sql_db = connection();
    if (!sql_db.transaction())
    {
        LOG(ERROR) << "Unable to start transaction:" << sql_db.lastError();
        return proto::router::kErrorInternalError;
    }

    // Verify the group being modified exists in this workspace.
    QSqlQuery select_self(sql_db);
    select_self.prepare("SELECT 1 FROM host_groups WHERE id=? AND workspace_id=?");
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
        parent_check.prepare("SELECT 1 FROM host_groups WHERE id=? AND workspace_id=?");
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
        cycle_check.prepare("WITH RECURSIVE ancestors(id) AS ("
            "    SELECT id FROM host_groups WHERE id=?"
            "    UNION ALL"
            "    SELECT g.parent_id FROM host_groups g JOIN ancestors a ON g.id = a.id "
            "      WHERE g.parent_id IS NOT NULL"
            ") SELECT 1 FROM ancestors WHERE id=? LIMIT 1");
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
    update_self.prepare("UPDATE host_groups SET parent_id=?, name=?, comment=? "
        "WHERE id=? AND workspace_id=?");
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

    QSqlQuery query(connection());
    query.prepare("DELETE FROM host_groups WHERE id=? AND workspace_id=?");
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
bool Database::openDatabase()
{
    QString dir_path = databaseDirectory();
    if (dir_path.isEmpty())
    {
        LOG(ERROR) << "Invalid directory path";
        return false;
    }

    // Ensure the directory exists.
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

    QSqlDatabase db = connection();
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

    // Foreign keys are off by default in SQLite, enable per-connection so the cascade rules
    // declared on the schema actually fire.
    if (!pragma.exec("PRAGMA foreign_keys = ON"))
        LOG(WARNING) << "Unable to enable foreign keys:" << pragma.lastError();

    // Write-Ahead Logging: concurrent readers do not block the writer, the writer does not
    // rewrite a rollback journal on every commit. synchronous stays at default FULL so durable
    // commits survive power loss.
    if (!pragma.exec("PRAGMA journal_mode = WAL"))
        LOG(WARNING) << "Unable to enable WAL journal mode:" << pragma.lastError();

    // Keep temporary B-trees (recursive CTEs, sorts without a covering index, GROUP BY) in
    // memory rather than on disk.
    if (!pragma.exec("PRAGMA temp_store = MEMORY"))
        LOG(WARNING) << "Unable to set temp_store:" << pragma.lastError();

    // Larger page cache (8 MiB instead of the default 2 MiB). Negative value means size in
    // kibibytes rather than page count. Keeps hot index pages and recently accessed rows
    // resident as the database grows.
    if (!pragma.exec("PRAGMA cache_size = -8000"))
        LOG(WARNING) << "Unable to set cache_size:" << pragma.lastError();

    if (!ensureSchema(db))
    {
        db.close();
        QSqlDatabase::removeDatabase(kConnectionName);
        return false;
    }

    return true;
}

