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

#include "base/serialization.h"
#include "base/crypto/secure_memory.h"
#include "client/sync/sync_crypto.h"

//--------------------------------------------------------------------------------------------------
// static
Database::ReadResult SyncCredentialTraits::readUnsynced(Database& db, QList<Unsynced>* unsynced)
{
    return CredentialStore(db).unsynced(unsynced);
}

//--------------------------------------------------------------------------------------------------
// static
std::optional<QByteArray> SyncCredentialTraits::seal(
    Database& /* db */, const SecureByteArray& key, const Config& config)
{
    proto::router::SyncCredential content;
    content.set_type(static_cast<quint32>(config.type()));
    content.set_display_name(config.displayName().toStdString());
    content.set_username(config.username().toStdString());

    const SecureByteArray password = config.password().toUtf8();
    content.set_password(password.constData(), static_cast<size_t>(password.size()));

    const QByteArray& unknown_data = config.unknownData();
    content.mutable_unknown_fields()->assign(unknown_data.constData(), static_cast<size_t>(unknown_data.size()));

    const SecureByteArray plain(serialize(content));

    memZero(content.mutable_username());
    memZero(content.mutable_password());

    return SyncCrypto::encryptRecord(key, kType, config.guid().toStdString(), plain.toByteArray());
}

//--------------------------------------------------------------------------------------------------
// static
SyncRecords::OpenResult SyncCredentialTraits::openReceived(
    Database& /* db */, const SecureByteArray& key, const proto::router::SyncRecord& record, Config* config)
{
    if (record.deleted())
        return SyncRecords::OpenResult::OPENED;

    std::optional<QByteArray> decrypted = SyncCrypto::decryptRecord(
        key, kType, record.id(), QByteArray::fromStdString(record.data()));
    if (!decrypted.has_value())
        return SyncRecords::OpenResult::UNREADABLE;

    const SecureByteArray plain(std::move(*decrypted));

    proto::router::SyncCredential content;
    if (!parse(plain.toByteArray(), &content))
        return SyncRecords::OpenResult::UNREADABLE;

    const std::string& password = content.password();

    config->setGuid(QString::fromStdString(record.id()));
    config->setType(static_cast<CredentialConfig::Type>(content.type()));
    config->setDisplayName(QString::fromStdString(content.display_name()));
    config->setUsername(QString::fromStdString(content.username()));
    config->setPassword(
        SecureString(QString::fromUtf8(password.data(), static_cast<qsizetype>(password.size()))));
    config->setUnknownData(QByteArray::fromStdString(content.unknown_fields()));

    const bool known_type = content.type() <= static_cast<quint32>(CredentialConfig::Type::OS);

    memZero(content.mutable_username());
    memZero(content.mutable_password());

    if (!known_type || !config->isValid())
        return SyncRecords::OpenResult::UNREADABLE;

    return SyncRecords::OpenResult::OPENED;
}

//--------------------------------------------------------------------------------------------------
// static
bool SyncCredentialTraits::storeReceived(
    Database& db, SyncTransaction& transaction, Config config, qint64 sync_revision)
{
    return CredentialStore(db).storeReceived(transaction, config, sync_revision);
}

//--------------------------------------------------------------------------------------------------
// static
bool SyncCredentialTraits::eraseReceived(Database& /* db */, SyncTransaction& transaction, const QString& record_id)
{
    return transaction.eraseRecord(kType, record_id);
}
