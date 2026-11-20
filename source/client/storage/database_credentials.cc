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

#include <QDateTime>
#include <QUuid>

#include "base/logging.h"
#include "base/sql/sql_query.h"
#include "base/sql/sql_transaction.h"

namespace {

const char kSettingReceivedCredentialsRevision[] = "received_credentials_revision";

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
Database::ReadResult Database::credentialList(
    QList<CredentialConfig>* credentials, std::optional<CredentialConfig::Type> type) const
{
    CHECK(credentials);
    credentials->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return ReadResult::FAILED;
    }

    SqlQuery query(db_, type.has_value() ?
        "SELECT id, type, name, data, guid, modify_time FROM credentials WHERE deleted=0 AND type=? "
        "ORDER BY casefold(name)" :
        "SELECT id, type, name, data, guid, modify_time FROM credentials WHERE deleted=0 "
        "ORDER BY casefold(name)");
    if (type.has_value())
        query.addInt64(static_cast<qint64>(*type));

    ReadResult result = ReadResult::OK;

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            // A failed step must not pass for the end of the rows: a caller may treat what is
            // missing from the list as deleted.
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return ReadResult::FAILED;
        }
        if (step == SqlQuery::StepResult::DONE)
            break;

        CredentialConfig credential;
        if (readCredential(query, &credential) != ReadResult::OK)
            result = ReadResult::INCOMPLETE;

        credentials->append(credential);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
bool Database::addCredential(CredentialConfig& credential)
{
    if (!isValid())
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
        SqlQuery restore_query(db_, "UPDATE credentials SET type=?, name=?, data=?, modify_time=?, "
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
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return false;
        }

        if (step == SqlQuery::StepResult::ROW)
        {
            credential.setId(restore_query.columnInt64(0));
            credential.setModifyTime(modify_time);

            if (restore_query.next() != SqlQuery::StepResult::DONE)
            {
                LOG(ERROR) << "Unable to execute query:" << db_.lastError();
                return false;
            }

            return true;
        }
    }

    SqlQuery query(db_, "INSERT INTO credentials (id, type, name, data, guid, modify_time, sync_dirty) "
                        "VALUES (NULL, ?, ?, ?, ?, ?, 1)");
    query.addInt64(static_cast<int>(credential.type()));
    query.addText(credential.displayName());
    query.addBlob(*encrypted_data);
    query.addText(credential.guid());
    query.addInt64(modify_time);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    credential.setId(db_.lastInsertRowId());
    credential.setModifyTime(modify_time);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool Database::modifyCredential(const CredentialConfig& credential)
{
    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return false;
    }

    if (!credential.isValid())
    {
        LOG(ERROR) << "Invalid parameters";
        return false;
    }

    SqlTransaction transaction(db_);
    if (!transaction.begin(SqlTransaction::Mode::IMMEDIATE))
    {
        LOG(ERROR) << "Unable to begin transaction";
        return false;
    }

    SqlQuery row(db_, "SELECT guid, data FROM credentials WHERE id=? AND deleted=0");
    row.addInt64(credential.id());

    const SqlQuery::StepResult step = row.next();
    if (step != SqlQuery::StepResult::ROW)
    {
        if (step == SqlQuery::StepResult::FAILED)
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
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

    SqlQuery query(db_, "UPDATE credentials SET type=?, name=?, data=?, modify_time=?, "
                        "sync_dirty=sync_dirty+1 WHERE id=?");
    query.addInt64(static_cast<int>(credential.type()));
    query.addText(credential.displayName());
    query.addBlob(*encrypted_data);
    query.addInt64(QDateTime::currentSecsSinceEpoch());
    query.addInt64(credential.id());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return transaction.commit();
}

//--------------------------------------------------------------------------------------------------
bool Database::removeCredential(qint64 credential_id)
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

    SqlQuery remove_query(db_, "DELETE FROM credentials WHERE id=? AND sync_revision=0");
    remove_query.addInt64(credential_id);

    if (!remove_query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    SqlQuery mark_query(db_, "UPDATE credentials SET name='', data=X'', modify_time=?, "
                             "sync_dirty=sync_dirty+1, deleted=1 "
                             "WHERE id=? AND sync_revision!=0 AND deleted=0");
    mark_query.addInt64(QDateTime::currentSecsSinceEpoch());
    mark_query.addInt64(credential_id);

    if (!mark_query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return transaction.commit();
}

//--------------------------------------------------------------------------------------------------
Database::FindResult Database::findCredential(qint64 credential_id, CredentialConfig* credential) const
{
    CHECK(credential);
    *credential = CredentialConfig();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return FindResult::FAILED;
    }

    SqlQuery query(db_, "SELECT id, type, name, data, guid, modify_time FROM credentials "
                        "WHERE id=? AND deleted=0");
    query.addInt64(credential_id);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return FindResult::FAILED;
    }

    if (step == SqlQuery::StepResult::DONE)
        return FindResult::NOT_FOUND;

    if (readCredential(query, credential) != ReadResult::OK)
    {
        LOG(ERROR) << "Unable to read the data of credentials" << credential_id;
        return FindResult::UNREADABLE;
    }

    return FindResult::FOUND;
}

