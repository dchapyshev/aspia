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

#include <set>
#include <string>
#include <unordered_map>
#include <utility>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/crypto/generic_hash.h"
#include "base/crypto/random.h"
#include "base/files/base_paths.h"
#include "base/sql/sql_query.h"
#include "base/sql/sql_transaction.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"

namespace {

constexpr int kClientDeviceTokenSize = 32;
constexpr qint64 kClientDeviceTokenTtlSec = 7 * 24 * 3600; // 7 days, sliding window.

//--------------------------------------------------------------------------------------------------
QString databaseDirectory()
{
    return BasePaths::appDataDir();
}

//--------------------------------------------------------------------------------------------------
// Reads a RouterUser row. Column order must match the SELECT projections used by userList()
// and findUser().
RouterUser readUser(const SqlQuery& query)
{
    RouterUser user;
    user.entry_id         = query.columnInt64(0);
    user.name             = query.columnText(1);
    user.group            = query.columnText(2);
    user.salt             = query.columnBlob(3);
    user.verifier         = query.columnBlob(4);
    user.sessions         = static_cast<quint32>(query.columnInt64(5));
    user.flags            = static_cast<quint32>(query.columnInt64(6));
    user.public_key       = query.columnBlob(7);
    user.wrap_private_key = query.columnBlob(8);
    user.wrap_salt        = query.columnBlob(9);
    user.otp_secret       = query.columnBlob(10);
    user.otp_counter      = query.columnUInt64(11);
    return user;
}

//--------------------------------------------------------------------------------------------------
bool hasColumn(SqlDatabase& db, const QString& table, const QString& column)
{
    SqlQuery query(db, "SELECT 1 FROM pragma_table_info(?) WHERE name=?");
    query.addText(table);
    query.addText(column);

    if (!query.isValid())
    {
        LOG(ERROR) << "Unable to query table info:" << db.lastError();
        return true; // Pessimistic: pretend it exists, do not attempt ALTER.
    }

    return query.next();
}

//--------------------------------------------------------------------------------------------------
bool ensureSchema(SqlDatabase& db)
{
    SqlTransaction transaction(db);
    if (!transaction.begin())
    {
        LOG(ERROR) << "Unable to start transaction:" << db.lastError();
        return false;
    }

    auto run = [&](const char* sql)
    {
        if (db.exec(sql))
            return true;
        LOG(ERROR) << "Unable to execute query:" << db.lastError() << "SQL:" << sql;
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
        if (hasColumn(db, "users", column.name))
            continue;

        const std::string sql = strCat({"ALTER TABLE \"users\" ADD COLUMN \"",
                                         column.name, "\" ", column.definition});
        if (!db.exec(sql.c_str()))
        {
            LOG(ERROR) << "Unable to add column" << column.name << ":" << db.lastError();
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
        if (hasColumn(db, "hosts", column.name))
            continue;

        const std::string sql = strCat({"ALTER TABLE \"hosts\" ADD COLUMN \"",
                                         column.name, "\" ", column.definition});
        if (!db.exec(sql.c_str()))
        {
            LOG(ERROR) << "Unable to add column" << column.name << ":" << db.lastError();
            return false;
        }
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << db.lastError();
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

    const char kSql[] =
        "SELECT id, name, \"group\", salt, verifier, sessions, flags, public_key, "
        "wrap_private_key, wrap_salt, otp_secret, otp_counter FROM users";
    SqlQuery query(db_, kSql);

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

    const char kSql[] =
        "INSERT INTO users (id, name, \"group\", salt, verifier, sessions, flags, public_key, "
        "wrap_private_key, wrap_salt) VALUES (NULL, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
    SqlQuery query(db_, kSql);
    query.addText(user.name);
    query.addText(user.group);
    query.addBlob(user.salt);
    query.addBlob(user.verifier);
    query.addInt64(user.sessions);
    query.addInt64(user.flags);
    query.addBlob(user.public_key);
    query.addBlob(user.wrap_private_key);
    query.addBlob(user.wrap_salt);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
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

    SqlTransaction transaction(db_);
    if (!transaction.begin())
    {
        LOG(ERROR) << "Unable to start transaction:" << db_.lastError();
        return false;
    }

    // Read the existing verifier first - a change to salt or verifier means a password rotation,
    // which must invalidate every device token attached to this user.
    QByteArray old_salt;
    QByteArray old_verifier;
    {
        SqlQuery select(db_, "SELECT salt, verifier FROM users WHERE id=?");
        select.addInt64(user.entry_id);

        if (!select.isValid())
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return false;
        }

        if (select.next())
        {
            old_salt = select.columnBlob(0);
            old_verifier = select.columnBlob(1);
        }
    }

    const char kSql[] =
        "UPDATE users SET name=?, \"group\"=?, salt=?, verifier=?, sessions=?, flags=?, "
        "public_key=?, wrap_private_key=?, wrap_salt=? WHERE id=?";
    SqlQuery query(db_, kSql);
    query.addText(user.name);
    query.addText(user.group);
    query.addBlob(user.salt);
    query.addBlob(user.verifier);
    query.addInt64(user.sessions);
    query.addInt64(user.flags);
    query.addBlob(user.public_key);
    query.addBlob(user.wrap_private_key);
    query.addBlob(user.wrap_salt);
    query.addInt64(user.entry_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    const bool password_changed = old_salt != user.salt || old_verifier != user.verifier;
    if (password_changed)
    {
        SqlQuery revoke(db_, "DELETE FROM client_device_tokens WHERE user_id=?");
        revoke.addInt64(user.entry_id);

        if (!revoke.exec())
        {
            LOG(ERROR) << "Unable to revoke device tokens:" << db_.lastError();
            return false;
        }
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << db_.lastError();
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

    SqlQuery query(db_, "DELETE FROM users WHERE id=?");
    query.addInt64(entry_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
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

    const char kSql[] =
        "SELECT id, name, \"group\", salt, verifier, sessions, flags, public_key, "
        "wrap_private_key, wrap_salt, otp_secret, otp_counter FROM users WHERE name=?";
    SqlQuery query(db_, kSql);
    query.addText(username);

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

    const char kSql[] =
        "SELECT id, name, \"group\", salt, verifier, sessions, flags, public_key, "
        "wrap_private_key, wrap_salt, otp_secret, otp_counter FROM users WHERE id=?";
    SqlQuery query(db_, kSql);
    query.addInt64(entry_id);

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

    SqlQuery query(db_, "UPDATE users SET otp_secret=?, otp_counter=? WHERE id=?");
    query.addBlob(encrypted_secret);
    query.addUInt64(counter);
    query.addInt64(user_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to set user OTP:" << db_.lastError();
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

    SqlQuery query(db_, "UPDATE users SET otp_secret=X'', otp_counter=0 WHERE id=?");
    query.addInt64(user_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to clear user OTP:" << db_.lastError();
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

    SqlQuery query(db_, "UPDATE users SET otp_counter=? WHERE id=?");
    query.addUInt64(counter);
    query.addInt64(user_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to update OTP counter:" << db_.lastError();
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::issueClientDeviceToken(qint64 user_id, const QString& address, QByteArray* token, qint64* token_id)
{
    CHECK(token);
    *token = QByteArray();
    if (token_id)
        *token_id = 0;

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    // Generate the raw token, hand it back to the caller (it will leave the router to the
    // client) and persist only its SHA-256 hash. Hashing without a salt is safe here because
    // the input is uniformly random CSPRNG output - precomputation/rainbow tables make no
    // sense against 2^256 of entropy.
    const QByteArray new_token = Random::byteArray(kClientDeviceTokenSize);
    const QByteArray token_hash = GenericHash::hash(GenericHash::SHA256, new_token);
    const qint64 now = QDateTime::currentSecsSinceEpoch();

    const char kSql[] =
        "INSERT INTO client_device_tokens "
        "(token_hash, user_id, created_at, last_used_at, address) VALUES (?, ?, ?, ?, ?)";
    SqlQuery query(db_, kSql);
    query.addBlob(token_hash);
    query.addInt64(user_id);
    query.addInt64(now);
    query.addInt64(now);
    query.addText(address);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to issue client device token:" << db_.lastError();
        return false;
    }

    if (token_id)
        *token_id = db_.lastInsertRowId();

    *token = new_token;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::findClientDeviceToken(const QByteArray& token, qint64* user_id, qint64* token_id) const
{
    CHECK(user_id);
    *user_id = 0;
    if (token_id)
        *token_id = 0;

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (token.size() != kClientDeviceTokenSize)
        return false;

    const QByteArray token_hash = GenericHash::hash(GenericHash::SHA256, token);

    const char kSql[] =
        "SELECT user_id, last_used_at, token_id FROM client_device_tokens WHERE token_hash=?";
    SqlQuery query(db_, kSql);
    query.addBlob(token_hash);

    if (!query.next())
        return false;

    const qint64 last_used_at = query.columnInt64(1);
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (now - last_used_at > kClientDeviceTokenTtlSec)
    {
        // Lazy GC: drop the row so the table does not accumulate stale entries. The caller
        // will treat the result as INVALID_TOKEN and walk the user back through TOTP.
        SqlQuery prune(db_, "DELETE FROM client_device_tokens WHERE token_hash=?");
        prune.addBlob(token_hash);
        if (!prune.exec())
            LOG(WARNING) << "Unable to prune expired client device token:" << db_.lastError();
        return false;
    }

    *user_id = query.columnInt64(0);
    if (token_id)
        *token_id = query.columnInt64(2);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::touchClientDeviceToken(const QByteArray& token, const QString& address)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    const QByteArray token_hash = GenericHash::hash(GenericHash::SHA256, token);

    const char kSql[] =
        "UPDATE client_device_tokens SET last_used_at=?, address=? WHERE token_hash=?";
    SqlQuery query(db_, kSql);
    query.addInt64(QDateTime::currentSecsSinceEpoch());
    query.addText(address);
    query.addBlob(token_hash);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to touch client device token:" << db_.lastError();
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

    SqlQuery query(db_, "DELETE FROM client_device_tokens WHERE token_id=? AND user_id=?");
    query.addInt64(token_id);
    query.addInt64(user_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to revoke client device token:" << db_.lastError();
        return false;
    }
    return db_.changes() > 0;
}

//--------------------------------------------------------------------------------------------------
bool Database::revokeUserClientDeviceTokens(qint64 user_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "DELETE FROM client_device_tokens WHERE user_id=?");
    query.addInt64(user_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to revoke client device tokens:" << db_.lastError();
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

    const char kSql[] =
        "SELECT token_id, created_at, last_used_at, address "
        "FROM client_device_tokens WHERE user_id=? ORDER BY created_at";
    SqlQuery query(db_, kSql);
    query.addInt64(user_id);

    while (query.next())
    {
        DeviceToken token;
        token.token_id     = query.columnInt64(0);
        token.created_at   = query.columnInt64(1);
        token.last_used_at = query.columnInt64(2);
        token.address      = query.columnText(3);
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

    SqlQuery query(db_, "SELECT id FROM hosts WHERE key=?");
    query.addBlob(key_hash);

    if (!query.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    if (query.next())
    {
        *host_id = query.columnUInt64(0);
        return proto::router::kErrorOk;
    }

    // Not found in hosts - check hosts_remove. A reconnecting host whose removal was scheduled
    // while it was offline will be matched here; the caller is expected to send the remove
    // command using the returned host_id and wait for the ack before finalizing the deletion.
    SqlQuery pending(db_, "SELECT host_id FROM hosts_remove WHERE key=?");
    pending.addBlob(key_hash);

    if (!pending.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    if (!pending.next())
        return proto::router::kErrorNotFound;

    *host_id = pending.columnUInt64(0);
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

    SqlQuery query(db_, "INSERT INTO hosts (id, key) VALUES (NULL, ?)");
    query.addBlob(key_hash);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
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
    const char kSql[] =
        "UPDATE hosts SET "
        "display_name = CASE WHEN display_name='' THEN ? ELSE display_name END, "
        "computer_name=?, cpu_arch=?, version=?, os_name=?, address=?, last_connect=? WHERE id=?";
    SqlQuery query(db_, kSql);
    query.addText(computer_name);
    query.addText(computer_name);
    query.addText(cpu_arch);
    query.addText(version);
    query.addText(os_name);
    query.addText(address);
    query.addInt64(timestamp);
    query.addUInt64(host_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
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

    SqlQuery query(db_, "SELECT workspace_id FROM hosts WHERE id=?");
    query.addUInt64(host_id);

    if (!query.next())
        return -1;
    return query.columnInt64(0);
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

    const char kSql[] =
        "UPDATE hosts SET display_name=?, group_id=?, comment=?, user_name=?, password=?, "
        "last_modify=? WHERE id=?";
    SqlQuery query(db_, kSql);
    query.addText(display_name);
    query.addInt64(group_id);
    query.addBlob(comment);
    query.addBlob(user_name);
    query.addBlob(password);
    query.addInt64(timestamp);
    query.addUInt64(host_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return db_.changes() > 0;
}

//--------------------------------------------------------------------------------------------------
void Database::hosts(qint64 start_item, qint64 end_item, proto::router::HostList* out) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    const bool paginate = end_item > 0 && end_item >= start_item;
    const std::string sql = strCat({
        "SELECT id, workspace_id, group_id, display_name, computer_name, cpu_arch, version, "
        "os_name, address, comment, user_name, password, last_connect, last_modify FROM hosts",
        paginate ? " LIMIT ? OFFSET ?" : ""});

    SqlQuery query(db_, sql);
    if (!query.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    if (paginate)
    {
        query.addInt64(end_item - start_item + 1);
        query.addInt64(start_item);
    }

    while (query.next())
    {
        proto::router::Host* host = out->add_host();
        host->set_host_id(query.columnUInt64(0));
        host->set_workspace_id(query.columnInt64(1));
        host->set_group_id(query.columnInt64(2));
        host->set_display_name(query.columnTextView(3));
        host->set_computer_name(query.columnTextView(4));
        host->set_cpu_arch(query.columnTextView(5));
        host->set_version(query.columnTextView(6));
        host->set_os_name(query.columnTextView(7));
        host->set_address(query.columnTextView(8));
        host->set_comment(query.columnBlobView(9));
        host->set_user_name(query.columnBlobView(10));
        host->set_password(query.columnBlobView(11));
        host->set_last_connect(query.columnInt64(12));
        host->set_last_modify(query.columnInt64(13));
    }

    out->set_error_code(proto::router::kErrorOk);
}

//--------------------------------------------------------------------------------------------------
void Database::hosts(qint64 workspace_id, qint64 group_id, qint64 start_item,
    qint64 end_item, proto::router::HostList* out) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    const bool paginate = end_item > 0 && end_item >= start_item;
    const std::string sql = strCat({
        "SELECT id, workspace_id, group_id, display_name, computer_name, cpu_arch, version, "
        "os_name, address, comment, user_name, password, last_connect, last_modify "
        "FROM hosts WHERE workspace_id=? AND group_id=?",
        paginate ? " LIMIT ? OFFSET ?" : ""});

    SqlQuery query(db_, sql);
    if (!query.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    query.addInt64(workspace_id);
    query.addInt64(group_id);
    if (paginate)
    {
        query.addInt64(end_item - start_item + 1);
        query.addInt64(start_item);
    }

    while (query.next())
    {
        proto::router::Host* host = out->add_host();
        host->set_host_id(query.columnUInt64(0));
        host->set_workspace_id(query.columnInt64(1));
        host->set_group_id(query.columnInt64(2));
        host->set_display_name(query.columnTextView(3));
        host->set_computer_name(query.columnTextView(4));
        host->set_cpu_arch(query.columnTextView(5));
        host->set_version(query.columnTextView(6));
        host->set_os_name(query.columnTextView(7));
        host->set_address(query.columnTextView(8));
        host->set_comment(query.columnBlobView(9));
        host->set_user_name(query.columnBlobView(10));
        host->set_password(query.columnBlobView(11));
        host->set_last_connect(query.columnInt64(12));
        host->set_last_modify(query.columnInt64(13));
    }

    out->set_error_code(proto::router::kErrorOk);
}

//--------------------------------------------------------------------------------------------------
qint64 Database::hostCount() const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return 0;
    }

    SqlQuery query(db_, "SELECT COUNT(*) FROM hosts");
    if (!query.next())
        return 0;

    return query.columnInt64(0);
}

//--------------------------------------------------------------------------------------------------
qint64 Database::hostCount(qint64 workspace_id, qint64 group_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return 0;
    }

    SqlQuery query(db_, "SELECT COUNT(*) FROM hosts WHERE workspace_id=? AND group_id=?");
    query.addInt64(workspace_id);
    query.addInt64(group_id);

    if (!query.next())
        return 0;

    return query.columnInt64(0);
}

//--------------------------------------------------------------------------------------------------
void Database::searchHosts(const QString& query_text,
    const std::set<qint64>& workspace_ids, proto::router::HostSearchResult* out) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    if (workspace_ids.empty() || query_text.isEmpty())
    {
        out->set_error_code(proto::router::kErrorOk);
        return;
    }

    // Escape LIKE wildcards so user input is matched literally.
    QString escaped = query_text;
    escaped.replace('\\', "\\\\");
    escaped.replace('%', "\\%");
    escaped.replace('_', "\\_");

    const QString pattern = '%' + escaped + '%';

    QStringList placeholders;
    placeholders.reserve(static_cast<qsizetype>(workspace_ids.size()));

    for (size_t i = 0; i < workspace_ids.size(); ++i)
        placeholders.append("?");

    const QString sql =
        "SELECT id, workspace_id, group_id, display_name, computer_name, cpu_arch, "
        "version, os_name, address, comment, user_name, password, last_connect, last_modify "
        "FROM hosts WHERE workspace_id IN (" + placeholders.join(',') + ") "
        "AND (casefold(display_name) LIKE casefold(?) ESCAPE '\\' "
        "OR CAST(id AS TEXT) LIKE ? ESCAPE '\\') "
        "ORDER BY display_name";

    SqlQuery query(db_, sql.toStdString());
    if (!query.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    for (qint64 workspace_id : workspace_ids)
        query.addInt64(workspace_id);
    query.addText(pattern);
    query.addText(pattern);

    while (query.next())
    {
        proto::router::Host* host = out->add_host();
        host->set_host_id(query.columnUInt64(0));
        host->set_workspace_id(query.columnInt64(1));
        host->set_group_id(query.columnInt64(2));
        host->set_display_name(query.columnTextView(3));
        host->set_computer_name(query.columnTextView(4));
        host->set_cpu_arch(query.columnTextView(5));
        host->set_version(query.columnTextView(6));
        host->set_os_name(query.columnTextView(7));
        host->set_address(query.columnTextView(8));
        host->set_comment(query.columnBlobView(9));
        host->set_user_name(query.columnBlobView(10));
        host->set_password(query.columnBlobView(11));
        host->set_last_connect(query.columnInt64(12));
        host->set_last_modify(query.columnInt64(13));
    }

    out->set_error_code(proto::router::kErrorOk);
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

    SqlTransaction transaction(db_);
    if (!transaction.begin())
    {
        LOG(ERROR) << "Unable to start transaction:" << db_.lastError();
        return false;
    }

    SqlQuery select(db_, "SELECT key FROM hosts WHERE id=?");
    select.addUInt64(host_id);

    if (!select.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    if (!select.next())
    {
        LOG(ERROR) << "Host not found:" << host_id;
        return false;
    }

    const QByteArray key = select.columnBlob(0);
    const qint64 timestamp = QDateTime::currentSecsSinceEpoch();

    const char kSql[] = "INSERT INTO hosts_remove (host_id, key, timestamp) VALUES (?, ?, ?)";
    SqlQuery insert(db_, kSql);
    insert.addUInt64(host_id);
    insert.addBlob(key);
    insert.addInt64(timestamp);

    if (!insert.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    SqlQuery del(db_, "DELETE FROM hosts WHERE id=?");
    del.addUInt64(host_id);

    if (!del.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << db_.lastError();
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

    SqlQuery query(db_, "SELECT 1 FROM hosts_remove WHERE host_id=?");
    query.addUInt64(host_id);

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

    SqlQuery query(db_, "DELETE FROM hosts_remove WHERE host_id=?");
    query.addUInt64(host_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return db_.changes() > 0;
}

//--------------------------------------------------------------------------------------------------
void Database::workspaceListWithAllAccess(qint64 user_id, qint64 workspace_id,
    proto::router::WorkspaceList* out) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    // Visible workspaces: those the admin has an access row for (optionally narrowed to workspace_id).
    // Indexed by id so the access query below can attach every member's entry.
    std::unordered_map<qint64, proto::router::Workspace*> by_id;
    {
        const std::string sql = strCat({
            "SELECT workspaces.id, workspaces.name, workspaces.comment FROM workspaces "
            "JOIN workspace_access ON workspace_access.workspace_id = workspaces.id "
            "AND workspace_access.user_id = ?",
            workspace_id > 0 ? " AND workspaces.id = ?" : ""});

        SqlQuery query(db_, sql);
        if (!query.isValid())
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            out->set_error_code(proto::router::kErrorInternalError);
            return;
        }

        query.addInt64(user_id);
        if (workspace_id > 0)
            query.addInt64(workspace_id);

        while (query.next())
        {
            proto::router::Workspace* item = out->add_workspace();
            item->set_entry_id(query.columnInt64(0));
            item->set_name(query.columnTextView(1));
            item->set_comment(query.columnBlobView(2));
            by_id.emplace(item->entry_id(), item);
        }
    }

    if (by_id.empty())
    {
        out->set_error_code(proto::router::kErrorOk);
        return;
    }

    // Every member's access entry for the visible workspaces in a single query (no per-workspace
    // round trip). The by_id lookup discards rows for workspaces the admin cannot see.
    const std::string sql = strCat({
        "SELECT workspace_id, user_id, wrapped_gk FROM workspace_access",
        workspace_id > 0 ? " WHERE workspace_id = ?" : ""});

    SqlQuery query(db_, sql);
    if (!query.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    if (workspace_id > 0)
        query.addInt64(workspace_id);

    while (query.next())
    {
        const auto it = by_id.find(query.columnInt64(0));
        if (it == by_id.end())
            continue;

        proto::router::WorkspaceAccess* access = it->second->add_access();
        access->set_user_id(query.columnInt64(1));
        access->set_wrapped_gk(query.columnBlobView(2));
    }

    out->set_error_code(proto::router::kErrorOk);
}

//--------------------------------------------------------------------------------------------------
void Database::workspaceListWithOwnAccess(qint64 user_id, qint64 workspace_id,
    proto::router::WorkspaceList* out) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    // The visibility join is on the user's own access row, so the same query already yields the
    // wrapped_gk it needs - no second query and no per-workspace round trip.
    const std::string sql = strCat({
        "SELECT workspaces.id, workspaces.name, workspaces.comment, workspace_access.wrapped_gk "
        "FROM workspaces JOIN workspace_access ON workspace_access.workspace_id = workspaces.id "
        "AND workspace_access.user_id = ?",
        workspace_id > 0 ? " AND workspaces.id = ?" : ""});

    SqlQuery query(db_, sql);
    if (!query.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    query.addInt64(user_id);
    if (workspace_id > 0)
        query.addInt64(workspace_id);

    while (query.next())
    {
        proto::router::Workspace* item = out->add_workspace();
        item->set_entry_id(query.columnInt64(0));
        item->set_name(query.columnTextView(1));
        item->set_comment(query.columnBlobView(2));

        proto::router::WorkspaceAccess* access = item->add_access();
        access->set_user_id(user_id);
        access->set_wrapped_gk(query.columnBlobView(3));
    }

    out->set_error_code(proto::router::kErrorOk);
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

    SqlQuery query(db_, "SELECT id, name, comment FROM workspaces WHERE id=?");
    query.addInt64(entry_id);

    if (!query.next())
        return Workspace();

    Workspace workspace;
    workspace.entry_id = query.columnInt64(0);
    workspace.name     = query.columnText(1);
    workspace.comment  = query.columnBlob(2);
    return workspace;
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::addWorkspace(std::string_view name, std::string_view comment,
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

    SqlTransaction transaction(db_);
    if (!transaction.begin())
    {
        LOG(ERROR) << "Unable to start transaction:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    SqlQuery check(db_, "SELECT 1 FROM workspaces WHERE name=?");
    check.addText(name);

    if (!check.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    if (check.next())
        return proto::router::kErrorAlreadyExists;

    SqlQuery insert_workspace(db_, "INSERT INTO workspaces (id, name, comment) VALUES (NULL, ?, ?)");
    insert_workspace.addText(name);
    insert_workspace.addBlob(comment);

    if (!insert_workspace.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    const qint64 new_id = db_.lastInsertRowId();

    const char kInsertAccessSql[] =
        "INSERT INTO workspace_access (workspace_id, user_id, wrapped_gk) VALUES (?, ?, ?)";
    SqlQuery insert_access(db_, kInsertAccessSql);

    for (const Workspace::Access& access : initial_access)
    {
        insert_access.reset();
        insert_access.addInt64(new_id);
        insert_access.addInt64(access.user_id);
        insert_access.addBlob(access.wrapped_gk);

        if (!insert_access.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return proto::router::kErrorInternalError;
        }
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    *entry_id = new_id;
    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::modifyWorkspace(qint64 entry_id, std::string_view name, std::string_view comment,
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

    std::set<qint64> desired_ids;
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

    SqlTransaction transaction(db_);
    if (!transaction.begin())
    {
        LOG(ERROR) << "Unable to start transaction:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    SqlQuery exists_check(db_, "SELECT 1 FROM workspaces WHERE id=?");
    exists_check.addInt64(entry_id);

    if (!exists_check.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    if (!exists_check.next())
        return proto::router::kErrorNotFound;

    SqlQuery name_check(db_, "SELECT id FROM workspaces WHERE name=?");
    name_check.addText(name);

    if (!name_check.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    if (name_check.next() && name_check.columnInt64(0) != entry_id)
        return proto::router::kErrorAlreadyExists;

    SqlQuery update_workspace(db_, "UPDATE workspaces SET name=?, comment=? WHERE id=?");
    update_workspace.addText(name);
    update_workspace.addBlob(comment);
    update_workspace.addInt64(entry_id);

    if (!update_workspace.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    SqlQuery select_current(db_, "SELECT user_id FROM workspace_access WHERE workspace_id=?");
    select_current.addInt64(entry_id);

    if (!select_current.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    std::set<qint64> current_ids;
    while (select_current.next())
        current_ids.insert(select_current.columnInt64(0));

    SqlQuery delete_access(db_, "DELETE FROM workspace_access WHERE workspace_id=? AND user_id=?");

    for (qint64 user_id : current_ids)
    {
        if (desired_ids.contains(user_id))
            continue;

        delete_access.reset();
        delete_access.addInt64(entry_id);
        delete_access.addInt64(user_id);

        if (!delete_access.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return proto::router::kErrorInternalError;
        }
    }

    const char kInsertAccessSql[] =
        "INSERT INTO workspace_access (workspace_id, user_id, wrapped_gk) VALUES (?, ?, ?)";
    SqlQuery insert_access(db_, kInsertAccessSql);

    for (const Workspace::Access& access : desired_access)
    {
        if (current_ids.contains(access.user_id))
            continue;

        if (access.wrapped_gk.isEmpty())
        {
            LOG(ERROR) << "Missing wrapped_gk for new access entry, user_id:" << access.user_id;
            return proto::router::kErrorInvalidData;
        }

        insert_access.reset();
        insert_access.addInt64(entry_id);
        insert_access.addInt64(access.user_id);
        insert_access.addBlob(access.wrapped_gk);

        if (!insert_access.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return proto::router::kErrorInternalError;
        }
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << db_.lastError();
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

    SqlQuery query(db_, "DELETE FROM workspaces WHERE id=?");
    query.addInt64(entry_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    if (db_.changes() == 0)
        return proto::router::kErrorNotFound;

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::setWorkspaceHosts(qint64 entry_id, const std::set<HostId>& desired_host_ids)
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

    for (HostId host_id : desired_host_ids)
    {
        if (host_id == kInvalidHostId)
        {
            LOG(ERROR) << "Invalid host_id in desired list:" << host_id;
            return proto::router::kErrorInvalidData;
        }
    }

    SqlTransaction transaction(db_);
    if (!transaction.begin())
    {
        LOG(ERROR) << "Unable to start transaction:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    // Release: hosts currently in this workspace but no longer wanted.
    SqlQuery select_current(db_, "SELECT id FROM hosts WHERE workspace_id=?");
    select_current.addInt64(entry_id);
    if (!select_current.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    SqlQuery release(db_, "UPDATE hosts SET workspace_id=0, group_id=0 WHERE id=?");
    while (select_current.next())
    {
        const HostId host_id = select_current.columnUInt64(0);
        if (desired_host_ids.contains(host_id))
            continue;

        release.reset();
        release.addUInt64(host_id);
        if (!release.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return proto::router::kErrorInternalError;
        }
    }

    // Claim: hosts the operator wants in this workspace. Only hosts that are unassigned or
    // already in this workspace are touched; hosts in another workspace stay put.
    SqlQuery claim(db_, "UPDATE hosts SET workspace_id=? WHERE id=? AND workspace_id IN (0, ?)");
    for (HostId host_id : desired_host_ids)
    {
        claim.reset();
        claim.addInt64(entry_id);
        claim.addUInt64(host_id);
        claim.addInt64(entry_id);
        if (!claim.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return proto::router::kErrorInternalError;
        }
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::set<qint64> Database::workspaceAccessIdsForUser(qint64 user_id) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return {};
    }

    SqlQuery query(db_, "SELECT workspace_id FROM workspace_access WHERE user_id=?");
    query.addInt64(user_id);

    std::set<qint64> result;
    while (query.next())
        result.insert(query.columnInt64(0));

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

    SqlQuery query(db_, "SELECT workspace_id, wrapped_gk FROM workspace_access WHERE user_id=?");
    query.addInt64(user_id);

    QList<Workspace::Access> result;
    while (query.next())
    {
        Workspace::Access access;
        access.workspace_id = query.columnInt64(0);
        access.user_id      = user_id;
        access.wrapped_gk   = query.columnBlob(1);
        result.append(access);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
bool Database::hasWorkspaceAccess(qint64 user_id, qint64 workspace_id) const
{
    if (!isValid() || user_id <= 0 || workspace_id <= 0)
        return false;

    SqlQuery query(db_, "SELECT 1 FROM workspace_access WHERE workspace_id=? AND user_id=?");
    query.addInt64(workspace_id);
    query.addInt64(user_id);

    return query.next();
}

//--------------------------------------------------------------------------------------------------
bool Database::setWorkspaceKeysForUser(
    qint64 user_id, const std::unordered_map<qint64, QByteArray>& wrapped_keys)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlTransaction transaction(db_);
    if (!transaction.begin())
    {
        LOG(ERROR) << "Unable to start transaction:" << db_.lastError();
        return false;
    }

    QList<qint64> workspace_ids;
    {
        SqlQuery select(db_, "SELECT workspace_id FROM workspace_access WHERE user_id=?");
        select.addInt64(user_id);

        if (!select.isValid())
        {
            LOG(ERROR) << "Unable to read workspace access:" << db_.lastError();
            return false;
        }

        while (select.next())
            workspace_ids.append(select.columnInt64(0));
    }

    for (qint64 workspace_id : std::as_const(workspace_ids))
    {
        const auto it = wrapped_keys.find(workspace_id);

        if (it != wrapped_keys.end())
        {
            const char kSql[] =
                "UPDATE workspace_access SET wrapped_gk=? WHERE workspace_id=? AND user_id=?";
            SqlQuery query(db_, kSql);
            query.addBlob(it->second);
            query.addInt64(workspace_id);
            query.addInt64(user_id);

            if (!query.exec())
            {
                LOG(ERROR) << "Unable to update workspace access:" << db_.lastError();
                return false;
            }
        }
        else
        {
            SqlQuery query(db_, "DELETE FROM workspace_access WHERE workspace_id=? AND user_id=?");
            query.addInt64(workspace_id);
            query.addInt64(user_id);

            if (!query.exec())
            {
                LOG(ERROR) << "Unable to update workspace access:" << db_.lastError();
                return false;
            }
        }
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << db_.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void Database::groupList(qint64 workspace_id, proto::router::GroupList* out) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    if (workspace_id <= 0)
    {
        LOG(ERROR) << "Invalid workspace id:" << workspace_id;
        out->set_error_code(proto::router::kErrorInvalidData);
        return;
    }

    // Read every group of this workspace. parent_id is NULL for root groups in storage; we
    // coalesce it to 0 to match the convention used everywhere in the API. Rows come back in
    // whatever order SQLite produces them. Display ordering (alphabetic, locale-aware, and so on)
    // is the client's job. Building a tree from this result does not require any particular order:
    // index nodes by id, then link children to parents.
    const char kSql[] =
        "SELECT id, IFNULL(parent_id, 0), name, comment FROM host_groups WHERE workspace_id=?";
    SqlQuery query(db_, kSql);
    if (!query.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    query.addInt64(workspace_id);

    while (query.next())
    {
        proto::router::Group* group = out->add_group();
        group->set_entry_id(query.columnInt64(0));
        group->set_parent_id(query.columnInt64(1));
        group->set_name(query.columnTextView(2));
        group->set_comment(query.columnBlobView(3));
    }

    out->set_error_code(proto::router::kErrorOk);
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
    const std::string sql = strCat({
        "SELECT id, IFNULL(parent_id, 0), name, comment FROM host_groups WHERE workspace_id=? AND ",
        parent_id == 0 ? "parent_id IS NULL" : "parent_id=?",
        " ORDER BY name"});

    SqlQuery query(db_, sql);
    query.addInt64(workspace_id);
    if (parent_id != 0)
        query.addInt64(parent_id);

    QList<Group> groups;
    while (query.next())
    {
        Group group;
        group.entry_id  = query.columnInt64(0);
        group.parent_id = query.columnInt64(1);
        group.name      = query.columnText(2);
        group.comment   = query.columnBlob(3);
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
    const char kSql[] =
        "SELECT id, IFNULL(parent_id, 0), name, comment FROM host_groups "
        "WHERE id=? AND workspace_id=?";
    SqlQuery query(db_, kSql);
    query.addInt64(entry_id);
    query.addInt64(workspace_id);

    if (!query.next())
        return Group();

    Group group;
    group.entry_id  = query.columnInt64(0);
    group.parent_id = query.columnInt64(1);
    group.name      = query.columnText(2);
    group.comment   = query.columnBlob(3);
    return group;
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::addGroup(qint64 workspace_id, qint64 parent_id, std::string_view name,
    std::string_view comment, qint64* entry_id)
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

    if (strTrimmed(name).empty())
    {
        LOG(ERROR) << "Invalid group name";
        return proto::router::kErrorInvalidData;
    }

    SqlTransaction transaction(db_);
    if (!transaction.begin())
    {
        LOG(ERROR) << "Unable to start transaction:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    // The parent (if any) must exist in this workspace. Filtering on workspace_id here is the
    // only safeguard against the API silently linking groups across workspace boundaries.
    if (parent_id != 0)
    {
        SqlQuery parent_check(db_, "SELECT 1 FROM host_groups WHERE id=? AND workspace_id=?");
        parent_check.addInt64(parent_id);
        parent_check.addInt64(workspace_id);

        if (!parent_check.isValid())
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return proto::router::kErrorInternalError;
        }

        if (!parent_check.next())
            return proto::router::kErrorInvalidData;
    }

    const char kSql[] =
        "INSERT INTO host_groups (workspace_id, parent_id, name, comment) VALUES (?, ?, ?, ?)";
    SqlQuery insert(db_, kSql);
    insert.addInt64(workspace_id);
    if (parent_id == 0)
        insert.addNull();
    else
        insert.addInt64(parent_id);
    insert.addText(name);
    insert.addBlob(comment);

    if (!insert.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    const qint64 new_id = db_.lastInsertRowId();

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    *entry_id = new_id;
    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::modifyGroup(qint64 workspace_id, qint64 entry_id, qint64 new_parent_id,
    std::string_view name, std::string_view comment)
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

    if (strTrimmed(name).empty())
    {
        LOG(ERROR) << "Invalid group name";
        return proto::router::kErrorInvalidData;
    }

    SqlTransaction transaction(db_);
    if (!transaction.begin())
    {
        LOG(ERROR) << "Unable to start transaction:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    // Verify the group being modified exists in this workspace.
    SqlQuery select_self(db_, "SELECT 1 FROM host_groups WHERE id=? AND workspace_id=?");
    select_self.addInt64(entry_id);
    select_self.addInt64(workspace_id);

    if (!select_self.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    if (!select_self.next())
        return proto::router::kErrorNotFound;

    if (new_parent_id != 0)
    {
        // The new parent must exist in this workspace.
        SqlQuery parent_check(db_, "SELECT 1 FROM host_groups WHERE id=? AND workspace_id=?");
        parent_check.addInt64(new_parent_id);
        parent_check.addInt64(workspace_id);

        if (!parent_check.isValid())
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return proto::router::kErrorInternalError;
        }

        if (!parent_check.next())
            return proto::router::kErrorInvalidData;

        // Cycle protection. Walk parent links from new_parent_id upward; if entry_id appears
        // anywhere on that chain, the move would put the group inside its own subtree. The
        // recursive CTE bounds itself naturally on a well-formed tree (parent_id IS NULL marks
        // the top) and SQLite caps runaway recursion at SQLITE_MAX_RECURSION_DEPTH. parent_id
        // links never cross workspace boundaries by construction (enforced at insert and
        // modify), so no workspace_id filter is needed inside the recursion.
        const char kSql[] =
            "WITH RECURSIVE ancestors(id) AS ("
            "    SELECT id FROM host_groups WHERE id=?"
            "    UNION ALL"
            "    SELECT g.parent_id FROM host_groups g JOIN ancestors a ON g.id = a.id "
            "      WHERE g.parent_id IS NOT NULL"
            ") SELECT 1 FROM ancestors WHERE id=? LIMIT 1";
        SqlQuery cycle_check(db_, kSql);
        cycle_check.addInt64(new_parent_id);
        cycle_check.addInt64(entry_id);

        if (!cycle_check.isValid())
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return proto::router::kErrorInternalError;
        }

        if (cycle_check.next())
            return proto::router::kErrorInvalidData;
    }

    const char kSql[] =
        "UPDATE host_groups SET parent_id=?, name=?, comment=? WHERE id=? AND workspace_id=?";
    SqlQuery update_self(db_, kSql);
    if (new_parent_id == 0)
        update_self.addNull();
    else
        update_self.addInt64(new_parent_id);
    update_self.addText(name);
    update_self.addBlob(comment);
    update_self.addInt64(entry_id);
    update_self.addInt64(workspace_id);

    if (!update_self.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << db_.lastError();
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

    SqlQuery query(db_, "DELETE FROM host_groups WHERE id=? AND workspace_id=?");
    query.addInt64(entry_id);
    query.addInt64(workspace_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    if (db_.changes() == 0)
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

    if (!db_.open(file_path))
        return false;

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

    // Foreign keys are off by default in SQLite, enable per-connection so the cascade rules
    // declared on the schema actually fire.
    if (!db_.exec("PRAGMA foreign_keys = ON"))
        LOG(WARNING) << "Unable to enable foreign keys:" << db_.lastError();

    // Write-Ahead Logging: concurrent readers do not block the writer, the writer does not
    // rewrite a rollback journal on every commit. synchronous stays at default FULL so durable
    // commits survive power loss.
    if (!db_.exec("PRAGMA journal_mode = WAL"))
        LOG(WARNING) << "Unable to enable WAL journal mode:" << db_.lastError();

    // Keep temporary B-trees (recursive CTEs, sorts without a covering index, GROUP BY) in
    // memory rather than on disk.
    if (!db_.exec("PRAGMA temp_store = MEMORY"))
        LOG(WARNING) << "Unable to set temp_store:" << db_.lastError();

    // Larger page cache (8 MiB instead of the default 2 MiB). Negative value means size in
    // kibibytes rather than page count. Keeps hot index pages and recently accessed rows
    // resident as the database grows.
    if (!db_.exec("PRAGMA cache_size = -8000"))
        LOG(WARNING) << "Unable to set cache_size:" << db_.lastError();

    if (!ensureSchema(db_))
    {
        db_.close();
        return false;
    }

    return true;
}

