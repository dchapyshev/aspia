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
#include "proto/router.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"

namespace {

// The user created by --create-config always gets id 1 (AUTOINCREMENT on an empty table). It is the
// router's guaranteed way into the admin channel: the router has no CLI to restore administrator
// access, so losing it means losing control over the installation. The record is therefore not
// deletable and must stay enabled - a disabled account is refused by the authenticator, which would
// lock everyone out. Its admin session mask needs no extra guard: the mask of every user is
// immutable after creation (see I1 in database.h).
constexpr qint64 kBuiltInUserId = 1;

constexpr int kClientDeviceTokenSize = 32;
constexpr qint64 kClientDeviceTokenTtlSec = 7 * 24 * 3600; // 7 days, sliding window.

// How long a queued host removal waits for the host to acknowledge it before the record is dropped
// anyway. Long enough for a machine that spends months switched off; after it the id is gone and
// the host has to be approved again.
constexpr qint64 kHostRemovalTtlSec = 180 * 24 * 3600;

// Every host query has to name the page it wants. Whatever comes out of one goes into a single
// reply, and a reply the channel cannot carry is not sent at all but ends the session, so the size
// of an answer must never follow the size of the database. A request that names no page has a
// count of zero and is refused along with one that asks for more than the cap.
bool isHostPageValid(qint64 offset, qint64 count)
{
    return offset >= 0 && count > 0 && count <= proto::router::kMaxHostPageSize;
}

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

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to query table info:" << db.lastError();
        return true; // Same pessimism as above: a failed check must not trigger an ALTER.
    }

    return step == SqlQuery::StepResult::ROW;
}

