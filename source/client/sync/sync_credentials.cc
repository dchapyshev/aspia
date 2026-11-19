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

#include "client/sync/sync_credentials.h"

#include <QDateTime>
#include <QHash>

#include "base/logging.h"
#include "base/serialization.h"
#include "base/crypto/secure_memory.h"
#include "client/storage/database.h"
#include "client/sync/sync_crypto.h"
#include "proto/router_constants.h"

namespace {

const proto::router::SyncDataType kType = proto::router::SYNC_DATA_TYPE_CREDENTIALS;

//--------------------------------------------------------------------------------------------------
std::optional<QByteArray> sealCredential(const SecureByteArray& key, const CredentialConfig& credential)
{
    proto::router::SyncCredential content;
    content.set_type(static_cast<quint32>(credential.type()));
    content.set_display_name(credential.displayName().toStdString());
    content.set_username(credential.username().toStdString());

    const SecureByteArray password = credential.password().toUtf8();
    content.set_password(password.constData(), static_cast<size_t>(password.size()));

    const QByteArray& unknown_data = credential.unknownData();
    content.mutable_unknown_fields()->assign(unknown_data.constData(), static_cast<size_t>(unknown_data.size()));

    const SecureByteArray plain(serialize(content));

    memZero(content.mutable_username());
    memZero(content.mutable_password());

    return SyncCrypto::encryptRecord(key, kType, credential.guid().toStdString(), plain.toByteArray());
}

//--------------------------------------------------------------------------------------------------
bool unsealCredential(
    const SecureByteArray& key, const proto::router::SyncRecord& record, CredentialConfig* credential)
{
    std::optional<QByteArray> decrypted = SyncCrypto::decryptRecord(
        key, kType, record.id(), QByteArray::fromStdString(record.data()));
    if (!decrypted.has_value())
        return false;

    const SecureByteArray plain(std::move(*decrypted));

    proto::router::SyncCredential content;
    if (!parse(plain.toByteArray(), &content))
        return false;

    const std::string& password = content.password();

    credential->setGuid(QString::fromStdString(record.id()));
    credential->setType(static_cast<CredentialConfig::Type>(content.type()));
    credential->setDisplayName(QString::fromStdString(content.display_name()));
    credential->setUsername(QString::fromStdString(content.username()));
    credential->setPassword(
        SecureString(QString::fromUtf8(password.data(), static_cast<qsizetype>(password.size()))));
    credential->setUnknownData(QByteArray::fromStdString(content.unknown_fields()));

    const bool known_type = content.type() <= static_cast<quint32>(CredentialConfig::Type::OS);

    memZero(content.mutable_username());
    memZero(content.mutable_password());

    return known_type && credential->isValid();
}

//--------------------------------------------------------------------------------------------------
bool hasSameContent(const CredentialConfig& first, const CredentialConfig& second)
{
    return first.type() == second.type() && first.displayName() == second.displayName() &&
           first.username() == second.username() && first.password() == second.password();
}

} // namespace

//--------------------------------------------------------------------------------------------------
SyncCredentials::SyncCredentials(Database& db, const QByteArray& storage_id, const SecureByteArray& key)
    : db_(db),
      storage_id_(storage_id),
      key_(key)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
SyncCredentials::Result SyncCredentials::readUnsyncedRecords(proto::router::SyncRecordRequest* request)
{
    CHECK(request);

    request->clear_record();
    sent_records_.clear();

    Database::SyncTransaction transaction(db_);
    const Database::SyncResult begun = transaction.begin(storage_id_);
    if (begun != Database::SyncResult::DONE)
        return begun == Database::SyncResult::DETACHED ? Result::DETACHED : Result::FAILED;

    QList<Database::UnsyncedCredential> unsynced;
    if (db_.unsyncedCredentials(&unsynced) == Database::ReadResult::FAILED)
    {
        LOG(ERROR) << "Unable to read the unsynced credentials";
        return Result::FAILED;
    }

    for (const Database::UnsyncedCredential& item : std::as_const(unsynced))
    {
        if (sent_records_.size() >= proto::router::kMaxSyncPageSize)
            break;

        const CredentialConfig& credential = item.credential;
        std::optional<QByteArray> data;

        if (!item.deleted)
        {
            if (!credential.isValid())
            {
                LOG(ERROR) << "Credential" << credential.id() << "could not be read and is not sent";
                continue;
            }

            data = sealCredential(key_, credential);
            if (!data.has_value())
            {
                LOG(ERROR) << "Unable to seal credential" << credential.id();
                return Result::FAILED;
            }

            if (static_cast<size_t>(data->size()) > proto::router::kMaxSyncRecordDataSize)
            {
                LOG(ERROR) << "Credential" << credential.id() << "is too large and is not sent";
                continue;
            }
        }

        proto::router::SyncRecord* record = request->add_record();
        record->set_revision(item.sync_revision);
        record->set_type(kType);
        record->set_id(credential.guid().toStdString());
        record->set_deleted(item.deleted);

        if (data.has_value())
            record->set_data(data->toStdString());

        sent_records_.append(SentRecord{ credential.guid(), item.sync_dirty });
    }

    return Result::DONE;
}

