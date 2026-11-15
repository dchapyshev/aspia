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

//--------------------------------------------------------------------------------------------------
Database::ReadResult readCredential(const SqlQuery& query, CredentialConfig* credential)
{
    credential->setId(query.columnInt64(0));
    credential->setType(static_cast<CredentialConfig::Type>(query.columnInt64(1)));
    credential->setDisplayName(query.columnText(2));
    credential->setGuid(query.columnText(4));

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
        "SELECT id, type, name, data, guid FROM credentials WHERE type=? ORDER BY casefold(name)" :
        "SELECT id, type, name, data, guid FROM credentials ORDER BY casefold(name)");
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

    // The column is sealed for the guid, so the guid comes first.
    if (credential.guid().isEmpty())
        credential.setGuid(QUuid::createUuid().toString(QUuid::WithoutBraces));

    std::optional<QByteArray> encrypted_data = credential.encryptedData();
    if (!encrypted_data.has_value())
        return false;

    SqlQuery query(db_, "INSERT INTO credentials (id, type, name, data, guid, modify_time) "
                        "VALUES (NULL, ?, ?, ?, ?, ?)");
    query.addInt64(static_cast<int>(credential.type()));
    query.addText(credential.displayName());
    query.addBlob(*encrypted_data);
    query.addText(credential.guid());
    query.addInt64(QDateTime::currentSecsSinceEpoch());

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    credential.setId(db_.lastInsertRowId());
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

    SqlQuery row(db_, "SELECT guid FROM credentials WHERE id=?");
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

    CredentialConfig sealed = credential;
    sealed.setGuid(row.columnText(0));

    std::optional<QByteArray> encrypted_data = sealed.encryptedData();
    if (!encrypted_data.has_value())
        return false;

    SqlQuery query(db_, "UPDATE credentials SET type=?, name=?, data=?, modify_time=? WHERE id=?");
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

    SqlQuery query(db_, "DELETE FROM credentials WHERE id=?");
    query.addInt64(credential_id);

    if (!query.exec())
    {
        LOG(ERROR) << "Unable to execute query:" << db_.lastError();
        return false;
    }

    return true;
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

    SqlQuery query(db_, "SELECT id, type, name, data, guid FROM credentials WHERE id=?");
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

    SqlQuery query(db_, "SELECT id, type, name, data, guid FROM credentials WHERE guid=?");
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
