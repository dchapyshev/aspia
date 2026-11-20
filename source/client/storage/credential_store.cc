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

#include "client/storage/credential_store.h"

#include <QDateTime>
#include <QUuid>

#include "base/logging.h"
#include "base/sql/sql_query.h"
#include "client/storage/sync_store.h"

namespace {

//--------------------------------------------------------------------------------------------------
Database::ReadResult readCredential(const SqlQuery& query, CredentialConfig* credential)
{
    credential->setId(query.columnInt64(0));
    credential->setType(static_cast<CredentialConfig::Type>(query.columnInt64(1)));
    credential->setDisplayName(query.columnText(2));
    credential->setGuid(query.columnText(4));
    credential->setModifyTime(query.columnInt64(5));

    if (!credential->setEncryptedData(query.columnBlob(3)))
    {
        LOG(ERROR) << "Unable to read encrypted data of credential" << credential->id();
        return Database::ReadResult::INCOMPLETE;
    }

    return Database::ReadResult::OK;
}

} // namespace

//--------------------------------------------------------------------------------------------------
CredentialStore::CredentialStore(Database& db)
    : DatabaseStore(db)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
Database::ReadResult CredentialStore::list(
    QList<CredentialConfig>* credentials, std::optional<CredentialConfig::Type> type) const
{
    CHECK(credentials);
    credentials->clear();

    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return Database::ReadResult::FAILED;
    }

    SqlQuery query(sql(), type.has_value() ?
        "SELECT id, type, name, data, guid, modify_time FROM credentials WHERE deleted=0 AND type=? "
        "ORDER BY casefold(name)" :
        "SELECT id, type, name, data, guid, modify_time FROM credentials WHERE deleted=0 "
        "ORDER BY casefold(name)");
    if (type.has_value())
        query.addInt64(static_cast<qint64>(*type));

    Database::ReadResult result = Database::ReadResult::OK;

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            // A failed step must not pass for the end of the rows: a caller may treat what is
            // missing from the list as deleted.
            LOG(ERROR) << "Unable to execute query:" << sql().lastError();
            return Database::ReadResult::FAILED;
        }
        if (step == SqlQuery::StepResult::DONE)
            break;

        CredentialConfig credential;
        if (readCredential(query, &credential) != Database::ReadResult::OK)
            result = Database::ReadResult::INCOMPLETE;

        credentials->append(credential);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
bool CredentialStore::add(CredentialConfig& credential)
{
    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (!credential.isValid())
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    if (credential.guid().isEmpty())
        credential.setGuid(QUuid::createUuid().toString(QUuid::WithoutBraces));

    std::optional<QByteArray> encrypted_data = credential.encryptedData();
    if (!encrypted_data.has_value())
        return false;

    const qint64 modify_time = QDateTime::currentSecsSinceEpoch();

    {
        SqlQuery restore_query(sql(), "UPDATE credentials SET type=?, name=?, data=?, modify_time=?, "
                                      "sync_dirty=sync_dirty+1, deleted=0 WHERE guid=? AND deleted!=0 "
                                      "RETURNING id");
        restore_query.addInt64(static_cast<int>(credential.type()));
        restore_query.addText(credential.displayName());
        restore_query.addBlob(*encrypted_data);
        restore_query.addInt64(modify_time);
        restore_query.addText(credential.guid());

        const SqlQuery::StepResult step = restore_query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << sql().lastError();
            return false;
        }

        if (step == SqlQuery::StepResult::ROW)
        {
            credential.setId(restore_query.columnInt64(0));
            credential.setModifyTime(modify_time);

            if (restore_query.next() != SqlQuery::StepResult::DONE)
            {
                LOG(ERROR) << "Unable to execute query:" << sql().lastError();
                return false;
            }

            return true;
        }
    }

    SqlQuery query(sql(), "INSERT INTO credentials (id, type, name, data, guid, modify_time, sync_dirty) "
                          "VALUES (NULL, ?, ?, ?, ?, ?, 1)");
    query.addInt64(static_cast<int>(credential.type()));
    query.addText(credential.displayName());
    query.addBlob(*encrypted_data);
    query.addText(credential.guid());
    query.addInt64(modify_time);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    credential.setId(sql().lastInsertRowId());
    credential.setModifyTime(modify_time);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool CredentialStore::modify(const CredentialConfig& credential)
{
    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (!credential.isValid())
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    SqlTransaction transaction(sql());
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction";
        return false;
    }

    SqlQuery row(sql(), "SELECT guid, data FROM credentials WHERE id=? AND deleted=0");
    row.addInt64(credential.id());

    const SqlQuery::StepResult step = row.next();
    if (step != SqlQuery::StepResult::ROW)
    {
        if (step == SqlQuery::StepResult::FAILED)
            LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        else
            LOG(ERROR) << "Credential" << credential.id() << "not found";
        return false;
    }

    CredentialConfig stored;
    stored.setGuid(row.columnText(0));

    if (!stored.setEncryptedData(row.columnBlob(1)))
        LOG(WARNING) << "Stored data of credential" << credential.id() << "could not be read and is replaced";

    CredentialConfig sealed = credential;
    sealed.setGuid(stored.guid());
    sealed.setUnknownData(stored.unknownData());

    std::optional<QByteArray> encrypted_data = sealed.encryptedData();
    if (!encrypted_data.has_value())
        return false;

    SqlQuery query(sql(), "UPDATE credentials SET type=?, name=?, data=?, modify_time=?, "
                          "sync_dirty=sync_dirty+1 WHERE id=?");
    query.addInt64(static_cast<int>(credential.type()));
    query.addText(credential.displayName());
    query.addBlob(*encrypted_data);
    query.addInt64(QDateTime::currentSecsSinceEpoch());
    query.addInt64(credential.id());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    return transaction.commit();
}

