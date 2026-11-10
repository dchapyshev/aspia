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

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QUuid>

#include "base/logging.h"
#include "base/files/base_paths.h"
#include "base/sql/sql_query.h"
#include "base/sql/sql_transaction.h"

namespace {

QString g_test_file_path;

//--------------------------------------------------------------------------------------------------
// Adds |column| to a |table| that was made before the column was there.
bool addMissingColumn(SqlDatabase& db, const QString& table, const QString& column, const QString& definition)
{
    SqlQuery query(db, "SELECT 1 FROM pragma_table_info(?) WHERE name=? LIMIT 1");
    query.addText(table);
    query.addText(column);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to read the schema of table" << table << ":" << db.lastError();
        return false;
    }

    if (step == SqlQuery::StepResult::ROW)
        return true;

    LOG(INFO) << "Adding column" << column << "to table" << table;

    const QString sql = QString("ALTER TABLE \"%1\" ADD COLUMN \"%2\" %3").arg(table, column, definition);
    if (!db.exec(sql.toUtf8().constData()))
    {
        LOG(ERROR) << "Unable to add column" << column << "to table" << table << ":" << db.lastError();
        return false;
    }

    LOG(INFO) << "Column" << column << "added to table" << table;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool createTables(SqlDatabase& db)
{
    // A group without a parent and a host without a group are the ones at the root: the root is
    // where the tree starts and not a record of its own, so there is no id for them to name.
    if (!db.exec("CREATE TABLE IF NOT EXISTS \"local_groups\" ("
                 "\"id\" INTEGER UNIQUE,"
                 "\"parent_id\" INTEGER REFERENCES \"local_groups\"(\"id\") ON DELETE CASCADE,"
                 "\"name\" TEXT NOT NULL DEFAULT '',"
                 "\"comment\" TEXT NOT NULL DEFAULT '',"
                 "\"guid\" TEXT NOT NULL UNIQUE,"
                 "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        LOG(ERROR) << "Unable to create local_groups table:" << db.lastError();
        return false;
    }

    if (!db.exec("CREATE TABLE IF NOT EXISTS \"local_hosts\" ("
                 "\"id\" INTEGER UNIQUE,"
                 "\"group_id\" INTEGER REFERENCES \"local_groups\"(\"id\") ON DELETE SET NULL,"
                 "\"router_id\" INTEGER NOT NULL DEFAULT 0,"
                 "\"name\" TEXT NOT NULL DEFAULT '',"
                 "\"comment\" TEXT NOT NULL DEFAULT '',"
                 "\"data\" BLOB DEFAULT X'',"
                 "\"create_time\" INTEGER NOT NULL DEFAULT 0,"
                 "\"modify_time\" INTEGER NOT NULL DEFAULT 0,"
                 "\"connect_time\" INTEGER NOT NULL DEFAULT 0,"
                 "\"guid\" TEXT NOT NULL UNIQUE,"
                 "\"credential_id\" INTEGER REFERENCES \"credentials\"(\"id\") ON DELETE SET NULL,"
                 "\"os_credential_id\" INTEGER REFERENCES \"credentials\"(\"id\") ON DELETE SET NULL,"
                 "\"profile_id\" INTEGER REFERENCES \"profiles\"(\"id\") ON DELETE SET NULL,"
                 "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        LOG(ERROR) << "Unable to create local_hosts table:" << db.lastError();
        return false;
    }

    if (!db.exec("CREATE TABLE IF NOT EXISTS \"routers\" ("
                 "\"id\" INTEGER UNIQUE,"
                 "\"name\" TEXT NOT NULL DEFAULT '',"
                 "\"session_type\" INTEGER NOT NULL DEFAULT 0,"
                 "\"data\" BLOB DEFAULT X'',"
                 "\"guid\" TEXT NOT NULL UNIQUE,"
                 "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        LOG(ERROR) << "Unable to create routers table:" << db.lastError();
        return false;
    }

    if (!db.exec("CREATE TABLE IF NOT EXISTS \"router_hosts\" ("
                 "\"router_id\" INTEGER NOT NULL REFERENCES \"routers\"(\"id\") ON DELETE CASCADE,"
                 "\"host_id\" INTEGER NOT NULL,"
                 "\"check_time\" INTEGER NOT NULL DEFAULT 0,"
                 "\"data\" BLOB DEFAULT X'',"
                 "\"credential_id\" INTEGER REFERENCES \"credentials\"(\"id\") ON DELETE SET NULL,"
                 "\"os_credential_id\" INTEGER REFERENCES \"credentials\"(\"id\") ON DELETE SET NULL,"
                 "\"profile_id\" INTEGER REFERENCES \"profiles\"(\"id\") ON DELETE SET NULL,"
                 "PRIMARY KEY(\"router_id\",\"host_id\"))"))
    {
        LOG(ERROR) << "Unable to create router_hosts table:" << db.lastError();
        return false;
    }

    if (!db.exec("CREATE TABLE IF NOT EXISTS \"credentials\" ("
                 "\"id\" INTEGER UNIQUE,"
                 "\"type\" INTEGER NOT NULL DEFAULT 0,"
                 "\"name\" TEXT NOT NULL DEFAULT '',"
                 "\"data\" BLOB DEFAULT X'',"
                 "\"guid\" TEXT NOT NULL UNIQUE,"
                 "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        LOG(ERROR) << "Unable to create credentials table:" << db.lastError();
        return false;
    }

    if (!db.exec("CREATE TABLE IF NOT EXISTS \"profiles\" ("
                 "\"id\" INTEGER UNIQUE,"
                 "\"name\" TEXT NOT NULL DEFAULT '',"
                 "\"data\" BLOB DEFAULT X'',"
                 "\"guid\" TEXT NOT NULL UNIQUE,"
                 "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        LOG(ERROR) << "Unable to create profiles table:" << db.lastError();
        return false;
    }

    //==============================================================================================
    // Migration begin.
    {
        const char kOsCredentialId[] = "INTEGER REFERENCES \"credentials\"(\"id\") ON DELETE SET NULL";

        if (!addMissingColumn(db, "local_hosts", "os_credential_id", kOsCredentialId))
        {
            LOG(ERROR) << "Unable to add os_credential_id column to local_hosts table:" << db.lastError();
            return false;
        }

        if (!addMissingColumn(db, "router_hosts", "os_credential_id", kOsCredentialId))
        {
            LOG(ERROR) << "Unable to add os_credential_id column to router_hosts table:" << db.lastError();
            return false;
        }

        const char kProfileId[] = "INTEGER REFERENCES \"profiles\"(\"id\") ON DELETE SET NULL";

        if (!addMissingColumn(db, "local_hosts", "profile_id", kProfileId))
        {
            LOG(ERROR) << "Unable to add profile_id column to local_hosts table:" << db.lastError();
            return false;
        }

        if (!addMissingColumn(db, "router_hosts", "profile_id", kProfileId))
        {
            LOG(ERROR) << "Unable to add profile_id column to router_hosts table:" << db.lastError();
            return false;
        }
    }
    // Migration end.
    //==============================================================================================

    if (!db.exec("CREATE TRIGGER IF NOT EXISTS \"router_hosts_drop_empty\" "
                 "AFTER UPDATE ON \"router_hosts\" "
                 "WHEN NEW.\"data\" = X'' AND NEW.\"credential_id\" IS NULL AND "
                 "NEW.\"os_credential_id\" IS NULL AND NEW.\"profile_id\" IS NULL "
                 "BEGIN "
                 "DELETE FROM \"router_hosts\" WHERE \"router_id\" = NEW.\"router_id\" AND "
                 "\"host_id\" = NEW.\"host_id\"; "
                 "END"))
    {
        LOG(ERROR) << "Unable to create router_hosts_drop_empty trigger:" << db.lastError();
        return false;
    }

    if (!db.exec("CREATE TABLE IF NOT EXISTS \"settings\" ("
                 "\"name\" TEXT PRIMARY KEY NOT NULL,"
                 "\"value\" TEXT NOT NULL)"))
    {
        LOG(ERROR) << "Unable to create settings table:" << db.lastError();
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
    if (!g_test_file_path.isEmpty())
        return g_test_file_path;

    QString dir_path = BasePaths::appUserDataDir();
    if (dir_path.isEmpty())
        return QString();

    return dir_path + "/client.db3";
}

//--------------------------------------------------------------------------------------------------
bool Database::open(const QString& file_path)
{
    LOG(INFO) << (!QFileInfo::exists(file_path) ? "Creating" : "Opening") << "database:" << file_path;

    if (!db_.open(file_path))
        return false;

    if (!db_.exec("PRAGMA secure_delete = ON"))
        LOG(WARNING) << "Unable to enable secure_delete:" << db_.lastError();

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

    if (!createTables(db_) || !ensureDefaultProfile())
    {
        db_.close();
        return false;
    }

    if (!db_.exec("PRAGMA foreign_keys = ON"))
    {
        LOG(ERROR) << "Unable to enable foreign keys:" << db_.lastError();
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
bool Database::import(const QList<RouterConfig>& routers,
                      const QList<LocalGroupConfig>& local_groups,
                      const QList<LocalHostConfig>& local_hosts,
                      const QList<RouterHostConfig>& router_hosts,
                      const QList<CredentialConfig>& credentials,
                      const QList<ProfileConfig>& profiles)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlTransaction transaction(db_);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction";
        return false;
    }

    // What the book held is what the file replaces, and the two have nothing to do with each other.
    // Saved credentials go with their routers, and hosts are deleted on their own, because the ones
    // at the root of the book lie in no group.
    if (!db_.exec("DELETE FROM local_groups") || !db_.exec("DELETE FROM routers") ||
        !db_.exec("DELETE FROM local_hosts") || !db_.exec("DELETE FROM credentials") ||
        !db_.exec("DELETE FROM profiles"))
    {
        LOG(ERROR) << "Unable to clear the address book:" << db_.lastError();
        return false;
    }

    // The ids the keys of this call turned into. A link that is not one of the keys already names
    // what it means in the address book, so it is left as it is.
    QHash<qint64, qint64> credential_ids;
    for (const CredentialConfig& credential : credentials)
    {
        CredentialConfig config = credential;
        config.setId(-1);

        if (!addCredential(config))
            return false;

        credential_ids.insert(credential.id(), config.id());
    }

    QHash<qint64, qint64> profile_ids;
    for (const ProfileConfig& profile : profiles)
    {
        ProfileConfig config = profile;
        config.setId(-1);

        if (!addProfile(config))
            return false;

        profile_ids.insert(profile.id(), config.id());
    }

    if (!ensureDefaultProfile())
        return false;

    QHash<qint64, qint64> router_ids;
    for (const RouterConfig& router : routers)
    {
        RouterConfig config = router;
        if (!addRouter(config))
            return false;

        router_ids.insert(router.routerId(), config.routerId());
    }

    QHash<qint64, qint64> group_ids;
    for (const LocalGroupConfig& group : local_groups)
    {
        LocalGroupConfig config = group;
        config.setParentId(group_ids.value(group.parentId(), group.parentId()));

        if (!addLocalGroup(config))
            return false;

        group_ids.insert(group.id(), config.id());
    }

    for (const LocalHostConfig& host : local_hosts)
    {
        LocalHostConfig config = host;
        config.setGroupId(group_ids.value(host.groupId(), host.groupId()));
        config.setRouterId(router_ids.value(host.routerId(), host.routerId()));
        config.setCredentialId(credential_ids.value(host.credentialId(), host.credentialId()));
        config.setOsCredentialId(credential_ids.value(host.osCredentialId(), host.osCredentialId()));
        config.setProfileId(profile_ids.value(host.profileId(), host.profileId()));

        if (!config.isValid())
        {
            LOG(ERROR) << "Invalid parameters";
            return false;
        }

        std::optional<QByteArray> encrypted_data = config.encryptedData();
        if (!encrypted_data.has_value())
            return false;

        if (config.guid().isEmpty())
            config.setGuid(QUuid::createUuid().toString(QUuid::WithoutBraces));

        SqlQuery query(db_, "INSERT INTO local_hosts (id, group_id, router_id, name, comment, data, "
                            "create_time, modify_time, connect_time, guid, credential_id, os_credential_id, "
                            "profile_id) "
                            "VALUES (NULL, NULLIF(?, 0), ?, ?, ?, ?, ?, ?, ?, ?, NULLIF(?, 0), NULLIF(?, 0), "
                            "NULLIF(?, 0))");
        query.addInt64(config.groupId());
        query.addInt64(config.routerId());
        query.addText(config.name());
        query.addText(config.comment());
        query.addBlob(*encrypted_data);
        query.addInt64(config.createTime());
        query.addInt64(config.modifyTime());
        query.addInt64(config.connectTime());
        query.addText(config.guid());
        query.addInt64(config.credentialId());
        query.addInt64(config.osCredentialId());
        query.addInt64(config.profileId());

        if (!query.exec())
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return false;
        }
    }

    for (const RouterHostConfig& host : router_hosts)
    {
        RouterHostConfig config = host;
        config.setRouterId(router_ids.value(host.routerId(), host.routerId()));
        config.setCredentialId(credential_ids.value(host.credentialId(), host.credentialId()));
        config.setOsCredentialId(credential_ids.value(host.osCredentialId(), host.osCredentialId()));
        config.setProfileId(profile_ids.value(host.profileId(), host.profileId()));

        if (!addRouterHost(config))
            return false;
    }

    return transaction.commit();
}

//--------------------------------------------------------------------------------------------------
bool Database::saveCopyAndClearAll(const QString& copy_path)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    // The workers keep their own connections to the file, so it is not renamed but emptied in place.
    {
        SqlQuery copy(db_, "VACUUM INTO ?");
        copy.addText(copy_path);

        if (!copy.exec())
        {
            LOG(ERROR) << "Unable to copy the database:" << db_.lastError();
            return false;
        }
    }

    SqlTransaction transaction(db_);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction";
        return false;
    }

    if (!db_.exec("DELETE FROM local_groups") || !db_.exec("DELETE FROM routers") ||
        !db_.exec("DELETE FROM local_hosts") || !db_.exec("DELETE FROM credentials") ||
        !db_.exec("DELETE FROM profiles") || !db_.exec("DELETE FROM settings"))
    {
        LOG(ERROR) << "Unable to clear the database:" << db_.lastError();
        return false;
    }

    if (!ensureDefaultProfile() || !transaction.commit())
        return false;

    LOG(INFO) << "Database is reset, the copy is saved to:" << copy_path;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::reencryptAll(const QList<LocalHostConfig>& local_hosts,
                            const QList<RouterConfig>& routers,
                            const QList<RouterHostConfig>& router_hosts,
                            const QList<CredentialConfig>& credentials,
                            const ProxyConfig& proxy,
                            const QByteArray& salt,
                            const QByteArray& verifier,
                            quint32 version)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlTransaction transaction(db_);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction";
        return false;
    }

    // On any failure the early return skips commit(), so the transaction destructor rolls back and
    // the address book stays fully readable with the old master password.
    //
    // Only the ciphertext is written. Going through modifyHost() would stamp every host as edited
    // now, and the moment a host was last edited is a column of the list the user reads. Names and
    // comments are plain text, so a change of key leaves them alone.
    for (const LocalHostConfig& local_host : local_hosts)
    {
        std::optional<QByteArray> encrypted_data = local_host.encryptedData();
        if (!encrypted_data.has_value())
            return false;

        SqlQuery query(db_, "UPDATE local_hosts SET data=? WHERE id=?");
        query.addBlob(*encrypted_data);
        query.addInt64(local_host.id());

        if (!query.exec())
        {
            LOG(ERROR) << "Unable to re-encrypt host" << local_host.id() << ":" << db_.lastError();
            return false;
        }
    }

    for (const RouterConfig& router : routers)
    {
        if (!modifyRouter(router))
        {
            LOG(ERROR) << "Unable to re-encrypt router:" << router.routerId();
            return false;
        }
    }

    for (const RouterHostConfig& router_host : router_hosts)
    {
        if (!modifyRouterHost(router_host))
        {
            LOG(ERROR) << "Unable to re-encrypt credentials of router host:" << router_host.hostId();
            return false;
        }
    }

    for (const CredentialConfig& credential : credentials)
    {
        std::optional<QByteArray> encrypted_data = credential.encryptedData();
        if (!encrypted_data.has_value())
            return false;

        SqlQuery query(db_, "UPDATE credentials SET data=? WHERE id=?");
        query.addBlob(*encrypted_data);
        query.addInt64(credential.id());

        if (!query.exec())
        {
            LOG(ERROR) << "Unable to re-encrypt credential" << credential.id() << ":" << db_.lastError();
            return false;
        }
    }

    if (!setProxyConfig(proxy))
    {
        LOG(ERROR) << "Unable to re-encrypt proxy";
        return false;
    }

    if (!setMasterPassword(salt, verifier, version))
        return false;

    return transaction.commit();
}

//--------------------------------------------------------------------------------------------------
// static
void Database::setFilePathForTesting(const QString& file_path)
{
    g_test_file_path = file_path;
    instance().db_.close();
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

    return open(file_path);
}
