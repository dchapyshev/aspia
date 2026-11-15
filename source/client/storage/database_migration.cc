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

#include "client/storage/database_migration.h"

#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QStringList>
#include <QUuid>

#include <optional>

#include "base/logging.h"
#include "base/serialization.h"
#include "base/crypto/data_cryptor.h"
#include "base/crypto/secure_byte_array.h"
#include "base/crypto/secure_memory.h"
#include "base/files/base_paths.h"
#include "base/sql/sql_database.h"
#include "base/sql/sql_query.h"
#include "base/sql/sql_transaction.h"
#include "client/storage/database.h"
#include "client/storage/user_settings.h"
#include "proto/router.h"
#include "proto/storage.h"

namespace {

const char kOldFileName[] = "client.db3";

// The id of the router of the old file a database waits for. The row goes once the router is there.
const char kSettingRouterId[] = "migration_router_id";
const char kSettingName[] = "database_name";

// The rows of the old file that go to UserSettings instead of the databases.
const char kUserSettings[] = "'confirm_close', 'check_updates', 'update_channel', 'mcp_enabled', 'mcp_auto_confirm', "
                             "'mcp_sign_in', 'mcp_session_types'";

// How long a start waits for another one that migrates the same files.
const int kLockTimeout = 30000;

enum class Kind { INTEGER, TEXT, BLOB };

struct Column
{
    const char* name;
    Kind kind;
};

struct OldRouter
{
    qint64 id = 0;
    QString guid;
};

//--------------------------------------------------------------------------------------------------
bool isValidGuid(const QString& guid)
{
    return !guid.isEmpty() && QUuid::fromString(guid).toString(QUuid::WithoutBraces) == guid;
}

//--------------------------------------------------------------------------------------------------
SecureString toSecureString(const std::string& value)
{
    return SecureString::fromUtf8(SecureByteArray(value.data(), static_cast<qsizetype>(value.size())));
}

//--------------------------------------------------------------------------------------------------
// The columns of |table| in |db|, none when there is no such table. The old file keeps the shape of
// the version that wrote it, since it is not upgraded before it is read.
bool tableColumns(SqlDatabase& db, const char* table, QStringList* columns)
{
    columns->clear();

    const std::string sql = QString("PRAGMA table_info(\"%1\")").arg(table).toStdString();
    SqlQuery query(db, sql);

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to read the columns of" << table << ":" << db.lastError();
            return false;
        }
        if (step == SqlQuery::StepResult::DONE)
            return true;

        columns->append(query.columnText(1));
    }
}

//--------------------------------------------------------------------------------------------------
// Copies the rows of |table| that match |condition| with the columns of |columns| the old file has.
// A column it lacks keeps the default of the new table, and a table it lacks is nothing to copy.
bool copyRows(SqlDatabase& from, SqlDatabase& to, const char* table, const QList<Column>& columns,
              const char* condition = nullptr, std::optional<qint64> argument = std::nullopt)
{
    QStringList present;
    if (!tableColumns(from, table, &present))
        return false;

    QList<Column> copied;
    QStringList names;
    for (const Column& column : columns)
    {
        if (!present.contains(QLatin1String(column.name)))
            continue;

        copied.append(column);
        names.append(QString("\"%1\"").arg(column.name));
    }

    if (copied.isEmpty())
        return true;

    QString select_sql = QString("SELECT %1 FROM \"%2\"").arg(names.join(','), table);
    if (condition)
        select_sql += QString(" WHERE ") + condition;

    QStringList placeholders;
    for (qsizetype i = 0; i < copied.size(); ++i)
        placeholders.append("?");

    const QString insert_sql =
        QString("INSERT INTO \"%1\" (%2) VALUES (%3)").arg(table, names.join(','), placeholders.join(','));

    const std::string select_text = select_sql.toStdString();
    const std::string insert_text = insert_sql.toStdString();

    SqlQuery select(from, select_text);
    if (argument.has_value())
        select.addInt64(*argument);

    SqlQuery insert(to, insert_text);

    for (;;)
    {
        const SqlQuery::StepResult step = select.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to read" << table << ":" << from.lastError();
            return false;
        }
        if (step == SqlQuery::StepResult::DONE)
            return true;

        insert.reset();

        for (int i = 0; i < static_cast<int>(copied.size()); ++i)
        {
            if (select.columnIsNull(i))
            {
                insert.addNull();
                continue;
            }

            switch (copied[i].kind)
            {
                case Kind::INTEGER:
                    insert.addInt64(select.columnInt64(i));
                    break;
                case Kind::TEXT:
                    insert.addText(select.columnText(i));
                    break;
                case Kind::BLOB:
                    insert.addBlob(select.columnBlob(i));
                    break;
            }
        }

        if (!insert.exec())
        {
            LOG(ERROR) << "Unable to write" << table << ":" << to.lastError();
            return false;
        }
    }
}

