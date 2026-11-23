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

#include <QHash>

#include "base/logging.h"
#include "base/crypto/generic_hash.h"
#include "client/sync/sync_credentials.h"
#include "client/sync/sync_local_groups.h"
#include "client/sync/sync_local_hosts.h"
#include "client/sync/sync_profiles.h"
#include "client/sync/sync_router_hosts.h"
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
SyncRecords::SyncResult SyncRecordsOf<Traits>::readUnsyncedRecords(proto::router::SyncRecordRequest* request)
{
    CHECK(request);

    static const qint64 kUnknownRevision = -1;

    request->clear_record();
    sent_records_.clear();

    SyncTransaction transaction(db_);
    const Database::SyncResult begun = transaction.begin(storage_id_);
    if (begun != Database::SyncResult::DONE)
        return begun == Database::SyncResult::DETACHED ? SyncResult::DETACHED : SyncResult::FAILED;

    QList<Unsynced> unsynced;
    if (Traits::readUnsynced(db_, &unsynced) == Database::ReadResult::FAILED)
    {
        LOG(ERROR) << "Unable to read the unsynced records of type" << Traits::kType;
        return SyncResult::FAILED;
    }

    for (const Unsynced& item : std::as_const(unsynced))
    {
        if (sent_records_.size() >= proto::router::kMaxSyncPageSize)
            break;

        const Config& config = Traits::config(item);
        const QString record_id = Traits::recordId(config);
        std::optional<QByteArray> data;

        if (!item.deleted)
        {
            if (!Traits::isReadable(item))
            {
                LOG(ERROR) << "Record" << record_id << "of type" << Traits::kType
                           << "could not be read and is not sent";
                continue;
            }

            data = Traits::seal(db_, key_, config);
            if (!data.has_value())
            {
                LOG(ERROR) << "Unable to seal record" << record_id << "of type" << Traits::kType;
                return SyncResult::FAILED;
            }

            if (static_cast<size_t>(data->size()) > proto::router::kMaxSyncRecordDataSize)
            {
                LOG(ERROR) << "Record" << record_id << "of type" << Traits::kType << "is too large and is not sent";
                continue;
            }
        }

        proto::router::SyncRecord* record = request->add_record();
        record->set_revision(std::max<qint64>(item.sync_revision, 0));
        record->set_type(Traits::kType);
        record->set_id(record_id.toStdString());
        record->set_deleted(item.deleted);

        if (data.has_value())
            record->set_data(data->toStdString());

        const QByteArray data_hash = GenericHash::hash(GenericHash::SHA256, data.value_or(QByteArray()));

        sent_records_.append(SentRecord{ record_id, data_hash, item.sync_dirty });

        if (!transaction.storeSentRecord(Traits::kType, record_id, data_hash, item.sync_dirty))
            return SyncResult::FAILED;

        if (item.sync_revision == 0)
        {
            if (!transaction.setRecordSyncRevision(Traits::kType, record_id, kUnknownRevision))
                return SyncResult::FAILED;
        }
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction";
        return SyncResult::FAILED;
    }

    return SyncResult::DONE;
}

//--------------------------------------------------------------------------------------------------
template <typename Traits>
SyncRecords::SyncResult SyncRecordsOf<Traits>::confirmSentRecords(const proto::router::SyncRecordResult& result)
{
    const QList<SentRecord> sent_records = std::exchange(sent_records_, {});

    if (result.record_revision_size() != sent_records.size())
    {
        LOG(ERROR) << "Revisions for" << result.record_revision_size() << "records, sent" << sent_records.size();
        return SyncResult::INVALID_ANSWER;
    }

    SyncTransaction transaction(db_);
    const Database::SyncResult begun = transaction.begin(storage_id_);
    if (begun != Database::SyncResult::DONE)
        return begun == Database::SyncResult::DETACHED ? SyncResult::DETACHED : SyncResult::FAILED;

    qint64 stored_count = 0;

    for (int i = 0; i < sent_records.size(); ++i)
    {
        const qint64 revision = result.record_revision(i);
        if (revision == 0)
        {
            if (!transaction.eraseSentRecord(Traits::kType, sent_records[i].record_id, sent_records[i].data_hash))
                return SyncResult::FAILED;

            continue;
        }

        if (!transaction.eraseAllSentRecords(Traits::kType, sent_records[i].record_id))
            return SyncResult::FAILED;

        if (!transaction.confirmSentRecord(
                Traits::kType, sent_records[i].record_id, revision, sent_records[i].sync_dirty))
        {
            return SyncResult::FAILED;
        }

        ++stored_count;
    }

    if (stored_count != 0 && storage_revision_ >= 0 && result.revision() == storage_revision_ + stored_count)
    {
        if (!transaction.setReceivedRevision(Traits::kType, result.revision()))
            return SyncResult::FAILED;

        LOG(INFO) << "Sent records of type" << Traits::kType << "are not to be received, revision:"
                  << result.revision();
        storage_revision_ = result.revision();
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction";
        return SyncResult::FAILED;
    }

    return SyncResult::DONE;
}