//--------------------------------------------------------------------------------------------------
bool CredentialStore::remove(qint64 credential_id)
{
    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    SqlTransaction transaction(sql());
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction";
        return false;
    }

    SqlQuery remove_query(sql(), "DELETE FROM credentials WHERE id=? AND sync_revision=0");
    remove_query.addInt64(credential_id);

    if (!remove_query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    SqlQuery mark_query(sql(), "UPDATE credentials SET name='', data=X'', modify_time=?, "
                               "sync_dirty=sync_dirty+1, deleted=1 "
                               "WHERE id=? AND sync_revision!=0 AND deleted=0");
    mark_query.addInt64(QDateTime::currentSecsSinceEpoch());
    mark_query.addInt64(credential_id);

    if (!mark_query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    return transaction.commit();
}

//--------------------------------------------------------------------------------------------------
Database::FindResult CredentialStore::find(qint64 credential_id, CredentialConfig* credential) const
{
    CHECK(credential);
    *credential = CredentialConfig();

    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return Database::FindResult::FAILED;
    }

    SqlQuery query(sql(), "SELECT id, type, name, data, guid, modify_time FROM credentials "
                          "WHERE id=? AND deleted=0");
    query.addInt64(credential_id);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return Database::FindResult::FAILED;
    }

    if (step == SqlQuery::StepResult::DONE)
        return Database::FindResult::NOT_FOUND;

    if (readCredential(query, credential) != Database::ReadResult::OK)
    {
        LOG(ERROR) << "Unable to read the data of credentials" << credential_id;
        return Database::FindResult::UNREADABLE;
    }

    return Database::FindResult::FOUND;
}

//--------------------------------------------------------------------------------------------------
Database::FindResult CredentialStore::findByGuid(const QString& guid, CredentialConfig* credential) const
{
    CHECK(credential);
    *credential = CredentialConfig();

    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return Database::FindResult::FAILED;
    }

    // No record carries an empty identifier, so there is nothing to look for.
    if (guid.isEmpty())
        return Database::FindResult::NOT_FOUND;

    SqlQuery query(sql(), "SELECT id, type, name, data, guid, modify_time FROM credentials "
                          "WHERE guid=? AND deleted=0");
    query.addText(guid);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return Database::FindResult::FAILED;
    }

    if (step == SqlQuery::StepResult::DONE)
        return Database::FindResult::NOT_FOUND;

    if (readCredential(query, credential) != Database::ReadResult::OK)
    {
        LOG(ERROR) << "Unable to read the data of credentials with guid" << guid;
        return Database::FindResult::UNREADABLE;
    }

    return Database::FindResult::FOUND;
}

