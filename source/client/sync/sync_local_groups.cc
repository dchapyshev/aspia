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

#include "client/sync/sync_local_groups.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "client/sync/sync_crypto.h"
#include "client/sync/sync_links.h"

//--------------------------------------------------------------------------------------------------
// static
Database::ReadResult SyncLocalGroupTraits::readUnsynced(Database& db, QList<Unsynced>* unsynced)
{
    return LocalGroupStore(db).unsynced(unsynced);
}

//--------------------------------------------------------------------------------------------------
// static
std::optional<QByteArray> SyncLocalGroupTraits::seal(Database& db, const SecureByteArray& key, const Config& config)
{
    const std::optional<QString> parent_guid = SyncLinks::groupGuid(db, config.parentId());
    if (!parent_guid.has_value())
    {
        LOG(ERROR) << "Unable to read the parent of group" << config.id();
        return std::nullopt;
    }

    proto::router::SyncLocalGroup content;
    content.set_parent_guid(parent_guid->toStdString());
    content.set_name(config.name().toStdString());
    content.set_comment(config.comment().toStdString());

    const QByteArray& unknown_data = config.unknownData();
    content.mutable_unknown_fields()->assign(unknown_data.constData(), static_cast<size_t>(unknown_data.size()));

    return SyncCrypto::encryptRecord(key, kType, config.guid().toStdString(), serialize(content));
}

//--------------------------------------------------------------------------------------------------
// static
SyncRecords::OpenResult SyncLocalGroupTraits::openReceived(
    Database& db, const SecureByteArray& key, const proto::router::SyncRecord& record, Config* config)
{
    if (record.deleted())
        return SyncRecords::OpenResult::OPENED;

    const std::optional<QByteArray> decrypted = SyncCrypto::decryptRecord(
        key, kType, record.id(), QByteArray::fromStdString(record.data()));
    if (!decrypted.has_value())
        return SyncRecords::OpenResult::UNREADABLE;

    proto::router::SyncLocalGroup content;
    if (!parse(*decrypted, &content))
        return SyncRecords::OpenResult::UNREADABLE;

    config->setGuid(QString::fromStdString(record.id()));
    config->setName(QString::fromStdString(content.name()));
    config->setComment(QString::fromStdString(content.comment()));
    config->setUnknownData(QByteArray::fromStdString(content.unknown_fields()));

    if (!config->isValid())
        return SyncRecords::OpenResult::UNREADABLE;

    const std::optional<qint64> parent_id = SyncLinks::groupId(db, content.parent_guid());
    if (!parent_id.has_value())
    {
        LOG(ERROR) << "Unable to read the parent of received group" << record.id();
        return SyncRecords::OpenResult::UNREADABLE;
    }

    config->setParentId(*parent_id);
    return SyncRecords::OpenResult::OPENED;
}

//--------------------------------------------------------------------------------------------------
// static
bool SyncLocalGroupTraits::storeReceived(
    Database& db, SyncTransaction& transaction, Config config, qint64 sync_revision)
{
    return LocalGroupStore(db).storeReceived(transaction, config, sync_revision);
}

//--------------------------------------------------------------------------------------------------
// static
bool SyncLocalGroupTraits::eraseReceived(Database& db, SyncTransaction& transaction, const QString& record_id)
{
    return LocalGroupStore(db).eraseReceived(transaction, record_id);
}