//--------------------------------------------------------------------------------------------------
Database::FindResult Database::findCredentialByGuid(const QString& guid, CredentialConfig* credential) const
{
    CHECK(credential);
    *credential = CredentialConfig();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return FindResult::FAILED;
    }

    // No record carries an empty identifier, so there is nothing to look for.
    if (guid.isEmpty())
        return FindResult::NOT_FOUND;

    SqlQuery query(db_, "SELECT id, type, name, data, guid, modify_time FROM credentials "
                        "WHERE guid=? AND deleted=0");
    query.addText(guid);

    const SqlQuery::StepResult step = query.next();
    if (step == SqlQuery::StepResult::FAILED)
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return FindResult::FAILED;
    }

    if (step == SqlQuery::StepResult::DONE)
        return FindResult::NOT_FOUND;

    if (readCredential(query, credential) != ReadResult::OK)
    {
        LOG(ERROR) << "Unable to read the data of credentials with guid" << guid;
        return FindResult::UNREADABLE;
    }

    return FindResult::FOUND;
}

//--------------------------------------------------------------------------------------------------
Database::ReadResult Database::unsyncedCredentials(QList<UnsyncedCredential>* credentials) const
{
    CHECK(credentials);
    credentials->clear();

    if (!isValid())
    {
        LOG(ERROR) << "Database is not valid";
        return ReadResult::FAILED;
    }

    SqlQuery query(db_, "SELECT id, type, name, data, guid, modify_time, sync_revision, sync_dirty, deleted "
                        "FROM credentials WHERE sync_dirty!=0");

    ReadResult result = ReadResult::OK;

    for (;;)
    {
        const SqlQuery::StepResult step = query.next();
        if (step == SqlQuery::StepResult::FAILED)
        {
            LOG(ERROR) << "Unable to execute query:" << db_.lastError();
            return ReadResult::FAILED;
        }
        if (step == SqlQuery::StepResult::DONE)
            break;

        UnsyncedCredential& unsynced = credentials->emplaceBack();
        unsynced.deleted = query.columnInt64(8) != 0;
        unsynced.sync_revision = query.columnInt64(6);
        unsynced.sync_dirty = query.columnInt64(7);

        if (unsynced.deleted)
        {
            unsynced.credential.setId(query.columnInt64(0));
            unsynced.credential.setGuid(query.columnText(4));
            unsynced.credential.setModifyTime(query.columnInt64(5));
        }
        else if (readCredential(query, &unsynced.credential) != ReadResult::OK)
        {
            result = ReadResult::INCOMPLETE;
        }
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
qint64 Database::receivedCredentialsRevision() const
{
    return readSetting(kSettingReceivedCredentialsRevision).toLongLong();
}

//--------------------------------------------------------------------------------------------------
bool Database::resetSyncState()
{
    CHECK(db_.isTransactionActive());

    if (!db_.exec("DELETE FROM credentials WHERE deleted!=0") ||
        !db_.exec("UPDATE credentials SET sync_revision=0, sync_dirty=1") ||
        !db_.exec("DELETE FROM profiles WHERE deleted!=0"))
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    SqlQuery profiles_query(db_, "UPDATE profiles SET sync_revision=0, sync_dirty=(guid<>? OR modify_time<>0)");
    profiles_query.addText(std::string_view(ProfileConfig::kDefaultGuid));

    if (!profiles_query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return setReceivedCredentialsRevision(0) && setReceivedProfilesRevision(0);
}

//--------------------------------------------------------------------------------------------------
bool Database::discardSyncData()
{
    CHECK(db_.isTransactionActive());

    if (!db_.exec("UPDATE credentials SET name='', data=X'', sync_revision=0, sync_dirty=0, deleted=1"))
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    LOG(INFO) << "Credentials discarded:" << db_.changes();

    SqlQuery profiles_query(db_, "UPDATE profiles SET name='', data=X'', sync_revision=0, sync_dirty=0, deleted=1 "
                                 "WHERE guid<>?");
    profiles_query.addText(std::string_view(ProfileConfig::kDefaultGuid));

    if (!profiles_query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    LOG(INFO) << "Profiles discarded:" << db_.changes();

    SqlQuery default_query(db_, "UPDATE profiles SET sync_revision=0, sync_dirty=0 WHERE guid=?");
    default_query.addText(std::string_view(ProfileConfig::kDefaultGuid));

    if (!default_query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return setReceivedCredentialsRevision(0) && setReceivedProfilesRevision(0);
}

//--------------------------------------------------------------------------------------------------
bool Database::setReceivedCredentialsRevision(qint64 revision)
{
    CHECK(db_.isTransactionActive());
    return writeSetting(kSettingReceivedCredentialsRevision, QString::number(revision));
}