//--------------------------------------------------------------------------------------------------
Database::ReadResult CredentialStore::unsynced(QList<Unsynced>* credentials) const
{
    CHECK(credentials);
    credentials->clear();

    if (!db_.isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return Database::ReadResult::FAILED;
    }

    SqlQuery query(sql(), "SELECT id, type, name, data, guid, modify_time, sync_revision, sync_dirty, deleted "
                          "FROM credentials WHERE sync_dirty!=0");

    Database::ReadResult result = Database::ReadResult::OK;

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << sql().lastError();
            return Database::ReadResult::FAILED;
        }
        if (step == SqlQuery::StepResult::DONE)
            break;

        Unsynced& unsynced = credentials->emplaceBack();
        unsynced.deleted = query.columnInt64(8) != 0;
        unsynced.sync_revision = query.columnInt64(6);
        unsynced.sync_dirty = query.columnInt64(7);

        if (unsynced.deleted)
        {
            unsynced.credential.setId(query.columnInt64(0));
            unsynced.credential.setGuid(query.columnText(4));
            unsynced.credential.setModifyTime(query.columnInt64(5));
        }
        else if (readCredential(query, &unsynced.credential) != Database::ReadResult::OK)
        {
            result = Database::ReadResult::INCOMPLETE;
        }
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
bool CredentialStore::storeReceived(
    SyncTransaction& transaction, const CredentialConfig& credential, qint64 sync_revision)
{
    if (!transaction.isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    if (!credential.isValid() || credential.guid().isEmpty() || sync_revision <= 0)
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    std::optional<QByteArray> encrypted_data = credential.encryptedData();
    if (!encrypted_data.has_value())
        return false;

    SqlQuery query(sql(), "INSERT INTO credentials (id, type, name, data, guid, modify_time, sync_revision) "
                          "VALUES (NULL, ?, ?, ?, ?, ?, ?) "
                          "ON CONFLICT(guid) DO UPDATE SET type=excluded.type, name=excluded.name, "
                          "data=excluded.data, modify_time=excluded.modify_time, "
                          "sync_revision=excluded.sync_revision, sync_dirty=0, deleted=0 "
                          "WHERE credentials.sync_revision!=excluded.sync_revision");
    query.addInt64(static_cast<int>(credential.type()));
    query.addText(credential.displayName());
    query.addBlob(*encrypted_data);
    query.addText(credential.guid());
    query.addInt64(credential.modifyTime());
    query.addInt64(sync_revision);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool CredentialStore::setUnknownData(
    SyncTransaction& transaction, const QString& guid, const QByteArray& unknown_data)
{
    if (!transaction.isActive())
    {
        LOG(ERROR) << "The transaction is not active";
        return false;
    }

    SqlQuery row(sql(), "SELECT data FROM credentials WHERE guid=? AND deleted=0");
    row.addText(guid);

    const SqlQuery::StepResult step = row.next();
    if (step != SqlQuery::StepResult::ROW)
    {
        if (step == SqlQuery::StepResult::FAILED)
            LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        else
            LOG(ERROR) << "Credential with guid" << guid << "not found";
        return false;
    }

    CredentialConfig credential;
    credential.setGuid(guid);

    if (!credential.setEncryptedData(row.columnBlob(0)))
    {
        LOG(ERROR) << "Unable to read encrypted data of credential with guid" << guid;
        return false;
    }

    credential.setUnknownData(unknown_data);

    std::optional<QByteArray> encrypted_data = credential.encryptedData();
    if (!encrypted_data.has_value())
    {
        LOG(ERROR) << "Unable to seal credential with guid" << guid;
        return false;
    }

    SqlQuery query(sql(), "UPDATE credentials SET data=? WHERE guid=?");
    query.addBlob(*encrypted_data);
    query.addText(guid);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << sql().lastError();
        return false;
    }

    return true;
}
