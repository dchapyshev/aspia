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

#include "client/sync/sync_records.h"

#include <QDateTime>
#include <QHash>

#include "base/logging.h"
#include "client/sync/sync_credentials.h"
#include "client/sync/sync_profiles.h"
#include "proto/router_constants.h"

//--------------------------------------------------------------------------------------------------
template <typename Traits>
SyncRecordsOf<Traits>::SyncRecordsOf(Database& db, const QByteArray& storage_id, const SecureByteArray& key)
    : db_(db),
      storage_id_(storage_id),
      key_(key)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
template <typename Traits>
SyncRecords::Result SyncRecordsOf<Traits>::readUnsyncedRecords(proto::router::SyncRecordRequest* request)
{
    CHECK(request);

    request->clear_record();
    sent_records_.clear();

    Database::SyncTransaction transaction(db_);
    const Database::SyncResult begun = transaction.begin(storage_id_);
    if (begun != Database::SyncResult::DONE)
        return begun == Database::SyncResult::DETACHED ? Result::DETACHED : Result::FAILED;

    QList<Unsynced> unsynced;
    if (Traits::readUnsynced(db_, &unsynced) == Database::ReadResult::FAILED)
    {
        LOG(ERROR) << "Unable to read the unsynced records of type" << Traits::kType;
        return Result::FAILED;
    }

    for (const Unsynced& item : std::as_const(unsynced))
    {
        if (sent_records_.size() >= proto::router::kMaxSyncPageSize)
            break;

        const Config& config = Traits::config(item);
        std::optional<QByteArray> data;

        if (!item.deleted)
        {
            if (!Traits::isReadable(item))
            {
                LOG(ERROR) << "Record" << config.id() << "of type" << Traits::kType
                           << "could not be read and is not sent";
                continue;
            }

            data = Traits::seal(key_, config);
            if (!data.has_value())
            {
                LOG(ERROR) << "Unable to seal record" << config.id() << "of type" << Traits::kType;
                return Result::FAILED;
            }

            if (static_cast<size_t>(data->size()) > proto::router::kMaxSyncRecordDataSize)
            {
                LOG(ERROR) << "Record" << config.id() << "of type" << Traits::kType << "is too large and is not sent";
                continue;
            }
        }

        proto::router::SyncRecord* record = request->add_record();
        record->set_revision(item.sync_revision);
        record->set_type(Traits::kType);
        record->set_id(config.guid().toStdString());
        record->set_deleted(item.deleted);

        if (data.has_value())
            record->set_data(data->toStdString());

        sent_records_.append(SentRecord{ config.guid(), item.sync_dirty });
    }

    return Result::DONE;
}

//--------------------------------------------------------------------------------------------------
template <typename Traits>
SyncRecords::Result SyncRecordsOf<Traits>::confirmSentRecords(const proto::router::SyncRecordResult& result)
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

        if (!transaction.confirmSentRecord(Traits::kType, sent_records[i].guid, revision, sent_records[i].sync_dirty))
            return Result::FAILED;

        ++stored_count;
    }

    if (stored_count != 0 && storage_revision_ >= 0 && result.revision() == storage_revision_ + stored_count)
    {
        if (!transaction.setReceivedRevision(Traits::kType, result.revision()))
            return Result::FAILED;

        LOG(INFO) << "Sent records of type" << Traits::kType << "are not to be received, revision:"
                  << result.revision();
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
template <typename Traits>
SyncRecords::Result SyncRecordsOf<Traits>::storeReceivedRecords(const proto::router::SyncRecordList& list)
{
    storage_revision_ = list.has_more() ? -1 : list.revision();

    if (list.record().empty())
        return Result::DONE;

    Database::SyncTransaction transaction(db_);
    const Database::SyncResult begun = transaction.begin(storage_id_);
    if (begun != Database::SyncResult::DONE)
        return begun == Database::SyncResult::DETACHED ? Result::DETACHED : Result::FAILED;

    QList<Unsynced> unsynced;
    if (Traits::readUnsynced(db_, &unsynced) == Database::ReadResult::FAILED)
    {
        LOG(ERROR) << "Unable to read the unsynced records of type" << Traits::kType;
        return Result::FAILED;
    }

    QHash<QString, const Unsynced*> unsynced_by_guid;
    for (const Unsynced& item : std::as_const(unsynced))
        unsynced_by_guid.insert(Traits::config(item).guid(), &item);

    const qint64 current_time = QDateTime::currentSecsSinceEpoch();
    qint64 received_revision = 0;

    for (const proto::router::SyncRecord& record : list.record())
    {
        received_revision = std::max<qint64>(received_revision, record.revision());

        if (record.type() != Traits::kType)
        {
            LOG(ERROR) << "Unexpected type of a received record:" << record.type();
            continue;
        }

        const QString guid = QString::fromStdString(record.id());
        const Unsynced* local = unsynced_by_guid.value(guid);

        if (local && local->sync_revision == record.revision())
            continue;

        const qint64 received_age = std::max<qint64>(list.current_time() - record.time(), 0);

        Config received;
        bool readable = true;
        bool received_wins = true;

        if (!Traits::openReceived(key_, record, &received))
        {
            LOG(ERROR) << "Received record" << guid << "of type" << Traits::kType << "could not be read";
            readable = false;
            received_wins = false;
        }
        else if (local)
        {
            const Config& local_config = Traits::config(*local);
            const bool same_content = record.deleted() ?
                local->deleted : !local->deleted && Traits::hasSameContent(local_config, received);
            const bool local_damaged = !local->deleted && !Traits::isReadable(*local);

            received_wins = local_damaged || same_content ||
                received_age < current_time - local_config.modifyTime();
        }

        bool stored = false;

        if (!received_wins)
        {
            stored = true;

            if (readable && !record.deleted() && local && !local->deleted && Traits::isReadable(*local))
                stored = Traits::storeUnknownData(db_, transaction, received);

            if (stored)
                stored = transaction.setRecordSyncRevision(Traits::kType, guid, record.revision());
        }
        else if (record.deleted())
        {
            stored = transaction.eraseRecord(Traits::kType, guid);
        }
        else
        {
            received.setModifyTime(current_time - received_age);
            stored = Traits::storeReceived(db_, transaction, std::move(received), record.revision());
        }

        if (!stored)
            return Result::FAILED;
    }

    if (!transaction.setReceivedRevision(Traits::kType, received_revision))
        return Result::FAILED;

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction";
        return Result::FAILED;
    }

    return Result::DONE;
}

template class SyncRecordsOf<SyncCredentialTraits>;
template class SyncRecordsOf<SyncProfileTraits>;