//--------------------------------------------------------------------------------------------------
bool ensureSchema(SqlDatabase& db)
{
    SqlTransaction transaction(db);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
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
             "\"hwid\" BLOB NOT NULL DEFAULT X'',"
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

    // revision is the optimistic-concurrency counter: incremented on every modification, checked
    // against the value the client based its edit on, so a save built from a stale snapshot is
    // rejected instead of silently overwriting a concurrent change.
    if (!run("CREATE TABLE IF NOT EXISTS \"workspaces\" ("
             "\"id\" INTEGER UNIQUE,"
             "\"name\" TEXT NOT NULL UNIQUE,"
             "\"comment\" BLOB NOT NULL DEFAULT X'',"
             "\"revision\" INTEGER NOT NULL DEFAULT 1,"
             "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        return false;
    }

    // The revision column was added later; backfill it on upgraded databases.
    if (!hasColumn(db, "workspaces", "revision"))
    {
        if (!db.exec("ALTER TABLE \"workspaces\" ADD COLUMN \"revision\" INTEGER NOT NULL DEFAULT 1"))
        {
            LOG(ERROR) << "Unable to add column revision:" << db.lastError();
            return false;
        }
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

    // hosts used to be a thin (id, key) table; the columns below were added later (most moved
    // in from the now-gone computers table). Backfill any column that's missing on upgraded
    // databases.
    static const struct { const char* name; const char* definition; } kHostColumns[] = {
        { "hwid",          "BLOB NOT NULL DEFAULT X''"   },
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

    if (!database.db_.isOpen())
        database.openDatabase();

    return database;
}

//--------------------------------------------------------------------------------------------------
// static
QString Database::filePath()
{
    // The override lets tests and unusual deployments point the router at their own file.
    QString file_path = qEnvironmentVariable("ASPIA_ROUTER_DB_FILE");
    if (!file_path.isEmpty())
        return file_path;

    file_path = databaseDirectory();
    if (file_path.isEmpty())
        return QString();

    file_path.append("/router.db3");
    return file_path;
}

//--------------------------------------------------------------------------------------------------
bool Database::open(const QString& file_path)
{
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

//--------------------------------------------------------------------------------------------------
bool Database::isValid() const
{
    return db_.isOpen();
}

//--------------------------------------------------------------------------------------------------
bool Database::userList(std::vector<RouterUser>* users) const
{
    CHECK(users);

    users->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    const char kSql[] =
        "SELECT id, name, \"group\", salt, verifier, sessions, flags, public_key, "
        "wrap_private_key, wrap_salt, otp_secret, otp_counter FROM users";
    SqlQuery query(db_, kSql);

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

        users->emplace_back(readUser(query));
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::addUser(
    const RouterUser& user, const std::unordered_map<qint64, QByteArray>& wrapped_keys,
    qint64 grantor_user_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (!user.isValid())
    {
        LOG(ERROR) << "Not valid user";
        return proto::router::kErrorInvalidData;
    }

    // The whole set, not only the keys that get consumed, so a bad one is answered and not ignored.
    for (const auto& [workspace_id, wrapped_gk] : wrapped_keys)
    {
        if (wrapped_gk.isEmpty() ||
            wrapped_gk.size() > static_cast<qsizetype>(proto::router::kMaxWrappedKeyLength))
        {
            LOG(ERROR) << "Invalid wrapped key for workspace" << workspace_id;
            return proto::router::kErrorInvalidData;
        }
    }

    SqlTransaction transaction(db_);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to start transaction:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    // Checked here instead of relying on the UNIQUE constraint: the INSERT failure below cannot
    // be told apart from a real database error, so a lost create race would answer with a
    // misleading internal error.
    SqlQuery name_check(db_, "SELECT 1 FROM users WHERE name=?");
    name_check.addText(user.name);

    const SqlQuery::StepResult name_step = name_check.next();
    if (name_step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    if (name_step == SqlQuery::StepResult::ROW)
    {
        LOG(ERROR) << "User name already exists:" << user.name;
        return proto::router::kErrorAlreadyExists;
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
        return proto::router::kErrorInternalError;
    }

    if (user.sessions & proto::router::SESSION_TYPE_ADMIN)
    {
        // The new administrator must see every workspace right away.
        const qint64 user_id = db_.lastInsertRowId();
        const std::string_view error_code =
            grantMissingWorkspaceAccess(user_id, grantor_user_id, wrapped_keys);
        if (error_code != proto::router::kErrorOk)
            return error_code;
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::modifyUser(
    const RouterUser& user, const std::unordered_map<qint64, QByteArray>& wrapped_keys,
    qint64 grantor_user_id, bool* password_changed)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    // A request with empty credentials does not change them: the stored values are kept and the
    // rotation branch is skipped. The user dialog sends such a request when only the properties
    // of the record are edited, so a snapshot taken before a concurrent password change cannot
    // roll that change back.
    const bool has_credentials = !user.salt.isEmpty() || !user.verifier.isEmpty();
    if (has_credentials && !user.isValid())
    {
        LOG(ERROR) << "Not valid user";
        return proto::router::kErrorInvalidData;
    }

    for (const auto& [workspace_id, wrapped_gk] : wrapped_keys)
    {
        if (wrapped_gk.isEmpty() ||
            wrapped_gk.size() > static_cast<qsizetype>(proto::router::kMaxWrappedKeyLength))
        {
            LOG(ERROR) << "Invalid wrapped key for workspace" << workspace_id;
            return proto::router::kErrorInvalidData;
        }
    }

    if (user.entry_id == kBuiltInUserId && !(user.flags & User::ENABLED))
    {
        LOG(ERROR) << "Attempt to disable the built-in user";
        return proto::router::kErrorAccessDenied;
    }

    if (password_changed)
        *password_changed = false;

    SqlTransaction transaction(db_);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to start transaction:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    // Read the existing verifier first - a change to salt or verifier means a password rotation,
    // which must invalidate every device token and re-wrap the workspace keys. The stored session
    // mask tells whether the user is an administrator: the request is not authoritative about it,
    // the mask of the record never changes and is not written back here.
    QByteArray old_salt;
    QByteArray old_verifier;
    QByteArray old_public_key;
    quint32 sessions = 0;
    bool user_found = false;
    {
        SqlQuery select(db_, "SELECT salt, verifier, sessions, public_key FROM users WHERE id=?");
        select.addInt64(user.entry_id);

        if (!select.isValid())
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return proto::router::kErrorInternalError;
        }

        const SqlQuery::StepResult step = select.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            // A failed read must not pass for a missing user: the caller would report NotFound
            // for a row that is still there.
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return proto::router::kErrorInternalError;
        }

        if (step == SqlQuery::StepResult::ROW)
        {
            user_found = true;
            old_salt = select.columnBlob(0);
            old_verifier = select.columnBlob(1);
            sessions = static_cast<quint32>(select.columnInt64(2));
            old_public_key = select.columnBlob(3);
        }
    }

    if (!user_found)
        return proto::router::kErrorNotFound;

    // The sessions column is absent from the queries: the access level is set when the user is
    // created and never changes afterwards. A user that was an administrator has the keys of every
    // workspace, and taking the rights away would leave the access entries behind without a way to
    // revoke what the user already knows.
    if (has_credentials)
    {
        // Same reasoning as in addUser: a rename that lost a race must answer
        // kErrorAlreadyExists, not the internal error of the UNIQUE constraint.
        SqlQuery name_check(db_, "SELECT 1 FROM users WHERE name=? AND id!=?");
        name_check.addText(user.name);
        name_check.addInt64(user.entry_id);

        const SqlQuery::StepResult name_step = name_check.next();
        if (name_step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return proto::router::kErrorInternalError;
        }

        if (name_step == SqlQuery::StepResult::ROW)
        {
            LOG(ERROR) << "User name already exists:" << user.name;
            return proto::router::kErrorAlreadyExists;
        }

        const char kSql[] =
            "UPDATE users SET name=?, \"group\"=?, salt=?, verifier=?, flags=?, "
            "public_key=?, wrap_private_key=?, wrap_salt=? WHERE id=?";
        SqlQuery query(db_, kSql);
        query.addText(user.name);
        query.addText(user.group);
        query.addBlob(user.salt);
        query.addBlob(user.verifier);
        query.addInt64(user.flags);
        query.addBlob(user.public_key);
        query.addBlob(user.wrap_private_key);
        query.addBlob(user.wrap_salt);
        query.addInt64(user.entry_id);

        if (!query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return proto::router::kErrorInternalError;
        }
    }
    else
    {
        // Without the credentials only the flags can change: the name is not editable in the
        // dialog without re-entering the password, and writing it from a possibly out of date
        // snapshot would revert a concurrent rename the same way.
        SqlQuery query(db_, "UPDATE users SET flags=? WHERE id=?");
        query.addInt64(user.flags);
        query.addInt64(user.entry_id);

        if (!query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return proto::router::kErrorInternalError;
        }
    }

    // A changed public key alone is a rotation too: the UPDATE above already stored the new key,
    // and every wrapped_gk sealed to the old one became undecryptable the same way as after a
    // password change - so the same complete re-sealed set is required below.
    const bool rotated = has_credentials &&
        (old_salt != user.salt || old_verifier != user.verifier ||
         old_public_key != user.public_key);
    if (password_changed)
        *password_changed = rotated;

    if (rotated)
    {
        SqlQuery revoke(db_, "DELETE FROM client_device_tokens WHERE user_id=?");
        revoke.addInt64(user.entry_id);

        if (!revoke.exec())
        {
            LOG(ERROR) << "Unable to revoke device tokens:" << db_.lastError();
            return proto::router::kErrorInternalError;
        }

        // The rotation invalidated the stored wrapped GKs, so a freshly re-sealed key must be
        // present for every workspace the user can access. If any is missing, return without
        // committing - the transaction rolls back and the user keeps access.
        std::vector<qint64> workspace_ids;
        {
            SqlQuery select(db_, "SELECT workspace_id FROM workspace_access WHERE user_id=?");
            select.addInt64(user.entry_id);

            if (!select.isValid())
            {
                LOG(ERROR) << "Unable to read workspace access:" << db_.lastError();
                return proto::router::kErrorInternalError;
            }

            for (;;)
            {
                const SqlQuery::StepResult step = select.next();
                if (step == SqlQuery::StepResult::FAILED)
                {
                    // A partial scan must not pass for the full set: every entry missed here would
                    // keep a wrapped GK sealed to the old key pair - unusable after the rotation.
                    LOG(ERROR) << "Unable to read workspace access:" << db_.lastError();
                    return proto::router::kErrorInternalError;
                }

                if (step == SqlQuery::StepResult::DONE)
                    break;

                workspace_ids.emplace_back(select.columnInt64(0));
            }
        }

        for (qint64 workspace_id : std::as_const(workspace_ids))
        {
            const auto it = wrapped_keys.find(workspace_id);
            if (it == wrapped_keys.end() || it->second.isEmpty())
            {
                // The sender re-sealed the keys of every workspace it knew about, so a missing
                // one means its workspace list predates this entry - a conflict the sender
                // resolves by refetching the list and retrying, not bad data.
                LOG(ERROR) << "Missing re-sealed key for workspace" << workspace_id
                           << ". Rejecting to preserve access";
                return proto::router::kErrorConflict;
            }
        }

        const char kSql[] = "UPDATE workspace_access SET wrapped_gk=? WHERE workspace_id=? AND user_id=?";
        for (qint64 workspace_id : std::as_const(workspace_ids))
        {
            SqlQuery query(db_, kSql);
            query.addBlob(wrapped_keys.at(workspace_id));
            query.addInt64(workspace_id);
            query.addInt64(user.entry_id);

            if (!query.exec())
            {
                LOG(ERROR) << "Unable to update workspace access:" << db_.lastError();
                return proto::router::kErrorInternalError;
            }
        }
    }

    if (sessions & proto::router::SESSION_TYPE_ADMIN)
    {
        // A workspace could have been created by another administrator at the moment this one was
        // being created, and then it has no access entry for that workspace. Repaired here. The
        // keys of |wrapped_keys| are sealed to the public key of the record as the sender saw it,
        // so a flags-only request can repair only while that snapshot is still true: with no key
        // pair stored (a pre-upgrade user) there is nothing to seal to at all, and after a
        // concurrent password rotation the keys target a discarded pair - inserted entries could
        // never be unsealed, and the broken rows would block the password changes of the user.
        const bool keys_usable =
            has_credentials || (!old_public_key.isEmpty() && old_public_key == user.public_key);
        if (keys_usable)
        {
            const std::string_view error_code =
                grantMissingWorkspaceAccess(user.entry_id, grantor_user_id, wrapped_keys);
            if (error_code != proto::router::kErrorOk)
                return error_code;
        }
        else
        {
            LOG(WARNING) << "Workspace access repair skipped for user" << user.entry_id
                         << "- the request has no usable public key";
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
std::string_view Database::removeUser(qint64 entry_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (entry_id <= 0)
    {
        LOG(ERROR) << "Invalid user id:" << entry_id;
        return proto::router::kErrorInvalidData;
    }

    if (entry_id == kBuiltInUserId)
    {
        LOG(ERROR) << "Attempt to delete the built-in user";
        return proto::router::kErrorAccessDenied;
    }

    SqlTransaction transaction(db_);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to start transaction:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    // The cascade of the DELETE removes the user's workspace_access rows, which changes the
    // membership of those workspaces: a save built from a member list that still contains the
    // user must get kErrorConflict instead of re-adding it, so the revisions move together
    // with the delete (see I4).
    SqlQuery bump(db_,
        "UPDATE workspaces SET revision=revision+1 WHERE id IN "
        "(SELECT workspace_id FROM workspace_access WHERE user_id=?)");
    bump.addInt64(entry_id);

    if (!bump.exec())
    {
        LOG(ERROR) << "Unable to update workspace revisions:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    SqlQuery query(db_, "DELETE FROM users WHERE id=?");
    query.addInt64(entry_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    if (db_.changes() == 0)
        return proto::router::kErrorNotFound;

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    return proto::router::kErrorOk;
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

    if (query.next() != SqlQuery::StepResult::ROW)
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

    if (query.next() != SqlQuery::StepResult::ROW)
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

    SqlQuery query(db_, "UPDATE users SET otp_secret=?, otp_counter=? WHERE id=? AND otp_secret=X''");
    query.addBlob(encrypted_secret);
    query.addUInt64(counter);
    query.addInt64(user_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to set user OTP:" << db_.lastError();
        return false;
    }
    return db_.changes() > 0;
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::clearUserOtp(qint64 user_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (user_id <= 0)
    {
        LOG(ERROR) << "Invalid user id:" << user_id;
        return proto::router::kErrorInvalidData;
    }

    SqlTransaction transaction(db_);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to start transaction:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    // COUNT(*) always yields exactly one row, so a failed next() is a database error and not
    // a missing user.
    SqlQuery exists(db_, "SELECT COUNT(*) FROM users WHERE id=?");
    exists.addInt64(user_id);

    if (exists.next() != SqlQuery::StepResult::ROW)
    {
        LOG(ERROR) << "Unable to check user existence:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    if (exists.columnInt64(0) == 0)
        return proto::router::kErrorNotFound;

    SqlQuery query(db_, "UPDATE users SET otp_secret=X'', otp_counter=0 WHERE id=?");
    query.addInt64(user_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to clear user OTP:" << db_.lastError();
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
bool Database::consumeUserOtpCounter(qint64 user_id, quint64 counter)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "UPDATE users SET otp_counter=? WHERE id=? AND otp_counter < ?");
    query.addUInt64(counter);
    query.addInt64(user_id);
    query.addUInt64(counter);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to consume OTP counter:" << db_.lastError();
        return false;
    }
    return db_.changes() > 0;
}

//--------------------------------------------------------------------------------------------------
bool Database::issueClientDeviceToken(
    qint64 user_id, std::string_view address, std::string* token, qint64* token_id)
{
    CHECK(token);
    *token = std::string();
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
    std::string new_token = Random::string(kClientDeviceTokenSize);
    const QByteArray token_hash = GenericHash::hash(GenericHash::SHA256, new_token);
    const qint64 now = QDateTime::currentSecsSinceEpoch();

    // A device that is never used again leaves behind a row nobody can ever present. Issuing the
    // next token of the same user is the one moment its rows are guaranteed to be touched, so the
    // dead ones go here; without it the table only grows. Only this user is swept: a single login
    // must not turn into a pass over the whole table.
    SqlQuery prune(db_, "DELETE FROM client_device_tokens WHERE user_id=? AND last_used_at < ?");
    prune.addInt64(user_id);
    prune.addInt64(now - kClientDeviceTokenTtlSec);

    if (!prune.exec())
        LOG(WARNING) << "Unable to prune expired client device tokens:" << db_.lastError();

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

    *token = std::move(new_token);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::findClientDeviceToken(std::string_view token, qint64* user_id, qint64* token_id) const
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

    SqlTransaction transaction(db_);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to start transaction:" << db_.lastError();
        return false;
    }

    const char kSql[] =
        "SELECT user_id, last_used_at, token_id FROM client_device_tokens WHERE token_hash=?";
    SqlQuery query(db_, kSql);
    query.addBlob(token_hash);

    if (query.next() != SqlQuery::StepResult::ROW)
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
        else if (!transaction.commit())
            LOG(WARNING) << "Unable to commit transaction:" << db_.lastError();
        return false;
    }

    *user_id = query.columnInt64(0);
    if (token_id)
        *token_id = query.columnInt64(2);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::touchClientDeviceToken(std::string_view token, std::string_view address)
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
std::string_view Database::revokeClientDeviceTokens(qint64 user_id, const std::vector<qint64>& token_ids)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (token_ids.empty())
        return proto::router::kErrorOk;

    SqlTransaction transaction(db_);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to start transaction:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    for (qint64 token_id : token_ids)
    {
        SqlQuery query(db_, "DELETE FROM client_device_tokens WHERE token_id=? AND user_id=?");
        query.addInt64(token_id);
        query.addInt64(user_id);

        if (!query.exec())
        {
            LOG(ERROR) << "Unable to revoke client device token:" << db_.lastError();
            return proto::router::kErrorInternalError;
        }

        if (db_.changes() <= 0)
        {
            LOG(ERROR) << "Device token not found: user_id=" << user_id << "token_id=" << token_id;
            return proto::router::kErrorNotFound;
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
std::string_view Database::revokeUserClientDeviceTokens(qint64 user_id)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (user_id <= 0)
    {
        LOG(ERROR) << "Invalid user id:" << user_id;
        return proto::router::kErrorInvalidData;
    }

    SqlTransaction transaction(db_);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to start transaction:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    // COUNT(*) always yields exactly one row, so a failed next() is a database error and not
    // a missing user.
    SqlQuery exists(db_, "SELECT COUNT(*) FROM users WHERE id=?");
    exists.addInt64(user_id);

    if (exists.next() != SqlQuery::StepResult::ROW)
    {
        LOG(ERROR) << "Unable to check user existence:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    if (exists.columnInt64(0) == 0)
        return proto::router::kErrorNotFound;

    SqlQuery query(db_, "DELETE FROM client_device_tokens WHERE user_id=?");
    query.addInt64(user_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to revoke client device tokens:" << db_.lastError();
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
bool Database::listClientDeviceTokens(qint64 user_id, std::vector<DeviceToken>* tokens) const
{
    CHECK(tokens);

    tokens->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    // The list answers "which devices can log in as this user without a code", so it must hold
    // exactly what findClientDeviceToken() would still accept: a token past its lifetime is not a
    // credential any more, and reporting it would show access that nobody has. The boundary is the
    // same one the presentation path uses.
    const char kSql[] =
        "SELECT token_id, created_at, last_used_at, address "
        "FROM client_device_tokens WHERE user_id=? AND last_used_at >= ? ORDER BY created_at";
    SqlQuery query(db_, kSql);
    query.addInt64(user_id);
    query.addInt64(QDateTime::currentSecsSinceEpoch() - kClientDeviceTokenTtlSec);

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

        DeviceToken token;
        token.token_id     = query.columnInt64(0);
        token.created_at   = query.columnInt64(1);
        token.last_used_at = query.columnInt64(2);
        token.address      = query.columnTextView(3);
        tokens->emplace_back(std::move(token));
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::hostId(std::string_view key_hash, HostId* host_id) const
{
    CHECK(host_id);

    *host_id = kInvalidHostId;

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return proto::router::kErrorInternalError;
    }

    if (key_hash.empty())
    {
        LOG(ERROR) << "Invalid key hash";
        return proto::router::kErrorInvalidData;
    }

    SqlTransaction transaction(db_);
    if (!transaction.begin())
    {
        LOG(ERROR) << "Unable to start transaction:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    SqlQuery query(db_, "SELECT id FROM hosts WHERE key=?");
    query.addBlob(key_hash);

    if (!query.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    const SqlQuery::StepResult host_step = query.next();
    if (host_step == SqlQuery::StepResult::FAILED)
    {
        // A failed read must not pass for a missing row: NotFound sends the caller down the
        // approval path for a host that is already registered.
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    if (host_step == SqlQuery::StepResult::ROW)
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

    const SqlQuery::StepResult pending_step = pending.next();
    if (pending_step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    if (pending_step != SqlQuery::StepResult::ROW)
        return proto::router::kErrorNotFound;

    *host_id = pending.columnUInt64(0);
    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
bool Database::addHost(std::string_view key_hash, std::string_view hwid)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (key_hash.empty() || hwid.empty())
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    SqlTransaction transaction(db_);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to start transaction:" << db_.lastError();
        return false;
    }

    // Permanent host IDs are produced by AUTOINCREMENT and must never enter the temporary host ID range.
    // sqlite_sequence itself appears with the first AUTOINCREMENT insert into the database, so a
    // missing table is a legitimately empty sequence - only a failed read of an existing one must
    // not pass for it (the range check below would be skipped).
    HostId next_id = 1;
    SqlQuery seq_exists(db_,
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name='sqlite_sequence'");
    const SqlQuery::StepResult exists_step = seq_exists.next();
    if (exists_step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to check host id sequence table:" << db_.lastError();
        return false;
    }

    if (exists_step == SqlQuery::StepResult::ROW)
    {
        SqlQuery seq_query(db_, "SELECT seq FROM sqlite_sequence WHERE name='hosts'");
        const SqlQuery::StepResult seq_step = seq_query.next();
        if (seq_step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to read host id sequence:" << db_.lastError();
            return false;
        }
        if (seq_step == SqlQuery::StepResult::ROW)
            next_id = static_cast<HostId>(seq_query.columnInt64(0)) + 1;
    }

    if (next_id >= kMinTempHostId)
    {
        LOG(ERROR) << "Permanent host id space is exhausted";
        return false;
    }

    SqlQuery query(db_, "INSERT INTO hosts (id, key, hwid) VALUES (NULL, ?, ?)");
    query.addBlob(key_hash);
    query.addBlob(hwid);

    if (!query.exec())
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
bool Database::updateHostInfo(HostId host_id, std::string_view hwid, std::string_view computer_name,
    std::string_view cpu_arch, const QString& version, std::string_view os_name,
    std::string_view address)
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
        "hwid=?, computer_name=?, cpu_arch=?, version=?, os_name=?, address=?, last_connect=? "
        "WHERE id=?";
    SqlQuery query(db_, kSql);
    query.addText(computer_name);
    query.addBlob(hwid);
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

    // The row is expected to exist (addHost() runs first); a zero-row update means it was removed
    // in between, so report that instead of a silent success.
    return db_.changes() > 0;
}

//--------------------------------------------------------------------------------------------------
qint64 Database::hostWorkspaceId(HostId host_id, bool* ok) const
{
    if (ok)
        *ok = true;

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        if (ok)
            *ok = false;
        return -1;
    }

    SqlQuery query(db_, "SELECT workspace_id FROM hosts WHERE id=?");
    query.addUInt64(host_id);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        // A failed read must not pass for a missing host: the caller would answer NotFound for
        // a row that is still there.
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        if (ok)
            *ok = false;
        return -1;
    }

    if (step != SqlQuery::StepResult::ROW)
        return -1;
    return query.columnInt64(0);
}

//--------------------------------------------------------------------------------------------------
bool Database::modifyHost(HostId host_id, qint64 group_id, std::string_view display_name,
    std::string_view comment, std::string_view user_name, std::string_view password)
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
void Database::hosts(qint64 offset, qint64 count, proto::router::HostList* out) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    if (!isHostPageValid(offset, count))
    {
        LOG(ERROR) << "Invalid host list page: offset" << offset << "count" << count;
        out->set_error_code(proto::router::kErrorInvalidRequest);
        return;
    }
    const std::string sql = strCat({
        "SELECT id, workspace_id, group_id, display_name, computer_name, cpu_arch, version, "
        "os_name, address, comment, user_name, password, last_connect, last_modify FROM hosts",
        " LIMIT ? OFFSET ?"});

    SqlQuery query(db_, sql);
    if (!query.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    query.addInt64(count);
    query.addInt64(offset);

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            // An error reply must not carry the partial list scanned so far.
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            out->clear_host();
            out->set_error_code(proto::router::kErrorInternalError);
            return;
        }

        if (step == SqlQuery::StepResult::DONE)
            break;

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
void Database::hosts(qint64 workspace_id, qint64 group_id, qint64 offset,
    qint64 count, proto::router::HostList* out) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    if (!isHostPageValid(offset, count))
    {
        LOG(ERROR) << "Invalid host list page: offset" << offset << "count" << count;
        out->set_error_code(proto::router::kErrorInvalidRequest);
        return;
    }
    const std::string sql = strCat({
        "SELECT id, workspace_id, group_id, display_name, computer_name, cpu_arch, version, "
        "os_name, address, comment, user_name, password, last_connect, last_modify "
        "FROM hosts WHERE workspace_id=? AND group_id=?",
        " LIMIT ? OFFSET ?"});

    SqlQuery query(db_, sql);
    if (!query.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    query.addInt64(workspace_id);
    query.addInt64(group_id);
    query.addInt64(count);
    query.addInt64(offset);

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            // An error reply must not carry the partial list scanned so far.
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            out->clear_host();
            out->set_error_code(proto::router::kErrorInternalError);
            return;
        }

        if (step == SqlQuery::StepResult::DONE)
            break;

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
qint64 Database::hostCount(bool* ok) const
{
    if (ok)
        *ok = true;

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        if (ok)
            *ok = false;
        return 0;
    }

    // COUNT(*) always yields exactly one row, so anything but ROW is a database error - it must
    // not pass for an empty table.
    SqlQuery query(db_, "SELECT COUNT(*) FROM hosts");
    if (query.next() != SqlQuery::StepResult::ROW)
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        if (ok)
            *ok = false;
        return 0;
    }

    return query.columnInt64(0);
}

//--------------------------------------------------------------------------------------------------
qint64 Database::hostCount(qint64 workspace_id, qint64 group_id, bool* ok) const
{
    if (ok)
        *ok = true;

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        if (ok)
            *ok = false;
        return 0;
    }

    SqlQuery query(db_, "SELECT COUNT(*) FROM hosts WHERE workspace_id=? AND group_id=?");
    query.addInt64(workspace_id);
    query.addInt64(group_id);

    // See the unfiltered overload: a missing row is an error, not an empty scope.
    if (query.next() != SqlQuery::StepResult::ROW)
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        if (ok)
            *ok = false;
        return 0;
    }

    return query.columnInt64(0);
}

//--------------------------------------------------------------------------------------------------
void Database::searchHosts(const QString& query_text, const std::set<qint64>& workspace_ids,
    qint64 offset, qint64 count, proto::router::HostSearchResult* out) const
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    if (!isHostPageValid(offset, count))
    {
        LOG(ERROR) << "Invalid host search page: offset" << offset << "count" << count;
        out->set_error_code(proto::router::kErrorInvalidRequest);
        return;
    }

    if (workspace_ids.empty() || query_text.isEmpty())
    {
        out->set_total_count(0);
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

    // The predicate is written once and used by both statements, so the count and the page can
    // never disagree about what a match is.
    const QString where =
        " FROM hosts WHERE workspace_id IN (" + placeholders.join(',') + ") "
        "AND (casefold(display_name) LIKE casefold(?) ESCAPE '\\' "
        "OR CAST(id AS TEXT) LIKE ? ESCAPE '\\')";

    // A zero count from a failed query would make the client truncate its pagination while the
    // page itself arrives non-empty, so a count failure fails the whole request.
    const QString count_sql = "SELECT COUNT(*)" + where;
    SqlQuery count_query(db_, count_sql.toStdString());
    if (!count_query.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    for (qint64 workspace_id : workspace_ids)
        count_query.addInt64(workspace_id);
    count_query.addText(pattern);
    count_query.addText(pattern);

    // COUNT(*) always yields exactly one row, so anything but ROW is a database error.
    if (count_query.next() != SqlQuery::StepResult::ROW)
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    out->set_total_count(count_query.columnInt64(0));

    const QString sql =
        "SELECT id, workspace_id, group_id, display_name, computer_name, cpu_arch, "
        "version, os_name, address, comment, user_name, password, last_connect, last_modify" +
        where + " ORDER BY display_name LIMIT ? OFFSET ?";

    SqlQuery query(db_, sql.toStdString());
    if (!query.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        out->clear_total_count();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    for (qint64 workspace_id : workspace_ids)
        query.addInt64(workspace_id);
    query.addText(pattern);
    query.addText(pattern);
    query.addInt64(count);
    query.addInt64(offset);

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            // An error reply must not carry the partial list scanned so far, nor the count that
            // was read before it.
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            out->clear_host();
            out->clear_total_count();
            out->set_error_code(proto::router::kErrorInternalError);
            return;
        }

        if (step == SqlQuery::StepResult::DONE)
            break;

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
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
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

    if (select.next() != SqlQuery::StepResult::ROW)
    {
        LOG(ERROR) << "Host not found:" << host_id;
        return false;
    }

    const QByteArray key = select.columnBlob(0);
    const qint64 timestamp = QDateTime::currentSecsSinceEpoch();

    // OR REPLACE keeps re-scheduling idempotent: a stale hosts_remove row (same host_id, or same
    // key from a host that re-enrolled under a new id) is overwritten instead of aborting on the
    // PRIMARY KEY / UNIQUE constraint and leaving the host un-removable.
    const char kSql[] = "INSERT OR REPLACE INTO hosts_remove (host_id, key, timestamp) VALUES (?, ?, ?)";
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

    return query.next() == SqlQuery::StepResult::ROW;
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

    // Idempotent: the desired end state is "no hosts_remove row for this id". A second finalization
    // (duplicate ack, or another session already finalized) deletes zero rows but still succeeded,
    // so report success instead of a spurious failure.
    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::pruneExpiredHostRemovals()
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "DELETE FROM hosts_remove WHERE timestamp < ?");
    query.addInt64(QDateTime::currentSecsSinceEpoch() - kHostRemovalTtlSec);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to prune expired host removals:" << db_.lastError();
        return false;
    }

    const int count = db_.changes();
    if (count > 0)
        LOG(INFO) << "Dropped" << count << "host removals nobody acknowledged";

    return true;
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

    SqlTransaction transaction(db_);
    if (!transaction.begin())
    {
        LOG(ERROR) << "Unable to start transaction:" << db_.lastError();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    // Visible workspaces: those the admin has an access row for (optionally narrowed to workspace_id).
    // Indexed by id so the access query below can attach every member's entry.
    std::unordered_map<qint64, proto::router::Workspace*> by_id;
    {
        const std::string sql = strCat({
            "SELECT workspaces.id, workspaces.name, workspaces.comment, workspaces.revision "
            "FROM workspaces "
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

        for (;;)
        {
            const SqlQuery::StepResult step = query.next();
            if (step == SqlQuery::StepResult::FAILED)
            {
                // An error reply must not carry the partial list scanned so far.
                LOG(ERROR) << "Unable to execute query:" << db_.lastError();
                out->clear_workspace();
                out->set_error_code(proto::router::kErrorInternalError);
                return;
            }

            if (step == SqlQuery::StepResult::DONE)
                break;

            proto::router::Workspace* item = out->add_workspace();
            item->set_entry_id(query.columnInt64(0));
            item->set_name(query.columnTextView(1));
            item->set_comment(query.columnBlobView(2));
            item->set_revision(query.columnInt64(3));
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
        // The first loop already filled |out| - an error reply must not carry that partial list.
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        out->clear_workspace();
        out->set_error_code(proto::router::kErrorInternalError);
        return;
    }

    if (workspace_id > 0)
        query.addInt64(workspace_id);

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            // An error reply must not carry the partial list scanned so far.
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            out->clear_workspace();
            out->set_error_code(proto::router::kErrorInternalError);
            return;
        }

        if (step == SqlQuery::StepResult::DONE)
            break;

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
        "SELECT workspaces.id, workspaces.name, workspaces.comment, workspaces.revision, "
        "workspace_access.wrapped_gk "
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

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            // An error reply must not carry the partial list scanned so far.
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            out->clear_workspace();
            out->set_error_code(proto::router::kErrorInternalError);
            return;
        }

        if (step == SqlQuery::StepResult::DONE)
            break;

        proto::router::Workspace* item = out->add_workspace();
        item->set_entry_id(query.columnInt64(0));
        item->set_name(query.columnTextView(1));
        item->set_comment(query.columnBlobView(2));
        item->set_revision(query.columnInt64(3));

        proto::router::WorkspaceAccess* access = item->add_access();
        access->set_user_id(user_id);
        access->set_wrapped_gk(query.columnBlobView(4));
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

    if (query.next() != SqlQuery::StepResult::ROW)
        return Workspace();

    Workspace workspace;
    workspace.entry_id = query.columnInt64(0);
    workspace.name     = query.columnTextView(1);
    workspace.comment  = query.columnBlobView(2);
    return workspace;
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::addWorkspace(std::string_view name, std::string_view comment,
    const std::vector<Workspace::Access>& initial_access, const std::set<HostId>& desired_host_ids,
    qint64* entry_id)
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

    if (comment.size() > proto::router::kMaxCommentLength)
    {
        LOG(ERROR) << "Workspace comment is too long:" << comment.size();
        return proto::router::kErrorInvalidData;
    }

    std::set<qint64> initial_ids;
    for (const Workspace::Access& access : initial_access)
    {
        if (access.user_id <= 0 || access.public_key.empty() ||
            access.wrapped_gk.empty() ||
            access.wrapped_gk.size() > proto::router::kMaxWrappedKeyLength)
        {
            LOG(ERROR) << "Invalid access record";
            return proto::router::kErrorInvalidData;
        }

        if (initial_ids.contains(access.user_id))
        {
            LOG(ERROR) << "Duplicate user_id in initial access list:" << access.user_id;
            return proto::router::kErrorInvalidData;
        }

        initial_ids.insert(access.user_id);
    }

    SqlTransaction transaction(db_);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
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

    const SqlQuery::StepResult check_step = check.next();
    if (check_step == SqlQuery::StepResult::FAILED)
    {
        // A failed check must not pass for a free name: the INSERT below would then answer with
        // a misleading internal error from the UNIQUE constraint.
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    if (check_step == SqlQuery::StepResult::ROW)
        return proto::router::kErrorAlreadyExists;

    // After the name check: an incomplete access list must not mask the more specific
    // "already exists" answer.
    const std::string_view error_code = checkAccessCoversAdmins(initial_ids);
    if (error_code != proto::router::kErrorOk)
        return error_code;

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
        const std::string_view seal_error =
            checkAccessSealTarget(access.user_id, access.public_key);
        if (seal_error != proto::router::kErrorOk)
            return seal_error;

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

    // Same transaction as the workspace itself: a failed host assignment must not leave a
    // created workspace behind a reply that reports an error.
    const std::string_view host_error = syncWorkspaceHosts(new_id, desired_host_ids);
    if (host_error != proto::router::kErrorOk)
        return host_error;

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    *entry_id = new_id;
    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::modifyWorkspace(qint64 entry_id, qint64 base_revision,
    std::string_view name, std::string_view comment,
    const std::vector<Workspace::Access>& desired_access, const std::set<HostId>& desired_host_ids)
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

    if (comment.size() > proto::router::kMaxCommentLength)
    {
        LOG(ERROR) << "Workspace comment is too long:" << comment.size();
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
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to start transaction:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    // COUNT(*) always yields exactly one row, so a failed next() is a database error and not
    // a missing workspace.
    SqlQuery exists_check(db_, "SELECT COUNT(*), IFNULL(MAX(revision), 0) FROM workspaces WHERE id=?");
    exists_check.addInt64(entry_id);

    if (exists_check.next() != SqlQuery::StepResult::ROW)
    {
        LOG(ERROR) << "Unable to check workspace existence:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    if (exists_check.columnInt64(0) == 0)
        return proto::router::kErrorNotFound;

    if (exists_check.columnInt64(1) != base_revision)
    {
        // The edit was based on an older state of the workspace. Applying it would silently
        // overwrite whatever the concurrent change did (revoked access entries would be granted
        // back, released hosts claimed again), so the client must refetch and retry instead.
        LOG(ERROR) << "Workspace" << entry_id << "was changed concurrently (stored revision"
                   << exists_check.columnInt64(1) << ", request based on" << base_revision << ")";
        return proto::router::kErrorConflict;
    }

    SqlQuery name_check(db_, "SELECT id FROM workspaces WHERE name=?");
    name_check.addText(name);

    if (!name_check.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    const SqlQuery::StepResult name_step = name_check.next();
    if (name_step == SqlQuery::StepResult::FAILED)
    {
        // A failed check must not pass for a free name: the UPDATE below would then answer with
        // a misleading internal error from the UNIQUE constraint.
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    if (name_step == SqlQuery::StepResult::ROW && name_check.columnInt64(0) != entry_id)
        return proto::router::kErrorAlreadyExists;

    // After the existence and name checks: an incomplete access list must not mask the more
    // specific "not found" / "already exists" answers.
    const std::string_view error_code = checkAccessCoversAdmins(desired_ids);
    if (error_code != proto::router::kErrorOk)
        return error_code;

    SqlQuery update_workspace(db_,
        "UPDATE workspaces SET name=?, comment=?, revision=revision+1 WHERE id=?");
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
    for (;;)
    {
        const SqlQuery::StepResult step = select_current.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            // A partial set would make existing entries look new: the inserts below would collide
            // with them instead of preserving their wrapped_gk.
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return proto::router::kErrorInternalError;
        }

        if (step == SqlQuery::StepResult::DONE)
            break;

        current_ids.insert(select_current.columnInt64(0));
    }

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

        if (access.wrapped_gk.empty() ||
            access.wrapped_gk.size() > proto::router::kMaxWrappedKeyLength)
        {
            LOG(ERROR) << "Invalid wrapped_gk for new access entry, user_id:" << access.user_id;
            return proto::router::kErrorInvalidData;
        }

        const std::string_view seal_error =
            checkAccessSealTarget(access.user_id, access.public_key);
        if (seal_error != proto::router::kErrorOk)
            return seal_error;

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

    // Same transaction as the rest of the workspace: either all the changes are applied or none.
    const std::string_view host_error = syncWorkspaceHosts(entry_id, desired_host_ids);
    if (host_error != proto::router::kErrorOk)
        return host_error;

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

    SqlTransaction transaction(db_);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to start transaction:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    // COUNT(*) always yields exactly one row, so a failed next() is a database error and not
    // a missing workspace.
    SqlQuery exists_check(db_, "SELECT COUNT(*) FROM workspaces WHERE id=?");
    exists_check.addInt64(entry_id);

    if (exists_check.next() != SqlQuery::StepResult::ROW)
    {
        LOG(ERROR) << "Unable to check workspace existence:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    if (exists_check.columnInt64(0) == 0)
        return proto::router::kErrorNotFound;

    SqlQuery release_hosts(db_,
        "UPDATE hosts SET workspace_id=0, group_id=0, comment=X'', user_name=X'', password=X'' "
        "WHERE workspace_id=?");
    release_hosts.addInt64(entry_id);

    if (!release_hosts.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    SqlQuery delete_workspace(db_, "DELETE FROM workspaces WHERE id=?");
    delete_workspace.addInt64(entry_id);

    if (!delete_workspace.exec())
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
bool Database::workspaceAccessIdsForUser(qint64 user_id, std::set<qint64>* workspace_ids) const
{
    CHECK(workspace_ids);

    workspace_ids->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "SELECT workspace_id FROM workspace_access WHERE user_id=?");
    query.addInt64(user_id);

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

        workspace_ids->insert(query.columnInt64(0));
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::workspaceAccessListForUser(qint64 user_id, std::vector<Workspace::Access>* access_list) const
{
    CHECK(access_list);

    access_list->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlQuery query(db_, "SELECT workspace_id, wrapped_gk FROM workspace_access WHERE user_id=?");
    query.addInt64(user_id);

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

        Workspace::Access access;
        access.workspace_id = query.columnInt64(0);
        access.user_id      = user_id;
        access.wrapped_gk   = query.columnBlobView(1);
        access_list->emplace_back(std::move(access));
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::hasWorkspaceAccess(qint64 user_id, qint64 workspace_id, bool* ok) const
{
    if (ok)
        *ok = true;

    if (user_id <= 0 || workspace_id <= 0)
        return false;

    if (!isValid())
    {
        if (ok)
            *ok = false;
        return false;
    }

    SqlQuery query(db_, "SELECT 1 FROM workspace_access WHERE workspace_id=? AND user_id=?");
    query.addInt64(workspace_id);
    query.addInt64(user_id);

    const SqlQuery::StepResult step = query.next();
    if (ok)
        *ok = step != SqlQuery::StepResult::FAILED;
    return step == SqlQuery::StepResult::ROW;
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

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            // An error reply must not carry the partial list scanned so far.
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            out->clear_group();
            out->set_error_code(proto::router::kErrorInternalError);
            return;
        }

        if (step == SqlQuery::StepResult::DONE)
            break;

        proto::router::Group* group = out->add_group();
        group->set_entry_id(query.columnInt64(0));
        group->set_parent_id(query.columnInt64(1));
        group->set_name(query.columnTextView(2));
        group->set_comment(query.columnBlobView(3));
    }

    out->set_error_code(proto::router::kErrorOk);
}

//--------------------------------------------------------------------------------------------------
Group Database::findGroup(qint64 workspace_id, qint64 entry_id, bool* ok) const
{
    if (ok)
        *ok = true;

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        if (ok)
            *ok = false;
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

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        // A failed read must not pass for a missing group.
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        if (ok)
            *ok = false;
        return Group();
    }

    if (step != SqlQuery::StepResult::ROW)
        return Group();

    Group group;
    group.entry_id  = query.columnInt64(0);
    group.parent_id = query.columnInt64(1);
    group.name      = query.columnTextView(2);
    group.comment   = query.columnBlobView(3);
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

    if (strTrimmed(name).empty() || strTrimmed(name).size() > proto::router::kMaxEntryNameLength)
    {
        LOG(ERROR) << "Invalid group name";
        return proto::router::kErrorInvalidData;
    }

    if (comment.size() > proto::router::kMaxCommentLength)
    {
        LOG(ERROR) << "Group comment is too long:" << comment.size();
        return proto::router::kErrorInvalidData;
    }

    SqlTransaction transaction(db_);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
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

        if (parent_check.next() != SqlQuery::StepResult::ROW)
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

    if (strTrimmed(name).empty() || strTrimmed(name).size() > proto::router::kMaxEntryNameLength)
    {
        LOG(ERROR) << "Invalid group name";
        return proto::router::kErrorInvalidData;
    }

    if (comment.size() > proto::router::kMaxCommentLength)
    {
        LOG(ERROR) << "Group comment is too long:" << comment.size();
        return proto::router::kErrorInvalidData;
    }

    SqlTransaction transaction(db_);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
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

    if (select_self.next() != SqlQuery::StepResult::ROW)
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

        if (parent_check.next() != SqlQuery::StepResult::ROW)
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

        if (cycle_check.next() == SqlQuery::StepResult::ROW)
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

    SqlTransaction transaction(db_);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to start transaction:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    const char kReleaseHostsSql[] =
        "WITH RECURSIVE deleted_groups(id) AS ("
        "    SELECT id FROM host_groups WHERE id=? AND workspace_id=?"
        "    UNION ALL"
        "    SELECT child.id FROM host_groups child "
        "    JOIN deleted_groups parent ON child.parent_id = parent.id "
        "    WHERE child.workspace_id=?"
        ") UPDATE hosts SET group_id=0 WHERE workspace_id=? "
        "AND group_id IN (SELECT id FROM deleted_groups)";
    SqlQuery release_hosts(db_, kReleaseHostsSql);
    release_hosts.addInt64(entry_id);
    release_hosts.addInt64(workspace_id);
    release_hosts.addInt64(workspace_id);
    release_hosts.addInt64(workspace_id);

    if (!release_hosts.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    SqlQuery delete_group(db_, "DELETE FROM host_groups WHERE id=? AND workspace_id=?");
    delete_group.addInt64(entry_id);
    delete_group.addInt64(workspace_id);

    if (!delete_group.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    if (db_.changes() == 0)
        return proto::router::kErrorNotFound;

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
bool Database::openDatabase()
{
    const QString file_path = filePath();
    if (file_path.isEmpty())
    {
        LOG(ERROR) << "Invalid file path";
        return false;
    }

    const QString dir_path = QFileInfo(file_path).absolutePath();

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

    return open(filePath());
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::grantMissingWorkspaceAccess(
    qint64 user_id, qint64 grantor_user_id, const std::unordered_map<qint64, QByteArray>& wrapped_keys)
{
    if (user_id <= 0)
    {
        LOG(ERROR) << "Invalid user id:" << user_id;
        return proto::router::kErrorInvalidData;
    }

    // Collect the ids while the cursor is open and insert only afterwards - inserting into
    // workspace_access with the cursor still open could make the SELECT revisit its own rows.
    std::vector<qint64> missing_ids;
    {
        SqlQuery select(db_,
            "SELECT id FROM workspaces WHERE NOT EXISTS ("
            "SELECT 1 FROM workspace_access WHERE workspace_access.workspace_id = workspaces.id "
            "AND workspace_access.user_id = ?)");
        select.addInt64(user_id);

        if (!select.isValid())
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return proto::router::kErrorInternalError;
        }

        for (;;)
        {
            const SqlQuery::StepResult step = select.next();
            if (step == SqlQuery::StepResult::FAILED)
            {
                // A partial scan must not pass for the full set: every workspace missed here would
                // silently stay inaccessible to the user.
                LOG(ERROR) << "Unable to execute query:" << db_.lastError();
                return proto::router::kErrorInternalError;
            }

            if (step == SqlQuery::StepResult::DONE)
                break;

            missing_ids.emplace_back(select.columnInt64(0));
        }
    }

    SqlQuery insert(db_, "INSERT INTO workspace_access (workspace_id, user_id, wrapped_gk) VALUES (?, ?, ?)");
    SqlQuery bump(db_, "UPDATE workspaces SET revision=revision+1 WHERE id=?");

    for (qint64 workspace_id : std::as_const(missing_ids))
    {
        const auto it = wrapped_keys.find(workspace_id);
        if (it == wrapped_keys.end() || it->second.isEmpty())
        {
            // The skip-or-reject decision below needs the access of the grantor; without a
            // grantor it cannot be made at all. The callers always pass one when workspaces can
            // exist - reaching this line is a contract violation, not a data problem.
            if (grantor_user_id <= 0)
            {
                LOG(ERROR) << "No grantor to check access for workspace" << workspace_id;
                return proto::router::kErrorInternalError;
            }

            // The grantor can seal the key of every workspace it has access to itself, so a
            // missing key means it built the request from a list of the workspaces read before
            // this one appeared. Reject as a conflict: the sender refetches the list and
            // retries with the key - otherwise the user stays without access to it forever.
            // The skip-or-reject decision must not be made on an error - "no access" and "could
            // not check" are different answers.
            bool access_known = false;
            const bool grantor_has_access =
                hasWorkspaceAccess(grantor_user_id, workspace_id, &access_known);
            if (!access_known)
            {
                LOG(ERROR) << "Unable to check grantor access for workspace" << workspace_id;
                return proto::router::kErrorInternalError;
            }

            if (grantor_has_access)
            {
                LOG(ERROR) << "The request has no key for workspace" << workspace_id;
                return proto::router::kErrorConflict;
            }

            LOG(WARNING) << "No sealed key for workspace" << workspace_id << "- user" << user_id
                         << "will not have access to it";
            continue;
        }

        insert.reset();
        insert.addInt64(workspace_id);
        insert.addInt64(user_id);
        insert.addBlob(it->second);

        if (!insert.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return proto::router::kErrorInternalError;
        }

        // The membership of the workspace changed: a save built from a member list that misses
        // the new entry must get kErrorConflict instead of silently revoking it (see I4).
        bump.reset();
        bump.addInt64(workspace_id);

        if (!bump.exec())
        {
            LOG(ERROR) << "Unable to update workspace revision:" << db_.lastError();
            return proto::router::kErrorInternalError;
        }
    }

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::checkAccessCoversAdmins(const std::set<qint64>& access_user_ids)
{
    // An administrator without a public key is not required: nobody can seal the group key for it.
    // A user upgraded from a database of a previous version has no key pair until it changes its
    // password, and demanding its entry would make every workspace uneditable meanwhile.
    SqlQuery select(db_, "SELECT id FROM users WHERE (sessions & ?) != 0 AND length(public_key) != 0");
    select.addInt64(proto::router::SESSION_TYPE_ADMIN);

    if (!select.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    for (;;)
    {
        const SqlQuery::StepResult step = select.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            // The loop guards an invariant: a partially checked list must not pass for a complete one.
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return proto::router::kErrorInternalError;
        }

        if (step == SqlQuery::StepResult::DONE)
            break;

        const qint64 user_id = select.columnInt64(0);
        if (!access_user_ids.contains(user_id))
        {
            // Either the client is broken or (far more likely) the administrator was created
            // after the client took its user list. Reject as a conflict: refetching the users
            // auto-includes every administrator, so a retry resolves the race.
            LOG(ERROR) << "The access list has no administrator" << user_id;
            return proto::router::kErrorConflict;
        }
    }

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::checkAccessSealTarget(qint64 user_id, std::string_view public_key)
{
    SqlQuery select(db_, "SELECT public_key FROM users WHERE id=?");
    select.addInt64(user_id);

    if (!select.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    const SqlQuery::StepResult step = select.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    // Checked before the supplied key: an entry for a user that is gone is how a concurrent
    // delete looks from here (the sender still saw the user in its snapshot), and the recovery
    // is a refetch - so a conflict, not bad data.
    if (step != SqlQuery::StepResult::ROW)
    {
        LOG(ERROR) << "Access entry for unknown user:" << user_id;
        return proto::router::kErrorConflict;
    }

    if (public_key.empty())
    {
        LOG(ERROR) << "Access entry for user" << user_id << "carries no seal target";
        return proto::router::kErrorInvalidData;
    }

    const QByteArray stored_key = select.columnBlob(0);
    const std::string_view stored(stored_key.constData(), static_cast<size_t>(stored_key.size()));

    if (stored.empty())
    {
        // Nobody can seal the group key for a user without a key pair, so whatever the entry
        // was sealed to, the user could never unseal it.
        LOG(ERROR) << "Access entry for user" << user_id << "without a key pair";
        return proto::router::kErrorInvalidData;
    }

    if (stored != public_key)
    {
        // The sender sealed to a key the user no longer has (the password was rotated after the
        // sender took its snapshot). The entry could never be unsealed, and a broken row would
        // also block the password changes of the user - the rotation re-wrap requires a key for
        // every stored entry.
        LOG(ERROR) << "Access entry for user" << user_id << "is sealed to an out of date key";
        return proto::router::kErrorConflict;
    }

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view Database::syncWorkspaceHosts(qint64 entry_id, const std::set<HostId>& desired_host_ids)
{
    // Both callers validate the id, but this is the one place that releases every host of a
    // workspace and wipes their encrypted fields - with entry_id 0 the release scan would pick
    // up the whole pool of unassigned hosts, so the guard stays here as well.
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

    // Validate desired hosts before releasing anything. The caller supplies the final set, so
    // success must mean every requested host is actually assignable to this workspace.
    SqlQuery host_check(db_, "SELECT COUNT(*), IFNULL(MAX(workspace_id), 0) FROM hosts WHERE id=?");
    for (HostId host_id : desired_host_ids)
    {
        host_check.reset();
        host_check.addUInt64(host_id);

        if (host_check.next() != SqlQuery::StepResult::ROW)
        {
            LOG(ERROR) << "Unable to check host existence:" << db_.lastError();
            return proto::router::kErrorInternalError;
        }

        if (host_check.columnInt64(0) == 0)
        {
            // The host was in the snapshot of the sender and is gone now - a concurrent delete.
            // Reject as a conflict (not "not found", which the sender reads as a missing
            // workspace): a refetch drops the host from the desired set and the retry passes.
            LOG(ERROR) << "Host not found:" << host_id;
            return proto::router::kErrorConflict;
        }

        const qint64 current_workspace_id = host_check.columnInt64(1);
        if (current_workspace_id != 0 && current_workspace_id != entry_id)
        {
            // The sender saw the host unassigned, another workspace claimed it meanwhile - the
            // same lost race as above.
            LOG(ERROR) << "Host" << host_id << "belongs to another workspace:"
                       << current_workspace_id;
            return proto::router::kErrorConflict;
        }
    }

    // Release: hosts currently in this workspace but no longer wanted. Collect the ids while the
    // cursor is open and update only afterwards - UPDATE-ing workspace_id (the column the SELECT
    // filters on) with the cursor still open could skip or revisit rows on an index scan.
    SqlQuery select_current(db_, "SELECT id FROM hosts WHERE workspace_id=?");
    select_current.addInt64(entry_id);
    if (!select_current.isValid())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return proto::router::kErrorInternalError;
    }

    std::vector<HostId> release_ids;
    for (;;)
    {
        const SqlQuery::StepResult step = select_current.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            // A partial scan must not pass for the full set: OK means the final set was applied.
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return proto::router::kErrorInternalError;
        }

        if (step == SqlQuery::StepResult::DONE)
            break;

        const HostId host_id = select_current.columnUInt64(0);
        if (!desired_host_ids.contains(host_id))
            release_ids.emplace_back(host_id);
    }

    // The encrypted fields are sealed with the workspace group key, so a host outside any
    // workspace cannot keep them. Clear them together with the workspace assignment.
    SqlQuery release(db_,
        "UPDATE hosts SET workspace_id=0, group_id=0, comment=X'', user_name=X'', password=X'' "
        "WHERE id=?");
    for (HostId host_id : release_ids)
    {
        release.reset();
        release.addUInt64(host_id);
        if (!release.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return proto::router::kErrorInternalError;
        }
    }

    // Claim: hosts the operator wants in this workspace. Validation above guarantees every
    // desired host is unassigned or already in this workspace.
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

    return proto::router::kErrorOk;
}

