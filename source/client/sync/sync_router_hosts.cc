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

#include "client/sync/sync_router_hosts.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "base/crypto/secure_memory.h"
#include "client/storage/credential_store.h"
#include "client/storage/profile_store.h"
#include "client/sync/sync_crypto.h"

namespace {

//--------------------------------------------------------------------------------------------------
std::optional<QString> credentialGuid(Database& db, qint64 credential_id)
{
    if (credential_id <= 0)
        return QString();

    QString guid;
    const Database::FindResult found = CredentialStore(db).findGuidByRowId(credential_id, &guid);
    if (found == Database::FindResult::NOT_FOUND)
        return QString();

    if (found == Database::FindResult::FAILED)
    {
        LOG(ERROR) << "Unable to read credentials" << credential_id;
        return std::nullopt;
    }

    return guid;
}

//--------------------------------------------------------------------------------------------------
std::optional<QString> profileGuid(Database& db, qint64 profile_id)
{
    if (profile_id <= 0)
        return QString();

    QString guid;
    const Database::FindResult found = ProfileStore(db).findGuidByRowId(profile_id, &guid);
    if (found == Database::FindResult::NOT_FOUND)
        return QString();

    if (found == Database::FindResult::FAILED)
    {
        LOG(ERROR) << "Unable to read profile" << profile_id;
        return std::nullopt;
    }

    return guid;
}

//--------------------------------------------------------------------------------------------------
std::optional<qint64> credentialId(Database& db, const std::string& guid)
{
    if (guid.empty())
        return 0;

    qint64 credential_id = 0;
    const Database::FindResult found = CredentialStore(db).findRowIdByGuid(QString::fromStdString(guid), &credential_id);
    if (found == Database::FindResult::NOT_FOUND)
    {
        LOG(INFO) << "Credentials with guid" << guid << "are not on the device, an empty row keeps the link";

        if (!CredentialStore(db).addEmptyRow(QString::fromStdString(guid), &credential_id))
            return std::nullopt;

        return credential_id;
    }

    if (found == Database::FindResult::FAILED)
    {
        LOG(ERROR) << "Unable to read credentials with guid" << guid;
        return std::nullopt;
    }

    return credential_id;
}

//--------------------------------------------------------------------------------------------------
std::optional<qint64> profileId(Database& db, const std::string& guid)
{
    if (guid.empty())
        return 0;

    qint64 profile_id = 0;
    const Database::FindResult found = ProfileStore(db).findRowIdByGuid(QString::fromStdString(guid), &profile_id);
    if (found == Database::FindResult::NOT_FOUND)
    {
        LOG(INFO) << "Profile with guid" << guid << "is not on the device, an empty row keeps the link";

        if (!ProfileStore(db).addEmptyRow(QString::fromStdString(guid), &profile_id))
            return std::nullopt;

        return profile_id;
    }

    if (found == Database::FindResult::FAILED)
    {
        LOG(ERROR) << "Unable to read profile with guid" << guid;
        return std::nullopt;
    }

    return profile_id;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
Database::ReadResult SyncRouterHostTraits::readUnsynced(Database& db, QList<Unsynced>* unsynced)
{
    return RouterHostStore(db).unsynced(unsynced);
}

//--------------------------------------------------------------------------------------------------
// static
std::optional<QByteArray> SyncRouterHostTraits::seal(Database& db, const SecureByteArray& key, const Config& config)
{
    const std::optional<QString> credential_guid = credentialGuid(db, config.credentialId());
    const std::optional<QString> os_credential_guid = credentialGuid(db, config.osCredentialId());
    const std::optional<QString> profile_guid = profileGuid(db, config.profileId());

    if (!credential_guid.has_value() || !os_credential_guid.has_value() || !profile_guid.has_value())
    {
        LOG(ERROR) << "Unable to read the links of router host" << config.hostId();
        return std::nullopt;
    }

    proto::router::SyncRouterHost content;
    content.set_username(config.username().toStdString());

    const SecureByteArray password = config.password().toUtf8();
    content.set_password(password.constData(), static_cast<size_t>(password.size()));

    content.set_credential_guid(credential_guid->toStdString());
    content.set_os_credential_guid(os_credential_guid->toStdString());
    content.set_profile_guid(profile_guid->toStdString());

    const QByteArray& unknown_data = config.unknownData();
    content.mutable_unknown_fields()->assign(unknown_data.constData(), static_cast<size_t>(unknown_data.size()));

    const SecureByteArray plain(serialize(content));

    memZero(content.mutable_username());
    memZero(content.mutable_password());

    return SyncCrypto::encryptRecord(key, kType, recordId(config).toStdString(), plain.toByteArray());
}

//--------------------------------------------------------------------------------------------------
// static
SyncRecords::OpenResult SyncRouterHostTraits::openReceived(
    Database& db, const SecureByteArray& key, const proto::router::SyncRecord& record, Config* config)
{
    bool ok = false;
    const HostId host_id = QString::fromStdString(record.id()).toULongLong(&ok);
    if (!ok || host_id == kInvalidHostId || isTempHostId(host_id))
    {
        LOG(ERROR) << "Invalid id of a router host record:" << record.id();
        return SyncRecords::OpenResult::UNREADABLE;
    }

    if (record.deleted())
        return SyncRecords::OpenResult::OPENED;

    std::optional<QByteArray> decrypted = SyncCrypto::decryptRecord(
        key, kType, record.id(), QByteArray::fromStdString(record.data()));
    if (!decrypted.has_value())
        return SyncRecords::OpenResult::UNREADABLE;

    const SecureByteArray plain(std::move(*decrypted));

    proto::router::SyncRouterHost content;
    if (!parse(plain.toByteArray(), &content))
        return SyncRecords::OpenResult::UNREADABLE;

    const std::optional<qint64> credential_id = credentialId(db, content.credential_guid());
    const std::optional<qint64> os_credential_id = credentialId(db, content.os_credential_guid());
    const std::optional<qint64> profile_id = profileId(db, content.profile_guid());

    const std::string& password = content.password();

    config->setHostId(host_id);
    config->setUsername(QString::fromStdString(content.username()));
    config->setPassword(
        SecureString(QString::fromUtf8(password.data(), static_cast<qsizetype>(password.size()))));
    config->setCredentialId(credential_id.value_or(0));
    config->setOsCredentialId(os_credential_id.value_or(0));
    config->setProfileId(profile_id.value_or(0));
    config->setUnknownData(QByteArray::fromStdString(content.unknown_fields()));

    memZero(content.mutable_username());
    memZero(content.mutable_password());

    if (!credential_id.has_value() || !os_credential_id.has_value() || !profile_id.has_value())
    {
        LOG(ERROR) << "Unable to read the links of received router host" << host_id;
        return SyncRecords::OpenResult::UNREADABLE;
    }

    if (!config->isValid())
        return SyncRecords::OpenResult::UNREADABLE;

    return SyncRecords::OpenResult::OPENED;
}

//--------------------------------------------------------------------------------------------------
// static
bool SyncRouterHostTraits::storeReceived(
    Database& db, SyncTransaction& transaction, Config config, qint64 sync_revision)
{
    if (config.username().isEmpty() && config.unknownData().isEmpty() && config.credentialId() <= 0 &&
        config.osCredentialId() <= 0 && config.profileId() <= 0)
    {
        LOG(INFO) << "Received router host" << config.hostId() << "holds nothing known to the device";
        return transaction.eraseRecord(kType, recordId(config));
    }

    return RouterHostStore(db).storeReceived(transaction, config, sync_revision);
}