//--------------------------------------------------------------------------------------------------
bool writeSetting(SqlDatabase& db, const char* name, const QString& value)
{
    SqlQuery query(db, "INSERT OR REPLACE INTO settings (name, value) VALUES (?, ?)");
    query.addText(std::string_view(name));
    query.addText(value);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to write setting" << name << ":" << db.lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
// The routers of the old file, the first of them first.
bool readRouters(SqlDatabase& db, QList<OldRouter>* routers)
{
    QStringList columns;
    if (!tableColumns(db, "routers", &columns))
        return false;

    if (columns.isEmpty())
        return true;

    SqlQuery query(db, "SELECT id, guid FROM routers ORDER BY id");

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to read the routers:" << db.lastError();
            return false;
        }
        if (step == SqlQuery::StepResult::DONE)
            return true;

        routers->append(OldRouter{ query.columnInt64(0), query.columnText(1) });
    }
}

//--------------------------------------------------------------------------------------------------
void copyUserSettings(SqlDatabase& db)
{
    SqlQuery query(db, std::string("SELECT name, value FROM settings WHERE name IN (") + kUserSettings + ")");
    UserSettings settings;

    SqlQuery::StepResult step;
    while ((step = query.next()) == SqlQuery::StepResult::ROW)
    {
        const std::string_view name = query.columnTextView(0);
        const std::string_view value = query.columnTextView(1);

        if (name == "confirm_close")
        {
            settings.setConfirmCloseEnabled(value == "1");
        }
        else if (name == "check_updates")
        {
            settings.setCheckUpdatesEnabled(value.empty() || value == "1");
        }
        else if (name == "update_channel")
        {
            settings.setUpdateChannel(query.columnText(1));
        }
        else if (name == "mcp_enabled")
        {
            settings.setMcpEnabled(value == "1");
        }
        else if (name == "mcp_auto_confirm")
        {
            settings.setMcpAutoConfirmEnabled(value == "1");
        }
        else if (name == "mcp_sign_in")
        {
            settings.setMcpSignInEnabled(value != "0");
        }
        else
        {
            bool ok = false;
            const quint32 session_types = query.columnText(1).toUInt(&ok);
            if (ok)
                settings.setMcpSessionTypes(session_types);
        }
    }

    if (step == SqlQuery::StepResult::FAILED)
        LOG(ERROR) << "Unable to read the settings of the user:" << db.lastError();
}

//--------------------------------------------------------------------------------------------------
// Everything of the old file that opens with any row, except the hosts of other routers.
bool copyBook(SqlDatabase& from, SqlDatabase& to, qint64 router_id)
{
    // The row naming the router is of the new file alone.
    const std::string condition =
        std::string("\"name\" NOT IN ('router', 'migration_router_id', ") + kUserSettings + ")";
    if (!copyRows(from, to, "settings", { { "name", Kind::TEXT }, { "value", Kind::TEXT } }, condition.c_str()))
    {
        return false;
    }

    if (!copyRows(from, to, "credentials",
                  { { "id", Kind::INTEGER }, { "type", Kind::INTEGER }, { "name", Kind::TEXT },
                    { "data", Kind::BLOB }, { "guid", Kind::TEXT } }))
    {
        return false;
    }

    if (!copyRows(from, to, "local_groups",
                  { { "id", Kind::INTEGER }, { "parent_id", Kind::INTEGER }, { "name", Kind::TEXT },
                    { "comment", Kind::TEXT }, { "guid", Kind::TEXT } }))
    {
        return false;
    }

    // A direct host goes to every database, a host of a router to the database of its router only.
    // A host of a router the old file no longer has goes nowhere.
    return copyRows(from, to, "local_hosts",
                    { { "id", Kind::INTEGER }, { "group_id", Kind::INTEGER }, { "name", Kind::TEXT },
                      { "comment", Kind::TEXT }, { "data", Kind::BLOB }, { "create_time", Kind::INTEGER },
                      { "modify_time", Kind::INTEGER }, { "connect_time", Kind::INTEGER },
                      { "guid", Kind::TEXT }, { "credential_id", Kind::INTEGER },
                      { "os_credential_id", Kind::INTEGER } },
                    "\"router_id\" = 0 OR \"router_id\" = ?", router_id);
}