//--------------------------------------------------------------------------------------------------
SyncCredentials::Result SyncCredentials::confirmSentRecords(const proto::router::SyncRecordResult& result)
{
    const QList<SentRecord> sent_records = std::exchange(sent_records_, {});

    if (result.record_revision_size() != sent_records.size())
    {
        LOG(ERROR) << "Revisions for" << result.record_revision_size() << "records, sent" << sent_records.size();
        return Result::INVALID_ANSWER;
    }

    Database::SyncTransaction transaction(db_);
    const Database::SyncResult begun = transaction.begin(storage_id_);
    if (begun != Database::SyncResult::DONE)
        return begun == Database::SyncResult::DETACHED ? Result::DETACHED : Result::FAILED;

    qint64 stored_count = 0;

    for (int i = 0; i < sent_records.size(); ++i)
    {
        const qint64 revision = result.record_revision(i);
        if (revision == 0)
            continue;

        if (!transaction.confirmSentCredential(sent_records[i].guid, revision, sent_records[i].sync_dirty))
            return Result::FAILED;

        ++stored_count;
    }

    if (stored_count != 0 && storage_revision_ >= 0 && result.revision() == storage_revision_ + stored_count)
    {
        if (!transaction.setReceivedCredentialsRevision(result.revision()))
            return Result::FAILED;

        LOG(INFO) << "Sent records are not to be received, revision:" << result.revision();
        storage_revision_ = result.revision();
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction";
        return Result::FAILED;
    }

    return Result::DONE;
}

//--------------------------------------------------------------------------------------------------
SyncCredentials::Result SyncCredentials::storeReceivedRecords(const proto::router::SyncRecordList& list)
{
    storage_revision_ = list.has_more() ? -1 : list.revision();

    if (list.record().empty())
        return Result::DONE;

    Database::SyncTransaction transaction(db_);
    const Database::SyncResult begun = transaction.begin(storage_id_);
    if (begun != Database::SyncResult::DONE)
        return begun == Database::SyncResult::DETACHED ? Result::DETACHED : Result::FAILED;

    QList<Database::UnsyncedCredential> unsynced;
    if (db_.unsyncedCredentials(&unsynced) == Database::ReadResult::FAILED)
    {
        LOG(ERROR) << "Unable to read the unsynced credentials";
        return Result::FAILED;
    }

    QHash<QString, const Database::UnsyncedCredential*> unsynced_by_guid;
    for (const Database::UnsyncedCredential& item : std::as_const(unsynced))
        unsynced_by_guid.insert(item.credential.guid(), &item);

    const qint64 current_time = QDateTime::currentSecsSinceEpoch();
    qint64 received_revision = 0;

    for (const proto::router::SyncRecord& record : list.record())
    {
        received_revision = std::max<qint64>(received_revision, record.revision());

        if (record.type() != kType)
        {
            LOG(ERROR) << "Unexpected type of a received record:" << record.type();
            continue;
        }

        const QString guid = QString::fromStdString(record.id());
        const Database::UnsyncedCredential* local = unsynced_by_guid.value(guid);

        if (local && local->sync_revision == record.revision())
            continue;

        const qint64 received_age = std::max<qint64>(list.current_time() - record.time(), 0);

        CredentialConfig received;
        bool readable = true;
        bool received_wins = true;

        if (!record.deleted() && !unsealCredential(key_, record, &received))
        {
            LOG(ERROR) << "Received record" << guid << "could not be read";
            readable = false;
            received_wins = false;
        }
        else if (local)
        {
            const bool same_content = record.deleted() ?
                local->deleted : !local->deleted && hasSameContent(local->credential, received);
            const bool local_damaged = !local->deleted && !local->credential.isValid();

            received_wins = local_damaged || same_content ||
                received_age < current_time - local->credential.modifyTime();
        }

        bool stored = false;

        if (!received_wins)
        {
            stored = true;

            if (readable && !record.deleted() && local && !local->deleted && local->credential.isValid())
                stored = transaction.setCredentialUnknownData(guid, received.unknownData());

            if (stored)
                stored = transaction.setCredentialSyncRevision(guid, record.revision());
        }
        else if (record.deleted())
        {
            stored = transaction.eraseCredential(guid);
        }
        else
        {
            received.setModifyTime(current_time - received_age);
            stored = transaction.storeReceivedCredential(received, record.revision());
        }

        if (!stored)
            return Result::FAILED;
    }

    if (!transaction.setReceivedCredentialsRevision(received_revision))
        return Result::FAILED;

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction";
        return Result::FAILED;
    }

    return Result::DONE;
}
