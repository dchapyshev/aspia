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

#include "client/sync/sync_profiles.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "client/sync/sync_crypto.h"

namespace {

//--------------------------------------------------------------------------------------------------
proto::router::SyncProfile syncContent(const ProfileConfig& profile)
{
    proto::router::SyncProfile content;
    content.set_name(profile.name().toStdString());
    content.set_udp_methods(profile.udpMethods());
    content.mutable_desktop()->CopyFrom(profile.desktopConfig());
    content.mutable_desktop()->clear_recording_path();
    return content;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
Database::ReadResult SyncProfileTraits::readUnsynced(const Database& db, QList<Unsynced>* unsynced)
{
    return db.unsyncedProfiles(unsynced);
}

//--------------------------------------------------------------------------------------------------
// static
std::optional<QByteArray> SyncProfileTraits::seal(const SecureByteArray& key, const Config& config)
{
    proto::router::SyncProfile content = syncContent(config);

    const QByteArray& unknown_data = config.unknownData();
    content.mutable_unknown_fields()->assign(unknown_data.constData(), static_cast<size_t>(unknown_data.size()));

    return SyncCrypto::encryptRecord(key, kType, config.guid().toStdString(), serialize(content));
}

//--------------------------------------------------------------------------------------------------
// static
bool SyncProfileTraits::openReceived(
    const SecureByteArray& key, const proto::router::SyncRecord& record, Config* config)
{
    if (record.deleted())
    {
        if (record.id() != ProfileConfig::kDefaultGuid)
            return true;

        LOG(ERROR) << "The storage holds the default profile as removed";
        return false;
    }

    const std::optional<QByteArray> decrypted = SyncCrypto::decryptRecord(
        key, kType, record.id(), QByteArray::fromStdString(record.data()));
    if (!decrypted.has_value())
        return false;

    proto::router::SyncProfile content;
    if (!parse(*decrypted, &content))
        return false;

    config->setGuid(QString::fromStdString(record.id()));
    config->setName(QString::fromStdString(content.name()));
    config->setUdpMethods(content.udp_methods());
    config->setDesktopConfig(content.desktop());
    config->setUnknownData(QByteArray::fromStdString(content.unknown_fields()));

    return config->isValid();
}

//--------------------------------------------------------------------------------------------------
// static
bool SyncProfileTraits::hasSameContent(const Config& first, const Config& second)
{
    return serialize(syncContent(first)) == serialize(syncContent(second));
}

//--------------------------------------------------------------------------------------------------
// static
bool SyncProfileTraits::storeReceived(
    const Database& db, Database::SyncTransaction& transaction, Config config, qint64 sync_revision)
{
    ProfileConfig local;
    const Database::FindResult found = db.findProfileByGuid(config.guid(), &local);
    if (found == Database::FindResult::FAILED)
    {
        LOG(ERROR) << "Unable to read profile" << config.guid();
        return false;
    }

    proto::control::Config desktop_config = config.desktopConfig();

    if (found == Database::FindResult::FOUND)
        desktop_config.set_recording_path(local.desktopConfig().recording_path());
    else
        desktop_config.clear_recording_path();

    config.setDesktopConfig(desktop_config);

    return transaction.storeReceivedProfile(config, sync_revision);
}

//--------------------------------------------------------------------------------------------------
// static
bool SyncProfileTraits::storeUnknownData(Database::SyncTransaction& transaction, const Config& config)
{
    return transaction.setProfileUnknownData(config.guid(), config.unknownData());
}