//--------------------------------------------------------------------------------------------------
// The router |router_id| of the old file. Left empty when the file has no such router or its record
// does not open. Returns false when the file could not be read.
bool readRouter(SqlDatabase& db, qint64 router_id, std::optional<RouterConfig>* router)
{
    router->reset();

    SqlQuery query(db, "SELECT session_type, data FROM routers WHERE id=?");
    query.addInt64(router_id);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to read the router:" << db.lastError();
        return false;
    }

    if (step == SqlQuery::StepResult::DONE)
    {
        LOG(ERROR) << "Router" << router_id << "is not in the old file";
        return true;
    }

    // The type of the session was a column of its own, and the record holds it now. The name of the
    // router is not moved, a database has one router and it needs no name.
    RouterConfig config;
    const bool opened = config.setEncryptedData(query.columnBlob(1));
    config.setSessionType(static_cast<proto::router::SessionType>(query.columnInt64(0)));
    config.setEnabled(true);

    if (!opened || !config.isValid())
    {
        LOG(ERROR) << "Router" << router_id << "does not open, it is not moved";
        return true;
    }

    *router = config;
    return true;
}

//--------------------------------------------------------------------------------------------------
// The credentials of the hosts of the router |router_id| of the old file. A row that does not open is
// left out, the rest are moved.
bool readRouterHosts(SqlDatabase& db, qint64 router_id, QList<RouterHostConfig>* hosts)
{
    QStringList columns;
    if (!tableColumns(db, "router_hosts", &columns))
        return false;

    if (columns.isEmpty())
        return true;

    SqlQuery query(db, "SELECT host_id, data, IFNULL(credential_id, 0), IFNULL(os_credential_id, 0) "
                       "FROM router_hosts WHERE router_id=?");
    query.addInt64(router_id);

    DataCryptor& cryptor = DataCryptor::instance();

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to read the router hosts:" << db.lastError();
            return false;
        }
        if (step == SqlQuery::StepResult::DONE)
            return true;

        RouterHostConfig host;
        host.setHostId(query.columnUInt64(0));
        host.setCredentialId(query.columnInt64(2));
        host.setOsCredentialId(query.columnInt64(3));

        // The row was sealed for the router it belonged to as well.
        const QByteArray blob = query.columnBlob(1);
        if (!blob.isEmpty())
        {
            const QByteArray aad = QByteArrayLiteral("router_hosts/") + QByteArray::number(router_id) + '/' +
                                   QByteArray::number(host.hostId());

            std::optional<QByteArray> decrypted = cryptor.decrypt(blob, aad);
            if (!decrypted.has_value())
            {
                LOG(ERROR) << "Credentials of router host" << host.hostId() << "do not open, they are not moved";
                continue;
            }

            const SecureByteArray plain(std::move(*decrypted));

            proto::storage::HostBlob data;
            if (!parse(plain.toByteArray(), &data))
            {
                LOG(ERROR) << "Credentials of router host" << host.hostId() << "do not parse, they are not moved";
                continue;
            }

            host.setUsername(QString::fromStdString(data.username()));
            host.setPassword(toSecureString(data.password()));

            memZero(data.mutable_username());
            memZero(data.mutable_password());
        }

        if (!host.isValid())
        {
            LOG(ERROR) << "Router host" << host.hostId() << "is not valid, it is not moved";
            continue;
        }

        hosts->append(host);
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
QString DatabaseMigration::oldFilePath()
{
    const QString dir_path = BasePaths::appUserDataDir();
    if (dir_path.isEmpty())
        return QString();

    return dir_path + '/' + kOldFileName;
}

