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

#include <QDir>
#include <QFileInfo>
#include <QUuid>

#include "base/logging.h"
#include "base/files/base_paths.h"
#include "base/sql/sql_query.h"
#include "base/sql/sql_transaction.h"

namespace {

//--------------------------------------------------------------------------------------------------
QString databaseDirectory()
{
    return BasePaths::appDataDir();
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

    // |otp_secret| is the shared secret, stored as generated (empty until self-enrollment
    // completes; non-empty implies 2FA is active for the user). TOTP requires the verifying
    // side to hold the secret itself, so the column cannot keep a hash the way
    // client_device_tokens does.
    // |otp_counter| is the highest TOTP step already consumed; the server refuses any
    // subsequent code whose step is less-than-or-equal to it (replay protection, per RFC 6238
    // section 5.2).
    if (!run("CREATE TABLE IF NOT EXISTS \"users\" ("
             "\"id\" INTEGER UNIQUE,"
             "\"name\" TEXT NOT NULL UNIQUE,"
             "\"group\" TEXT NOT NULL,"
             "\"salt\" BLOB NOT NULL,"
             "\"verifier\" BLOB NOT NULL,"
             "\"sessions\" INTEGER DEFAULT 0,"
             "\"flags\" INTEGER DEFAULT 0,"
             "\"otp_secret\" BLOB NOT NULL DEFAULT X'',"
             "\"otp_counter\" INTEGER NOT NULL DEFAULT 0,"
             "\"email\" TEXT NOT NULL DEFAULT '',"
             "\"guid\" TEXT NOT NULL DEFAULT '',"
             "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        return false;
    }

    // workspace_id == 0 means the host is not yet assigned to any workspace; group_id == 0
    // means the host is shown at the workspace root. No FKs on these columns because 0 is a
    // sentinel value; for any non-zero value the application enforces that it points to an
    // existing row in workspaces/host_groups.
    // name, computer_name (real OS hostname), cpu_arch, version, os_name, address and
    // last_connect are updated by the router on every host connection and reflect the latest
    // connect attempt.
    // revision is the optimistic-concurrency counter of the operator-edited fields
    // (workspace_id, group_id, display_name, comment): moved by operator edits and by the
    // cascades that touch those fields, never by what the host reports about itself, so a
    // reconnecting host cannot fail an open edit dialog.
    if (!run("CREATE TABLE IF NOT EXISTS \"hosts\" ("
             "\"id\" INTEGER UNIQUE,"
             "\"key\" BLOB NOT NULL UNIQUE,"
             "\"hwid\" BLOB NOT NULL DEFAULT X'',"
             "\"revision\" INTEGER NOT NULL DEFAULT 1,"
             "\"workspace_id\" INTEGER NOT NULL DEFAULT 0,"
             "\"group_id\" INTEGER NOT NULL DEFAULT 0,"
             "\"display_name\" TEXT NOT NULL DEFAULT '',"
             "\"computer_name\" TEXT NOT NULL DEFAULT '',"
             "\"cpu_arch\" TEXT NOT NULL DEFAULT '',"
             "\"version\" TEXT NOT NULL DEFAULT '',"
             "\"os_name\" TEXT NOT NULL DEFAULT '',"
             "\"address\" TEXT NOT NULL DEFAULT '',"
             "\"comment\" TEXT NOT NULL DEFAULT '',"
             "\"last_connect\" INTEGER NOT NULL DEFAULT 0,"
             "\"last_modify\" INTEGER NOT NULL DEFAULT 0,"
             "\"telemetry\" TEXT NOT NULL DEFAULT '',"
             "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        return false;
    }

    // revision is the optimistic-concurrency counter: incremented on every modification, checked
    // against the value the client based its edit on, so a save built from a stale snapshot is
    // rejected instead of silently overwriting a concurrent change.
    if (!run("CREATE TABLE IF NOT EXISTS \"workspaces\" ("
             "\"id\" INTEGER UNIQUE,"
             "\"revision\" INTEGER NOT NULL DEFAULT 1,"
             "\"name\" TEXT NOT NULL UNIQUE,"
             "\"comment\" TEXT NOT NULL DEFAULT '',"
             "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        return false;
    }

    if (!run("CREATE TABLE IF NOT EXISTS \"workspace_access\" ("
             "\"workspace_id\" INTEGER NOT NULL,"
             "\"user_id\" INTEGER NOT NULL,"
             "PRIMARY KEY(\"workspace_id\", \"user_id\"),"
             "FOREIGN KEY(\"workspace_id\") REFERENCES \"workspaces\"(\"id\") ON DELETE CASCADE,"
             "FOREIGN KEY(\"user_id\") REFERENCES \"users\"(\"id\") ON DELETE CASCADE)"))
    {
        return false;
    }

    if (!run("CREATE TABLE IF NOT EXISTS \"host_groups\" ("
             "\"id\" INTEGER PRIMARY KEY AUTOINCREMENT,"
             "\"revision\" INTEGER NOT NULL DEFAULT 1,"
             "\"workspace_id\" INTEGER NOT NULL,"
             "\"parent_id\" INTEGER,"
             "\"name\" TEXT NOT NULL,"
             "\"comment\" TEXT NOT NULL DEFAULT '',"
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

    // workspace_access's PRIMARY KEY is (workspace_id, user_id), which indexes only the leading
    // column. WorkspaceStore::accessIdsForUser filters by user_id alone, so we need a dedicated
    // index on the trailing column.
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

    // Bearer "remember this device" tokens issued during client sessions (host and relay
    // sessions do not use this flow). The raw token never reaches the database - only its SHA-256
    // hash is stored, so a leak of |router.db3| does not yield usable tokens.
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

    // Composite index for the dominant host list/count query (hosts of a given workspace and
    // group). Without it list-by-group does a full table scan once hosts grows.
    if (!run("CREATE INDEX IF NOT EXISTS \"hosts_workspace_group\" "
             "ON \"hosts\"(\"workspace_id\", \"group_id\")"))
    {
        return false;
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction:" << db.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool migrateTables(SqlDatabase& db)
{
    SqlTransaction transaction(db);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to start transaction:" << db.lastError();
        return false;
    }

    qint64 version = 0;

    {
        SqlQuery query(db, "PRAGMA user_version");
        if (query.next() != SqlQuery::StepResult::ROW)
        {
            LOG(ERROR) << "Unable to read the version of the schema:" << db.lastError();
            return false;
        }

        version = query.columnInt64(0);
    }

    //----------------------------------------------------------------------------------------------
    // 3.4.x -> 3.5.0
    //----------------------------------------------------------------------------------------------
    if (version < 1)
    {
        if (!db.addColumn("users", "guid", "TEXT NOT NULL DEFAULT ''"))
            return false;

        std::vector<qint64> users_without_guid;

        {
            SqlQuery query(db, "SELECT id FROM users WHERE guid=''");

            for (;;)
            {
                const SqlQuery::StepResult step = query.next();
                if (step == SqlQuery::StepResult::FAILED)
                {
                    LOG(ERROR) << "Unable to read the users without a guid:" << db.lastError();
                    return false;
                }

                if (step == SqlQuery::StepResult::DONE)
                    break;

                users_without_guid.emplace_back(query.columnInt64(0));
            }
        }

        for (qint64 user_id : users_without_guid)
        {
            SqlQuery query(db, "UPDATE users SET guid=? WHERE id=?");
            query.addText(QUuid::createUuid().toString(QUuid::WithoutBraces));
            query.addInt64(user_id);

            if (!query.exec())
            {
                LOG(ERROR) << "Unable to give the user" << user_id << "a guid:" << db.lastError();
                return false;
            }
        }

        if (!db.exec("PRAGMA user_version = 1"))
        {
            LOG(ERROR) << "Unable to update the schema to version 1:" << db.lastError();
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
        // The full check, not quick_check: only the full one compares every index with its
        // table, and an index out of step with the table reads fine and fails every write.
        SqlQuery pragma(db_, "PRAGMA integrity_check");
        if (pragma.next() == SqlQuery::StepResult::ROW)
        {
            const QString result = pragma.columnText(0);
            if (result != "ok")
                LOG(ERROR) << "Database integrity check failed:" << result;
        }
        else
        {
            LOG(WARNING) << "Unable to run integrity_check:" << db_.lastError();
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

    if (!ensureSchema(db_) || !migrateTables(db_))
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