//--------------------------------------------------------------------------------------------------
template <typename Traits>
SyncRecords::SyncResult SyncRecordsOf<Traits>::discardSentRecords()
{
    const QList<SentRecord> sent_records = std::exchange(sent_records_, {});

    SyncTransaction transaction(db_);
    const Database::SyncResult begun = transaction.begin(storage_id_);
    if (begun != Database::SyncResult::DONE)
        return begun == Database::SyncResult::DETACHED ? SyncResult::DETACHED : SyncResult::FAILED;

    for (const SentRecord& sent_record : sent_records)
    {
        if (!transaction.eraseSentRecord(Traits::kType, sent_record.record_id, sent_record.data_hash))
            return SyncResult::FAILED;
    }

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction";
        return SyncResult::FAILED;
    }

    return SyncResult::DONE;
}

//--------------------------------------------------------------------------------------------------
template <typename Traits>
SyncRecords::SyncResult SyncRecordsOf<Traits>::storeReceivedRecords(const proto::router::SyncRecordList& list)
{
    storage_revision_ = list.has_more() ? -1 : list.revision();

    if (list.record().empty())
        return SyncResult::DONE;

    SyncTransaction transaction(db_);
    const Database::SyncResult begun = transaction.begin(storage_id_);
    if (begun != Database::SyncResult::DONE)
        return begun == Database::SyncResult::DETACHED ? SyncResult::DETACHED : SyncResult::FAILED;

    QList<Unsynced> unsynced;
    if (Traits::readUnsynced(db_, &unsynced) == Database::ReadResult::FAILED)
    {
        LOG(ERROR) << "Unable to read the unsynced records of type" << Traits::kType;
        return SyncResult::FAILED;
    }

    QHash<QString, const Unsynced*> unsynced_by_id;
    for (const Unsynced& item : std::as_const(unsynced))
        unsynced_by_id.insert(Traits::recordId(Traits::config(item)), &item);

    qint64 received_revision = 0;

    for (const proto::router::SyncRecord& record : list.record())
    {
        received_revision = std::max<qint64>(received_revision, record.revision());

        if (record.type() != Traits::kType)
        {
            LOG(ERROR) << "Unexpected type of a received record:" << record.type();
            continue;
        }

        const QString record_id = QString::fromStdString(record.id());
        const Unsynced* local = unsynced_by_id.value(record_id);

        if (local && local->sync_revision == record.revision())
            continue;

        if (local)
        {
            const QByteArray data_hash = GenericHash::hash(GenericHash::SHA256, std::string_view(record.data()));
            qint64 sent_dirty = 0;

            const Database::FindResult found =
                transaction.findSentRecord(Traits::kType, record_id, data_hash, &sent_dirty);
            if (found == Database::FindResult::FAILED)
                return SyncResult::FAILED;

            if (!transaction.eraseAllSentRecords(Traits::kType, record_id))
                return SyncResult::FAILED;

            if (found == Database::FindResult::FOUND)
            {
                LOG(INFO) << "Record" << record_id << "of type" << Traits::kType
                          << "is the one this device sent, revision:" << record.revision();

                if (!transaction.confirmSentRecord(Traits::kType, record_id, record.revision(), sent_dirty))
                    return SyncResult::FAILED;

                continue;
            }
        }

        Config received;
        bool stored = false;

        if (Traits::openReceived(db_, key_, record, &received) == OpenResult::UNREADABLE)
        {
            LOG(ERROR) << "Received record" << record_id << "of type" << Traits::kType << "could not be read";
            stored = transaction.setRecordSyncRevision(Traits::kType, record_id, record.revision());
        }
        else
        {
            if (local)
            {
                LOG(INFO) << "Local change of record" << record_id << "of type" << Traits::kType
                          << "gives way to revision" << record.revision() << "of the router";
            }

            if (record.deleted())
            {
                stored = Traits::eraseReceived(db_, transaction, record_id);
            }
            else
            {
                received.setModifyTime(record.time());
                stored = Traits::storeReceived(db_, transaction, std::move(received), record.revision());
            }
        }

        if (!stored)
            return SyncResult::FAILED;
    }

    if (!transaction.setReceivedRevision(Traits::kType, received_revision))
        return SyncResult::FAILED;

    if (!transaction.commit())
    {
        LOG(ERROR) << "Unable to commit transaction";
        return SyncResult::FAILED;
    }

    return SyncResult::DONE;
}

template class SyncRecordsOf<SyncCredentialTraits>;
template class SyncRecordsOf<SyncProfileTraits>;
template class SyncRecordsOf<SyncRouterHostTraits>;
template class SyncRecordsOf<SyncLocalGroupTraits>;
template class SyncRecordsOf<SyncLocalHostTraits>;