//--------------------------------------------------------------------------------------------------
// static
DatabaseMigration::Result DatabaseMigration::migrate(const QString& old_file_path, const QString& dir_path)
{
    if (QFileInfo::exists(dir_path) || !QFileInfo::exists(old_file_path))
        return Result::NOT_NEEDED;

    QLockFile lock(dir_path + ".lock");
    if (!lock.tryLock(kLockTimeout))
    {
        LOG(ERROR) << "Unable to lock the migration:" << lock.error();
        return Result::FAILED;
    }

    // Another start may have done it while this one waited for the lock.
    if (QFileInfo::exists(dir_path))
        return Result::NOT_NEEDED;

    LOG(INFO) << "Migrating" << old_file_path << "into" << dir_path;

    const QString temp_path = dir_path + ".tmp";

    // What a start that failed halfway left behind.
    if (!QDir(temp_path).removeRecursively() || !QDir().mkpath(temp_path))
    {
        LOG(ERROR) << "Unable to prepare the directory:" << temp_path;
        return Result::FAILED;
    }

    if (!build(old_file_path, temp_path) || !QDir().rename(temp_path, dir_path))
    {
        LOG(ERROR) << "Unable to migrate" << old_file_path;
        QDir(temp_path).removeRecursively();
        return Result::FAILED;
    }

    LOG(INFO) << "Migration done";
    return Result::DONE;
}

//--------------------------------------------------------------------------------------------------
// static
bool DatabaseMigration::finish(Database& db, const QString& old_file_path)
{
    bool found = false;
    const QString value = db.readSetting(kSettingRouterId, &found);
    if (!found)
        return true;

    const qint64 router_id = value.toLongLong();

    SqlDatabase old_db;
    if (!old_db.open(old_file_path, SqlDatabase::OpenMode::READ_ONLY))
    {
        LOG(ERROR) << "Unable to open" << old_file_path;
        return false;
    }

    std::optional<RouterConfig> router;
    QList<RouterHostConfig> hosts;
    if (!readRouter(old_db, router_id, &router) || !readRouterHosts(old_db, router_id, &hosts))
        return false;

    SqlTransaction transaction(db.db_);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction";
        return false;
    }

    // What the user entered here already stays as it is.
    RouterConfig stored_router;
    if (router.has_value() && db.routerConfig(&stored_router) == Database::FindResult::NOT_FOUND &&
        !db.setRouterConfig(*router))
    {
        return false;
    }

    for (const RouterHostConfig& host : std::as_const(hosts))
    {
        RouterHostConfig stored_host;
        if (db.findRouterHost(host.hostId(), &stored_host) != Database::FindResult::NOT_FOUND)
            continue;

        if (!db.addRouterHost(host))
            return false;

        SqlQuery reset(db.db_, "UPDATE router_hosts SET modify_time=0 WHERE host_id=?");
        reset.addUInt64(host.hostId());

        if (!reset.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db.db_.lastError();
            return false;
        }
    }

    SqlQuery query(db.db_, "DELETE FROM settings WHERE name=?");
    query.addText(std::string_view(kSettingRouterId));

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db.db_.lastError();
        return false;
    }

    if (!transaction.commit())
        return false;

    LOG(INFO) << "Router" << router_id << "of the old file is moved with" << hosts.size() << "hosts";
    return true;
}

//--------------------------------------------------------------------------------------------------
// static
bool DatabaseMigration::build(const QString& old_file_path, const QString& dir_path)
{
    SqlDatabase old_db;
    if (!old_db.open(old_file_path, SqlDatabase::OpenMode::READ_ONLY))
    {
        LOG(ERROR) << "Unable to open" << old_file_path;
        return false;
    }

    QList<OldRouter> routers;
    if (!readRouters(old_db, &routers))
        return false;

    copyUserSettings(old_db);

    if (routers.isEmpty())
    {
        const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        return buildDatabase(old_db, dir_path + '/' + id + ".db3", 0, QString("Database #1"));
    }

    for (qsizetype i = 0; i < routers.size(); ++i)
    {
        const OldRouter& router = routers[i];

        // A guid that is not one would make a poor file name, so the database gets a new one.
        QString id = router.guid;
        if (!isValidGuid(id))
            id = QUuid::createUuid().toString(QUuid::WithoutBraces);

        const QString name = QString("Database #%1").arg(i + 1);

        if (!buildDatabase(old_db, dir_path + '/' + id + ".db3", router.id, name))
            return false;

        if (i == 0)
            UserSettings().setLastDatabase(id);
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
// static
bool DatabaseMigration::buildDatabase(SqlDatabase& old_db, const QString& file_path, qint64 router_id,
                                      const QString& name)
{
    SqlDatabase db;
    if (!db.open(file_path))
        return false;

    if (!Database::createTables(db))
        return false;

    SqlTransaction transaction(db);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction";
        return false;
    }

    if (!copyBook(old_db, db, router_id))
        return false;

    if (!writeSetting(db, kSettingName, name))
        return false;

    if (router_id != 0 && !writeSetting(db, kSettingRouterId, QString::number(router_id)))
        return false;

    return transaction.commit();
}
