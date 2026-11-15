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

#include "client/storage/database.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QUuid>

#include "base/logging.h"
#include "base/files/base_paths.h"
#include "base/sql/sql_query.h"
#include "base/sql/sql_transaction.h"

namespace {

QString g_test_dir_path;
QString g_selected_id;

//--------------------------------------------------------------------------------------------------
QString filePathOf(const QString& id)
{
    const QString dir_path = Database::directoryPath();
    if (dir_path.isEmpty() || id.isEmpty())
        return QString();

    return dir_path + '/' + id + ".db3";
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
QString Database::directoryPath()
{
    if (!g_test_dir_path.isEmpty())
        return g_test_dir_path;

    QString dir_path = BasePaths::appUserDataDir();
    if (dir_path.isEmpty())
        return QString();

    return dir_path + '/' + kDirectoryName;
}

//--------------------------------------------------------------------------------------------------
// static
QString Database::filePath()
{
    return filePathOf(g_selected_id);
}

//--------------------------------------------------------------------------------------------------
// static
void Database::select(const QString& id)
{
    g_selected_id = id;
    instance().db_.close();
}

//--------------------------------------------------------------------------------------------------
// static
bool Database::moveToTrash(const QString& id)
{
    if (id == g_selected_id)
        select(QString());

    return QFile::moveToTrash(filePathOf(id));
}

//--------------------------------------------------------------------------------------------------
// static
bool Database::remove(const QString& id)
{
    if (id == g_selected_id)
        select(QString());

    return QFile::remove(filePathOf(id));
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
QString Database::guid() const
{
    return QFileInfo(db_.filePath()).completeBaseName();
}

//--------------------------------------------------------------------------------------------------
bool Database::import(const std::optional<RouterConfig>& router,
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

    if (!db_.exec("DELETE FROM local_groups") || !clearRouter() ||
        !db_.exec("DELETE FROM router_hosts") || !db_.exec("DELETE FROM local_hosts") ||
        !db_.exec("DELETE FROM credentials") || !db_.exec("DELETE FROM profiles"))
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

    if (router && !setRouterConfig(*router))
        return false;

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

        SqlQuery query(db_, "INSERT INTO local_hosts (id, group_id, name, comment, data, create_time, "
                            "modify_time, connect_time, guid, credential_id, os_credential_id, profile_id) "
                            "VALUES (NULL, NULLIF(?, 0), ?, ?, ?, ?, ?, ?, ?, NULLIF(?, 0), NULLIF(?, 0), "
                            "NULLIF(?, 0))");
        query.addInt64(config.groupId());
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
        config.setCredentialId(credential_ids.value(host.credentialId(), host.credentialId()));
        config.setOsCredentialId(credential_ids.value(host.osCredentialId(), host.osCredentialId()));
        config.setProfileId(profile_ids.value(host.profileId(), host.profileId()));

        if (!addRouterHost(config))
            return false;
    }

    return transaction.commit();
}

//--------------------------------------------------------------------------------------------------
bool Database::reencryptAll(const QList<LocalHostConfig>& local_hosts,
                            const std::optional<RouterConfig>& router,
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

    if (router && !setRouterConfig(*router))
    {
        LOG(ERROR) << "Unable to re-encrypt the router";
        return false;
    }

    for (const RouterHostConfig& router_host : router_hosts)
    {
        std::optional<QByteArray> encrypted_data = router_host.encryptedData();
        if (!encrypted_data.has_value())
            return false;

        SqlQuery query(db_, "UPDATE router_hosts SET data=? WHERE host_id=?");
        query.addBlob(*encrypted_data);
        query.addUInt64(router_host.hostId());

        if (!query.exec())
        {
            LOG(ERROR) << "Unable to re-encrypt credentials of router host" << router_host.hostId() << ":"
                       << db_.lastError();
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
void Database::setDirectoryPathForTesting(const QString& dir_path)
{
    g_test_dir_path = dir_path;
}

//--------------------------------------------------------------------------------------------------
// static
bool Database::createTables(SqlDatabase& db)
{
    // A group without a parent and a host without a group are the ones at the root: the root is
    // where the tree starts and not a record of its own, so there is no id for them to name.
    if (!db.exec("CREATE TABLE IF NOT EXISTS \"local_groups\" ("
                 "\"id\" INTEGER UNIQUE,"
                 "\"parent_id\" INTEGER REFERENCES \"local_groups\"(\"id\") ON DELETE CASCADE,"
                 "\"name\" TEXT NOT NULL DEFAULT '',"
                 "\"comment\" TEXT NOT NULL DEFAULT '',"
                 "\"guid\" TEXT NOT NULL UNIQUE,"
                 "\"modify_time\" INTEGER NOT NULL DEFAULT 0,"
                 "\"sync_revision\" INTEGER NOT NULL DEFAULT 0,"
                 "\"sync_dirty\" INTEGER NOT NULL DEFAULT 0,"
                 "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        LOG(ERROR) << "Unable to create local_groups table:" << db.lastError();
        return false;
    }

    if (!db.exec("CREATE TABLE IF NOT EXISTS \"local_hosts\" ("
                 "\"id\" INTEGER UNIQUE,"
                 "\"group_id\" INTEGER REFERENCES \"local_groups\"(\"id\") ON DELETE SET NULL,"
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
                 "\"sync_revision\" INTEGER NOT NULL DEFAULT 0,"
                 "\"sync_dirty\" INTEGER NOT NULL DEFAULT 0,"
                 "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        LOG(ERROR) << "Unable to create local_hosts table:" << db.lastError();
        return false;
    }

    if (!db.exec("CREATE TABLE IF NOT EXISTS \"router_hosts\" ("
                 "\"host_id\" INTEGER PRIMARY KEY,"
                 "\"check_time\" INTEGER NOT NULL DEFAULT 0,"
                 "\"data\" BLOB DEFAULT X'',"
                 "\"credential_id\" INTEGER REFERENCES \"credentials\"(\"id\") ON DELETE SET NULL,"
                 "\"os_credential_id\" INTEGER REFERENCES \"credentials\"(\"id\") ON DELETE SET NULL,"
                 "\"profile_id\" INTEGER REFERENCES \"profiles\"(\"id\") ON DELETE SET NULL,"
                 "\"modify_time\" INTEGER NOT NULL DEFAULT 0,"
                 "\"sync_revision\" INTEGER NOT NULL DEFAULT 0,"
                 "\"sync_dirty\" INTEGER NOT NULL DEFAULT 0)"))
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
                 "\"modify_time\" INTEGER NOT NULL DEFAULT 0,"
                 "\"sync_revision\" INTEGER NOT NULL DEFAULT 0,"
                 "\"sync_dirty\" INTEGER NOT NULL DEFAULT 0,"
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
                 "\"modify_time\" INTEGER NOT NULL DEFAULT 0,"
                 "\"sync_revision\" INTEGER NOT NULL DEFAULT 0,"
                 "\"sync_dirty\" INTEGER NOT NULL DEFAULT 0,"
                 "PRIMARY KEY(\"id\" AUTOINCREMENT))"))
    {
        LOG(ERROR) << "Unable to create profiles table:" << db.lastError();
        return false;
    }

    if (!db.exec("CREATE TRIGGER IF NOT EXISTS \"router_hosts_drop_empty\" "
                 "AFTER UPDATE ON \"router_hosts\" "
                 "WHEN NEW.\"data\" = X'' AND NEW.\"credential_id\" IS NULL AND "
                 "NEW.\"os_credential_id\" IS NULL AND NEW.\"profile_id\" IS NULL "
                 "BEGIN "
                 "DELETE FROM \"router_hosts\" WHERE \"host_id\" = NEW.\"host_id\"; "
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

//--------------------------------------------------------------------------------------------------
bool Database::openDatabase()
{
    QString file_path = filePath();
    if (file_path.isEmpty())
    {
        LOG(ERROR) << "Invalid file path";
        return false;
    }

    QString dir_path = QFileInfo(file_path).path();

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

    return open(file_path);
}
